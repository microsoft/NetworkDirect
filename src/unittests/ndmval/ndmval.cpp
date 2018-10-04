// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmval.cpp - NetworkDirect memory validation test
//
// This test establishes a connection, sends 1024 bytes from the client, and receives with a 1023-byte on the server side
//
// Client operation:
//  1. Establish connection
//  2. Allocate 1024 bytes of buffer
//  3. Send 1024 bytes to server
//  4. Expect ND_SUCCESS on iWARP or ND_REMOTE_ERROR on IB
// Server operation:
//  1. Establish connection
//  2. Allocate 1023 bytes of buffer
//  3. Try to receive from client
//  4. Expect ND_BUFFER_OVERFLOW

// to get STATUS_DATA_ERROR
#include <ntstatus.h>
// define WIN32_NO_STATUS so that STATUS_* types are not defined again
#define WIN32_NO_STATUS
#include "ndcommon.h"
#include <logging.h>
#include <ndtestutil.h>

const USHORT x_DefaultPort = 54331;
const SIZE_T x_InsufficientXfer = (1023u);
const SIZE_T x_SufficientXfer = (1024u);

const LPCWSTR TESTNAME = L"ndmval.exe";

void ShowUsage()
{
    printf("ndmval [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-l <logFile>  - Log output to a file named <logFile>\n"
        "<ip>            - IPv4 Address\n"
        "<port>          - Port number, (default: %hu)\n",
        x_DefaultPort
    );
}

class NdMValClient : public NdTestClientBase
{
public:
    ~NdMValClient()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }
    }

    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nMaxSge)
    {
        UNREFERENCED_PARAMETER(queueDepth);
        UNREFERENCED_PARAMETER(nMaxSge);

        NdTestBase::Init(v4Src);
        NdTestBase::CreateMR();

        // Allocate and register the data buffer.
        m_pBuf = HeapAlloc(GetProcessHeap(), 0, x_SufficientXfer);
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }
        NdTestBase::RegisterDataBuffer(m_pBuf, x_SufficientXfer,
            ND_MR_FLAG_ALLOW_LOCAL_WRITE, ND_SUCCESS, "Register memory failed");

        ND2_ADAPTER_INFO adapterInfo;
        NdTestBase::GetAdapterInfo(&adapterInfo);

        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(adapterInfo.MaxReceiveQueueDepth, 1);

        NdTestClientBase::Connect(v4Src, v4Dst, 0, 0);
        NdTestClientBase::CompleteConnect();

        printf("Connected.\n");

        ND2_SGE sge;
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_SufficientXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();

        NdTestBase::Send(&sge, 1, 0);

        ND2_RESULT ndRes;
        NdTestBase::WaitForCompletion(&ndRes, false);


        // We expect status to be ND_SUCCESS (for iWARP) or STATUS_DATA_ERROR(for IB)
        printf("Got send completion, status: %08x\n", ndRes.Status);
        if (ndRes.Status != ND_SUCCESS)
        {
            switch (ndRes.Status)
            {
            case STATUS_DATA_ERROR:
                break;
            case STATUS_BUFFER_OVERFLOW:
            case STATUS_DATA_OVERRUN:
                LOG_FAILURE_HRESULT(ndRes.Status, L"Warning: INDEndpoint::Send expecting ND_SUCCESS or STATUS_DATA_ERROR because receiver doesn't have enough buffer, but got errors %08x", __LINE__);
                break;
            default:
                LOG_FAILURE_HRESULT_AND_EXIT(ndRes.Status, L"INDEndpoint::Send expecting ND_SUCCESS or STATUS_DATA_ERROR because receiver doesn't have enough buffer, but got errors %08x", __LINE__);
                break;
            }
        }

        printf("Test complete.\n");

        //tear down
        NdTestBase::Shutdown();
    }

private:
    void *m_pBuf = nullptr;
};

class NdMValServer : public NdTestServerBase
{
public:
    ~NdMValServer()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }
    }

    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge)
    {
        UNREFERENCED_PARAMETER(queueDepth);
        UNREFERENCED_PARAMETER(nSge);

        NdMValServer::Init(v4Src);
        NdTestBase::CreateMR();

        // Allocate and register the data buffer.
        m_pBuf = HeapAlloc(GetProcessHeap(), 0, x_InsufficientXfer);
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }
        NdTestBase::RegisterDataBuffer(m_pBuf, x_InsufficientXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);
        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(adapterInfo.MaxReceiveQueueDepth, 1);
        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();

        // pre-post receive with 1023 bytes
        ND2_SGE sge;
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_InsufficientXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1);

        NdTestServerBase::Accept(0, 0);
        printf("Connected.\n");

        printf("Read data from client with insufficient buffer\n");
        ND2_RESULT ndRes;
        WaitForCompletion(&ndRes, false);

        if (ndRes.Status != ND_DATA_OVERRUN)
        {
            if (ndRes.Status == ND_SUCCESS)
            {
                LOG_FAILURE_AND_EXIT(
                    L"INDEndpoint::Recv expected to fail with ND_DATA_OVERRUN, but got ND_SUCCESS",
                    __LINE__
                );
            }
            // If we get the wrong error code, treat this as a warning
            LOG_FAILURE_HRESULT(
                ndRes.Status,
                L"Warning: INDEndpoint::Recv expected to fail with ND_DATA_OVERRUN, but got %08x",
                __LINE__
            );
        }
        printf("Test complete.\n");
    }

private:
    void *m_pBuf = nullptr;
};

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    bool bServer = false;
    bool bClient = false;
    struct sockaddr_in v4Server = { 0 };

    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        printf("Failed to initialize Windows Sockets: %d\n", ret);
        exit(__LINE__);
    }

    INIT_LOG(TESTNAME);
    for (int i = 1; i < argc; i++)
    {
        TCHAR *arg = argv[i];
        if ((wcscmp(arg, L"-s") == 0) || (wcscmp(arg, L"-S") == 0))
        {
            bServer = true;
        }
        else if ((wcscmp(arg, L"-c") == 0) || (wcscmp(arg, L"-C") == 0))
        {
            bClient = true;
        }
        else if ((wcscmp(arg, L"-l") == 0) || (wcscmp(arg, L"--logFile") == 0))
        {
            RedirectLogsToFile(argv[++i]);
        }
        else if ((wcscmp(arg, L"-h") == 0) || (wcscmp(arg, L"--help") == 0))
        {
            ShowUsage();
            exit(0);
        }
    }

    // ip address is last parameter
    int len = sizeof(v4Server);
    WSAStringToAddress(argv[argc - 1], AF_INET, nullptr,
        reinterpret_cast<struct sockaddr*>(&v4Server), &len);

    if ((bClient && bServer) || (!bClient && !bServer))
    {
        printf("Exactly one of client (c or "
            "server (s) must be specified.\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (v4Server.sin_addr.s_addr == 0)
    {
        printf("Bad address.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (v4Server.sin_port == 0)
    {
        v4Server.sin_port = htons(x_DefaultPort);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdStartup failed with %08x", __LINE__);
    }

    Timer timer;
    timer.Start();
    if (bServer)
    {
        NdMValServer server;
        server.RunTest(v4Server, 0, 0);
    }
    else
    {
        struct sockaddr_in v4Src;
        SIZE_T len = sizeof(v4Src);
        HRESULT hr = NdResolveAddress(
            (const struct sockaddr*)&v4Server,
            sizeof(v4Server),
            (struct sockaddr*)&v4Src,
            &len);
        if (FAILED(hr))
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdResolveAddress failed with %08x", __LINE__);
        }

        NdMValClient client;
        client.RunTest(v4Src, v4Server, 0, 0);
    }
    timer.End();

    printf("Elapsed time %f seconds\n", timer.Report() / 1000000);
    hr = NdCleanup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdCleanup failed with %08x", __LINE__);
    }

    END_LOG(TESTNAME);
    _fcloseall();
    WSACleanup();
    return 0;
}
