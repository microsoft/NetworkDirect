// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndpingpong.cpp - NetworkDirect send/recv pingpong test
//

#include "ndcommon.h"
#include "ndtestutil.h"
#include "logging.h"
#include <functional>

const USHORT x_DefaultPort = 54325;
const DWORD x_MaxXfer = (4 * 1024 * 1024);
const DWORD x_HdrLen = 40;
const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
const DWORD x_MaxIterations = 100000;
const LPCWSTR TESTNAME = L"ndpingpong.exe";

void ShowUsage()
{
    printf("ndpingpong [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-b            - Blocking I/O (wait for CQ notification)\n"
        "\t-p            - Polling I/O (poll on the CQ) (default)\n"
        "\t-n <nSge>     - Number of scatter/gather entries per transfer (default: 1)\n"
        "\t-q <pipeline> - Pipeline limit of <pipeline> requests\n"
        "\t-l <logFile>  - Log output to a file named <logFile>\n"
        "<ip>            - IPv4 Address\n"
        "<port>          - Port number, (default: %hu)\n",
        x_DefaultPort
    );
}

class NdPingPongServer : public NdTestServerBase
{
public:

    NdPingPongServer(char *pBuf, bool useEvents) :
        m_pBuf(pBuf),
        m_bUseEvents(useEvents)
    {}

    ~NdPingPongServer()
    {
        if (m_sendSgl != nullptr)
        {
            delete[] m_sendSgl;
        }

        if (m_recvSgl != nullptr)
        {
            delete[] m_recvSgl;
        }
    }

    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge)
    {
        NdPingPongServer::Init(v4Src);
        NdTestBase::CreateMR();
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        m_queueDepth = min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth);
        m_queueDepth = (queueDepth != 0) ? min(queueDepth, m_queueDepth) : m_queueDepth;
        m_inlineThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(m_queueDepth, nSge, m_inlineThreshold);

        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();

        m_sendSgl = new (std::nothrow) ND2_SGE[nSge];
        m_recvSgl = new (std::nothrow) ND2_SGE[nSge];
        if (m_sendSgl == nullptr || m_recvSgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        m_nMaxSge = nSge;

        // prepare recv sge's and post recvs
        m_nRecvSge = NdTestBase::PrepareSge(m_recvSgl, nSge,
            m_pBuf, x_MaxXfer, x_HdrLen, m_pMr->GetLocalToken());
        for (size_t i = 0; i < m_queueDepth; i++)
        {
            NdTestBase::PostReceive(m_recvSgl, m_nRecvSge, &m_bRecvCompleted);
        }

        NdTestServerBase::Accept(0, 0);


        // warmup iterations
        Pong(1000, x_HdrLen);

        for (DWORD szXfer = 1; szXfer <= x_MaxXfer; szXfer <<= 1)
        {
            DWORD iterations = x_MaxIterations;
            if (iterations > (x_MaxVolume / szXfer))
            {
                iterations = x_MaxVolume / szXfer;
            }

            Pong(iterations, szXfer);
        }

        //tear down
        NdTestBase::Shutdown();
    }

    void Pong(DWORD nIters, DWORD len)
    {
        // prepare send sge
        DWORD nSendSge = NdTestBase::PrepareSge(m_sendSgl, m_nMaxSge,
            m_pBuf, len, x_HdrLen, m_pMr->GetLocalToken());
        DWORD txFlags = len < m_inlineThreshold ? ND_OP_FLAG_INLINE : 0;

        DWORD nResults = 0;
        bool bCancelled = false;
        const std::function<void(ND2_RESULT *)> processCompletionFn = [&bCancelled](ND2_RESULT *pComp)
        {
            if (pComp->Status == ND_SUCCESS)
            {
                *(reinterpret_cast<bool *>(pComp->RequestContext)) = true;
            }
            else if (pComp->Status == ND_CANCELED)
            {
                bCancelled = true;
            }
            else
            {
                LOG_FAILURE_HRESULT_AND_EXIT(pComp->Status, L"unexpected event: %08x\n", __LINE__);
            }
        };

        for (DWORD i = 0; i < nIters; i++)
        {
            // wait for recv
            while (!m_bRecvCompleted && !bCancelled)
            {
                WaitForCompletion(processCompletionFn, m_bUseEvents);
            }
            m_bRecvCompleted = false;
            NdTestBase::PostReceive(m_recvSgl, m_nRecvSge, &m_bRecvCompleted);

            // send pong and wait for send completion
            NdTestBase::Send(m_sendSgl, nSendSge, txFlags, &m_bSendCompleted);
            while (!m_bSendCompleted && !bCancelled)
            {
                WaitForCompletion(processCompletionFn, m_bUseEvents);
            }
            m_bSendCompleted = false;
        }
    }

private:
    char *m_pBuf = nullptr;
    ND2_SGE* m_sendSgl = nullptr;
    ND2_SGE* m_recvSgl = nullptr;
    DWORD m_nMaxSge = 0, m_nRecvSge = 0, m_queueDepth = 0, m_inlineThreshold = 0;
    bool m_bUseEvents = false;
    bool m_bSendCompleted = false;
    bool m_bRecvCompleted = false;
};

class NdPingPongClient : public NdTestClientBase
{
public:
    NdPingPongClient(char *pBuf, bool bUseEvents) :
        m_pBuf(pBuf),
        m_bUseEvents(bUseEvents)
    {}

    ~NdPingPongClient()
    {
        if (m_sendSgl != nullptr)
        {
            delete[] m_sendSgl;
        }

        if (m_recvSgl != nullptr)
        {
            delete[] m_recvSgl;
        }
    }

    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nMaxSge)
    {
        NdTestBase::Init(v4Src);

        NdTestBase::CreateMR();
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen,
            ND_MR_FLAG_ALLOW_LOCAL_WRITE, ND_SUCCESS, "Register memory failed");

        ND2_ADAPTER_INFO adapterInfo;
        NdTestBase::GetAdapterInfo(&adapterInfo);

        m_queueDepth = min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxInitiatorQueueDepth);
        m_queueDepth = (queueDepth != 0) ? min(queueDepth, m_queueDepth) : m_queueDepth;
        m_inlineThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(m_queueDepth, nMaxSge, m_inlineThreshold);

        NdTestClientBase::Connect(v4Src, v4Dst, 0, 0);
        NdTestClientBase::CompleteConnect();

        m_sendSgl = new (std::nothrow) ND2_SGE[nMaxSge];
        m_recvSgl = new (std::nothrow) ND2_SGE[nMaxSge];
        if (m_sendSgl == nullptr || m_recvSgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate failed to allocate sgl.", __LINE__);
        }

        m_nMaxSge = nMaxSge;

        // prepare recv sge's and post recvs
        m_nRecvSge = NdTestBase::PrepareSge(m_recvSgl, nMaxSge,
            m_pBuf, x_MaxXfer, x_HdrLen, m_pMr->GetLocalToken());
        for (size_t i = 0; i < m_queueDepth; i++)
        {
            NdTestBase::PostReceive(m_recvSgl, m_nRecvSge, &m_bRecvCompleted);
        }

        printf("Using %u processors. Sender Frequency is %I64d\n\n"
            " %9s %9s %9s %7s %11s\n",
            CpuMonitor::CpuCount(),
            Timer::Frequency(),
            "Size", "Iter", "Latency", "CPU", "Bytes/Sec");

        // warmup iterations
        Ping(1000, x_HdrLen);
        Sleep(1000);

        for (ULONG szXfer = 1; szXfer <= x_MaxXfer; szXfer <<= 1)
        {
            ULONG iterations = x_MaxIterations;
            if (iterations > (x_MaxVolume / szXfer))
            {
                iterations = x_MaxVolume / szXfer;
            }

            m_Cpu.Start();
            m_Timer.Start();

            Ping(iterations, szXfer);

            m_Timer.End();
            m_Cpu.End();

            // Factor of 2 to account for ping *and* pong.
            double bytesSec = 2.0 * szXfer * iterations / (m_Timer.Report() / 1000000.0);
            // Factor of 2 to account for half-round trip latency.
            double latency = (m_Timer.Report() / iterations) / 2.0;
            printf(" %9ul %9ul %9.2f %7.2f %11.0f\n",
                szXfer,
                iterations,
                latency,
                m_Cpu.Report(),
                bytesSec);
        }

        //tear down
        NdTestBase::Shutdown();
    }

    void Ping(DWORD nIters, DWORD len)
    {
        // prepare send sge
        DWORD nSendSge = NdTestBase::PrepareSge(m_sendSgl, m_nMaxSge,
            m_pBuf, len, x_HdrLen, m_pMr->GetLocalToken());
        DWORD txFlags = len < m_inlineThreshold ? ND_OP_FLAG_INLINE : 0;

        DWORD nResults = 0;
        bool bCancelled = false;
        const std::function<void(ND2_RESULT *)> processCompletionFn = [&bCancelled](ND2_RESULT *pComp)
        {
            if (pComp->Status == ND_SUCCESS)
            {
                *(reinterpret_cast<bool *>(pComp->RequestContext)) = true;
            }
            else if (pComp->Status == ND_CANCELED)
            {
                bCancelled = true;
            }
            else
            {
                LOG_FAILURE_HRESULT_AND_EXIT(pComp->Status, L"unexpected event: %08x\n", __LINE__);
            }
        };

        for (DWORD i = 0; i < nIters; i++)
        {
            // send ping and wait for completion
            NdTestBase::Send(m_sendSgl, nSendSge, txFlags, &m_bSendCompleted);
            while (!m_bSendCompleted && !bCancelled)
            {
                WaitForCompletion(processCompletionFn, m_bUseEvents);
            }
            m_bSendCompleted = false;

            // recv pong and repost
            while (!m_bRecvCompleted && !bCancelled)
            {
                WaitForCompletion(processCompletionFn, m_bUseEvents);
            }
            m_bRecvCompleted = false;
            NdTestBase::PostReceive(m_recvSgl, m_nRecvSge, &m_bRecvCompleted);
        }
    }

private:
    char *m_pBuf = nullptr;
    DWORD m_queueDepth = 0, m_inlineThreshold = 0;
    ND2_SGE* m_sendSgl = nullptr;
    ND2_SGE* m_recvSgl = nullptr;
    DWORD m_nMaxSge = 0, m_nRecvSge = 0;
    bool m_bUseEvents = false;
    bool m_bSendCompleted = false;
    bool m_bRecvCompleted = false;

    Timer m_Timer;
    CpuMonitor m_Cpu;
};

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    bool bServer = false;
    bool bClient = false;
    DWORD nSge = 1;
    LONG queueDepth = 64;
    bool bPolling = false;
    bool bBlocking = false;
    struct sockaddr_in v4Server = { 0 };

    INIT_LOG(TESTNAME);
    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        printf("Failed to initialize Windows Sockets: %d\n", ret);
        exit(__LINE__);
    }

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
        else if ((wcscmp(arg, L"-p") == 0) || (wcscmp(arg, L"-P") == 0))
        {
            bPolling = true;
        }
        else if ((wcscmp(arg, L"-b") == 0) || (wcscmp(arg, L"-B") == 0))
        {
            bBlocking = true;
        }
        else if ((wcscmp(arg, L"-n") == 0) || (wcscmp(arg, L"-N") == 0))
        {
            if (i == argc - 2)
            {
                ShowUsage();
                exit(-1);
            }
            nSge = _ttol(argv[++i]);
        }
        else if ((wcscmp(arg, L"-q") == 0) || (wcscmp(arg, L"-Q") == 0))
        {
            if (i == argc - 2)
            {
                ShowUsage();
                exit(-1);
            }
            queueDepth = _ttol(argv[++i]);
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

    // ip address is the last parameter
    int len = sizeof(v4Server);
    WSAStringToAddress(
        argv[argc - 1], AF_INET, nullptr,
        reinterpret_cast<struct sockaddr*>(&v4Server), &len);

    if ((bClient && bServer) || (!bClient && !bServer))
    {
        printf("Exactly one of client (c or "
            "server (s) must be specified.\n\n");
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

    if ((bPolling && bBlocking))
    {
        printf("Exactly one of blocking (b or polling (p) must be specified.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (nSge == 0)
    {
        printf("Invalid or missing SGE length\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdStartup failed with %08x", __LINE__);
    }

    char *pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer + x_HdrLen));
    if (!pBuf)
    {
        LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
    }

    if (bServer)
    {
#pragma warning (suppress: 6001) // no need to initialize pBuf
        NdPingPongServer server(pBuf, bBlocking);
        server.RunTest(v4Server, 0, nSge);
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
            HeapFree(GetProcessHeap(), 0, pBuf);
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdResolveAddress failed with %08x", __LINE__);
        }
#pragma warning (suppress: 6001) // no need to initialize pBuf
        NdPingPongClient client(pBuf, bBlocking);
        client.RunTest(v4Src, v4Server, 0, nSge);
    }

    HeapFree(GetProcessHeap(), 0, pBuf);
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
