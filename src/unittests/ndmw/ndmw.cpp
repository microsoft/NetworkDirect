// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmw.cpp - NetworkDirect memory window test
//
// This test establishes a connection, binds a memory window to a large
// (64MB) buffer, and performs RDMA operations on the memory window
// checking buffer contents, at various offset in the advertised region.
//
// Client operation:
//  1. Establish connection
//  2. Bind memory window
//  3. Format upper 32MB of buffer
//  4. Send 1024 bytes of buffer at 32MB offset and MW descriptor
//  5. Wait for receive
//  7. Validate buffer contents - lower 32MB should be bitwise NOT of upper 32MB.
//  8. Send 0 byte message to start second test.
//  9. Wait for receive
//  10. Validate buffer contents - lower 32MB should be reverse order (in 4-byte
//     chunks) of upper 32MB.
//  11. Disconnect
//
// Server operation:
//  1. Establish connection
//  2. Receive MW descriptor and 1024 bytes of data.
//  3. RDMA Read at offset 32MB for 1024 bytes.
//  4. Check that 1024 bytes received match 1024 bytes read.
//  5. RDMA Read at offset 32MB + 1024 bytes for 32MB - 1024 bytes.
//  6. Reverse all byte values in upper 32 MB.
//  7. Write to offset 16MB buffer contents from 48-64 MB
//  8. Write to offset 0 buffer contents from 32-48 MB
//  9. Send 0-byte message indicating first test complete.
//  10. Wait for receive.
//  11. RDMA write 4 bytes at a time, writing bottom up to offset 32MB-64MB
//      from from 64MB-32MB (yes, lots of writes).
//  12. Send 0-byte message indicating second test complete.
//  13. Disconnect

#include "ndcommon.h"
#include <logging.h>
#include <ndtestutil.h>

#define RECV_CTXT ((void *) 0x1000)
#define READ_CTXT ((void *) 0x2000)
#define SEND_CTXT ((void *) 0x3000)
#define WRITE_CTXT ((void *) 0x4000)

struct PrivateConnData
{
    uint8_t key[36];
};

const PrivateConnData x_DefaultData = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35
};

struct PeerInfo
{
    uint32_t m_remoteToken;
    uint64_t m_remoteAddress;
};

const USHORT x_DefaultPort = 54323;
const SIZE_T x_MaxXfer = (64 * 1024 * 1024);
const SIZE_T x_PiggyBackXfer = 1024;

const SIZE_T x_ReadBase = (32 * 1024 * 1024);
const SIZE_T x_BigChunkSize = (16 * 1024 * 1024);
const SIZE_T x_SmallChunkSize = 4;

const LPCWSTR TESTNAME = L"ndmw.exe";

void ShowUsage()
{
    printf("ndmw [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-l <logFile>  - Log output to a file named <logFile>\n"
        "<ip>            - IPv4 Address\n"
        "<port>          - Port number, (default: %hu)\n",
        x_DefaultPort
    );
}

class NdMWServer : public NdTestServerBase
{
public:
    void RunTest(const struct sockaddr_in& v4Src, DWORD /*queueDepth*/, DWORD /*nSge */)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth), 1);

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }

        ULONG flags =
            ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer, flags);

        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();

        ULONG pDataLen = 0;
        if (m_pConnector->GetPrivateData(nullptr, &pDataLen) != ND_BUFFER_OVERFLOW)
        {
            LOG_FAILURE_AND_EXIT(L"GetPrivateData failed\n", __LINE__);
        }

        m_pTmpBuf = static_cast<char *>(malloc(pDataLen));
        if (m_pTmpBuf == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate memory\n", __LINE__);
        }

        HRESULT hr = m_pConnector->GetPrivateData(m_pTmpBuf, &pDataLen);
        if (ND_SUCCESS != hr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to GetPrivateData\n", __LINE__);
        }

        // verify private data
        struct PrivateConnData *pData = reinterpret_cast<struct PrivateConnData *>(m_pTmpBuf);
#pragma warning (suppress: 6385) // CodeAnalysis complains about length of buffer being
        // compared is more than pDataLen, but we check for that already
        if (pData == nullptr || pDataLen < sizeof(x_DefaultData)  ||
            memcmp(pData, &x_DefaultData, sizeof(x_DefaultData)) != 0)
        {
            LOG_FAILURE_AND_EXIT(L"PrivateData not as expected.", __LINE__);
        }

        // post reveive for the terminate message
        ND2_SGE sge = { 0 };
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_PiggyBackXfer + sizeof(struct PeerInfo);
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestServerBase::Accept(1, 1, &x_DefaultData, sizeof(x_DefaultData));
        printf("Connected.\n");

        // Test 1.
        printf("Phase 1: Large read/write test with data validation.\n");

        // wait for incoming receive
        WaitForCompletionAndCheckContext(RECV_CTXT);
        struct PeerInfo *peerInfo = reinterpret_cast<struct PeerInfo *>(m_pBuf + x_PiggyBackXfer);

        // Issue RDMA Read for the first KB.
        sge.Buffer = m_pBuf + x_ReadBase;
        sge.BufferLength = x_PiggyBackXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();

        NdTestBase::Read(&sge, 1, peerInfo->m_remoteAddress + x_ReadBase, peerInfo->m_remoteToken, 0, READ_CTXT);
        WaitForCompletionAndCheckContext(READ_CTXT);

        // Compare the 1KB we read with the 1KB we received.
        if (memcmp(m_pBuf, m_pBuf + x_ReadBase, x_PiggyBackXfer) != 0)
        {
            LOG_FAILURE_AND_EXIT(L"Recv/Read buffer mismatch.", __LINE__);
        }

        // Read the rest of the 32MB.
        sge.Buffer = m_pBuf + x_ReadBase + x_PiggyBackXfer;
        sge.BufferLength = x_ReadBase - x_PiggyBackXfer;

        NdTestBase::Read(&sge, 1, peerInfo->m_remoteAddress + x_ReadBase + x_PiggyBackXfer, peerInfo->m_remoteToken, 0, READ_CTXT);
        WaitForCompletionAndCheckContext(READ_CTXT);

        // Reverse all byte values in upper 32 MB.
        for (SIZE_T i = x_ReadBase; i < x_MaxXfer; i++)
        {
            m_pBuf[i] = ~m_pBuf[i];
        }

        // Write the upper 16MB of the lower 32MB at the target.
        sge.Buffer = m_pBuf + x_ReadBase + x_BigChunkSize;
        sge.BufferLength = x_BigChunkSize;
        NdTestBase::Write(&sge, 1, peerInfo->m_remoteAddress + x_BigChunkSize, peerInfo->m_remoteToken, 0, WRITE_CTXT);
        WaitForCompletionAndCheckContext(WRITE_CTXT);

        // Write the lower 16MB at the target.
        sge.Buffer = m_pBuf + x_ReadBase;
        sge.BufferLength = x_BigChunkSize;
        NdTestBase::Write(&sge, 1, peerInfo->m_remoteAddress, peerInfo->m_remoteToken, 0, WRITE_CTXT);
        WaitForCompletionAndCheckContext(WRITE_CTXT);

        // prepost receive to detect test2 start
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        // Send 0 byte message to indicate test 1 completion.
        NdTestBase::Send(nullptr, 0, 0);
        WaitForCompletion();

        // wait to receive test2 start signal
        WaitForCompletionAndCheckContext(RECV_CTXT);

        printf("Phase 2: Small write test with data validation.\n");

        // Write 4-byte chunks, reversing the order of the chunks.
        for (SIZE_T i = 0; i < x_ReadBase; i += x_SmallChunkSize)
        {
            // Write the 4-byte chunk.
            sge.Buffer = m_pBuf + x_ReadBase + i;
            sge.BufferLength = x_SmallChunkSize;
            NdTestBase::Write(&sge, 1, peerInfo->m_remoteAddress + x_MaxXfer - x_SmallChunkSize - i, peerInfo->m_remoteToken, 0, WRITE_CTXT);
            if (i % (x_ReadBase / 100) == 0)
            {
                printf("%Id%%\r", i / (x_ReadBase / 100));
            }
            WaitForCompletionAndCheckContext(WRITE_CTXT);
        }

        // prepost receive to termination
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        // Send 0 byte message to indicate test2 completion.
        NdTestBase::Send(nullptr, 0, 0);
        WaitForCompletion();

        WaitForCompletion();
        printf("Test complete.\n");

        //tear down
        NdTestBase::Shutdown();
    }

    ~NdMWServer()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }

        if (m_pTmpBuf != nullptr)
        {
            free(m_pTmpBuf);
        }
    }

private:
    char *m_pBuf = nullptr;
    char *m_pTmpBuf = nullptr;
};

class NdMWClient : public NdTestClientBase
{
public:

    ~NdMWClient()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }

        if (m_pTmpBuf != nullptr)
        {
            free(m_pTmpBuf);
        }
    }

    void RunTest(const struct sockaddr_in& v4Src, const struct sockaddr_in& v4Dst, DWORD /*queueDepth*/, DWORD /*nSge*/)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth), 1);

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer, ND_MR_FLAG_RDMA_READ_SINK | ND_MR_FLAG_ALLOW_LOCAL_WRITE);

        // Pre-post receive request for detecting test1 completion.
        ND2_SGE sge;
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestClientBase::Connect(v4Src, v4Dst, 1, 1, &x_DefaultData, sizeof(x_DefaultData));

        ULONG pDataLen = 0;
        if (m_pConnector->GetPrivateData(nullptr, &pDataLen) != ND_BUFFER_OVERFLOW)
        {
            LOG_FAILURE_AND_EXIT(L"GetPrivateData failed\n", __LINE__);
        }

        m_pTmpBuf = static_cast<char *>(malloc(pDataLen));
        if (m_pTmpBuf == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate memory\n", __LINE__);
        }

        HRESULT hr = m_pConnector->GetPrivateData(m_pTmpBuf, &pDataLen);
        if (ND_SUCCESS != hr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to GetPrivateData\n", __LINE__);
        }

        // verify private data
        struct PrivateConnData *pData = reinterpret_cast<struct PrivateConnData *>(m_pTmpBuf);
#pragma warning (suppress: 6385) // CodeAnalysis complains about length of buffer being
        // compared is more than pDataLen, but we check for that already
        if (pData == nullptr || pDataLen < sizeof(x_DefaultData) ||
            memcmp(pData, &x_DefaultData, sizeof(x_DefaultData)) != 0)
        {
            LOG_FAILURE_AND_EXIT(L"PrivateData not as expected.", __LINE__);
        }

        NdTestClientBase::CompleteConnect();
        printf("Connected.\n");

        NdTestBase::CreateMW();
        NdTestBase::Bind(m_pBuf, x_MaxXfer, ND_OP_FLAG_ALLOW_READ | ND_OP_FLAG_ALLOW_WRITE);

        // prepare peerinfo
        struct PeerInfo *peerInfo;
        peerInfo = reinterpret_cast<struct PeerInfo *>(m_pBuf);
        peerInfo->m_remoteAddress = reinterpret_cast<uint64_t>(m_pBuf);
        peerInfo->m_remoteToken = m_pMw->GetRemoteToken();

        ND2_SGE sgl[2];
        sgl[0].Buffer = m_pBuf + x_ReadBase;
        sgl[0].BufferLength = x_PiggyBackXfer;
        sgl[0].MemoryRegionToken = m_pMr->GetLocalToken();

        sgl[1].Buffer = peerInfo;
        sgl[1].BufferLength = sizeof(PeerInfo);
        sgl[1].MemoryRegionToken = m_pMr->GetLocalToken();

        NdTestBase::Send(&sgl[0], 2, 0, SEND_CTXT);
        WaitForCompletionAndCheckContext(SEND_CTXT);

        // wait for test1 completion signal
        WaitForCompletionAndCheckContext(RECV_CTXT);

        // Validate the buffer
        for (SIZE_T i = 0; i < x_ReadBase; i++)
        {
            if (m_pBuf[i] != ~m_pBuf[i + x_ReadBase])
            {
                LOG_FAILURE_HRESULT_AND_EXIT((LONG)i, L"Buffer validation failed at index %d", __LINE__);
            }
        }

        printf("Phase 1: complete\n");

        // Pre-post receive request for detecting test1 completion.
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        // signal start of second test.
        NdTestBase::Send(nullptr, 0, 0);
        WaitForCompletion();

        // wait for test2 completion signal
        WaitForCompletionAndCheckContext(RECV_CTXT);

        // Validate buffer again.
        const UINT32* pLow = reinterpret_cast<const UINT32*>(m_pBuf);
        const UINT32* pHigh = reinterpret_cast<const UINT32*>(m_pBuf + x_MaxXfer) - 1;
        while (pLow < pHigh)
        {
            if (*pLow != *pHigh)
            {
                LOG_FAILURE_HRESULT_AND_EXIT(
                    (LONG)((ULONG_PTR)pLow - (ULONG_PTR)m_pBuf) / sizeof(UINT32),
                    L"Buffer validation failed at index %d",
                    __LINE__
                );
            }
            pLow++;
            pHigh--;
        }

        printf("Phase 2: complete\n");

        // signal test completion
        NdTestBase::Send(nullptr, 0, 0);
        WaitForCompletion();

        NdTestBase::Shutdown();
    }
private:
    char *m_pBuf = nullptr;
    char *m_pTmpBuf = nullptr;
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
        NdMWServer server;
        server.RunTest(v4Server, 0, 0);
    }
    else
    {
        struct sockaddr_in v4Src;
        SIZE_T len = sizeof(v4Src);
        HRESULT hr = NdResolveAddress((const struct sockaddr*)&v4Server,
            sizeof(v4Server), (struct sockaddr*)&v4Src, &len);
        if (FAILED(hr))
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdResolveAddress failed with %08x", __LINE__);
        }

        NdMWClient client;
        client.RunTest(v4Src, v4Server, 0, 0);
    }
    timer.End();

    printf("Elapsed time %f seconds\n", timer.Report() / 1000000.0);
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
