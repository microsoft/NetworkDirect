//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndrpingpong.cpp - NetworkDirect bidirectional RDMA write ping-pong test
//

#include "ndcommon.h"
#include "ndtestutil.h"
#include <logging.h>

const USHORT x_DefaultPort = 54327;
const SIZE_T x_MaxXfer = (4 * 1024 * 1024);
const SIZE_T x_HdrLen = 40;
const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
const SIZE_T x_MaxIterations = 100000;

const LPCWSTR TESTNAME = L"ndrpingpong.exe";

#define RECV_CTXT ((void *) 0x1000)
#define SEND_CTXT ((void *) 0x2000)
#define READ_CTXT ((void *) 0x3000)
#define WRITE_CTXT ((void *) 0x4000)

#define CLIENT_WRMUP_VAL ('A')
#define SERVER_WRMUP_VAL ('B')

#define CLIENT_TEST_VAL ('X')
#define SERVER_TEST_VAL ('Y')

void ShowUsage()
{
    printf("nrdpingpong [options] <ip>[:<port>]\n"
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

struct PeerInfo
{
    UINT32 m_remoteToken;
    UINT64 m_remoteAddress;
};

class NdrPingPongServer : public NdTestServerBase
{
public:
    NdrPingPongServer(bool blocking) :
        m_blocking(blocking)
    {
    }

    void DoPongs(ULONG szXfer, ULONG iters, bool isWarmup)
    {
        char clientVal = isWarmup ? CLIENT_WRMUP_VAL : CLIENT_TEST_VAL;
        char serverVal = isWarmup ? SERVER_WRMUP_VAL : SERVER_TEST_VAL;

        DWORD nSge = NdTestBase::PrepareSge(m_sgl, m_nMaxSge, m_pBuf,
            szXfer, x_HdrLen, m_pMr->GetLocalToken());
        DWORD flags = szXfer < m_inlineThreshold ? ND_OP_FLAG_INLINE : 0;

        bool bCancelled = false;
        while (iters > 0 && !bCancelled)
        {
            // wait until incoming RMA
            while ((m_pBuf[szXfer - 1]) != clientVal);

            // reset contents and send back
            m_pBuf[szXfer - 1] = serverVal;
            NdTestBase::Write(m_sgl, nSge, m_remoteAddress, m_remoteToken, flags, WRITE_CTXT);
            iters--;

            WaitForCompletion([&](ND2_RESULT *pCompletion)
            {
                switch (pCompletion->Status)
                {
                case ND_CANCELED:
                    bCancelled = true;
                    break;

                case ND_SUCCESS:
                    if (pCompletion->RequestContext == WRITE_CTXT)
                    {
                        break;
                    }
                    else if (pCompletion->RequestContext == RECV_CTXT)
                    {
                        // we may get incoming TERM message before write completion
                        m_termReceived = true;
                        break;
                    }
                    __fallthrough;

                default:
                    LOG_FAILURE_AND_EXIT(L"unexpected completion\n", __LINE__);
                }
            }, m_blocking);
        }
    }

    void RunTest(const struct sockaddr_in& v4Src, DWORD queueDepth, DWORD nSge)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        // Make sure adapter supports in-order RDMA
        if ((adapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0)
        {
            LOG_FAILURE_AND_EXIT(L"Adapter does not support in-order RDMA.", __LINE__);
        }

        m_queueDepth = (queueDepth > 0) ?
            min(queueDepth, adapterInfo.MaxCompletionQueueDepth) :
            adapterInfo.MaxCompletionQueueDepth;
        m_queueDepth = min(m_queueDepth, adapterInfo.MaxInitiatorQueueDepth);
        m_nMaxSge = min(nSge, adapterInfo.MaxInitiatorSge);
        m_inlineThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(m_queueDepth, adapterInfo.MaxReceiveQueueDepth),
            nSge, m_inlineThreshold);

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo)));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }

#pragma warning(suppress: 6387) // m_pBuf is already nullptr checked
        memset(m_pBuf, 0, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo));
        m_sgl = new (std::nothrow) ND2_SGE[m_nMaxSge];
        if (m_sgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        ULONG flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo), flags);

        // post receive for peerInfo and terminate messages
        ND2_SGE sge = { 0 };
        sge.Buffer = m_pBuf + x_MaxXfer + x_HdrLen;
        sge.BufferLength = sizeof(PeerInfo);
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();
        NdTestServerBase::Accept(0, 0);

        // wait for incoming peer info message
        WaitForCompletionAndCheckContext(RECV_CTXT);

        NdTestBase::CreateMW();
        NdTestBase::Bind(m_pBuf, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo), ND_OP_FLAG_ALLOW_WRITE);

        // send remote token and address
        PeerInfo *pInfo = reinterpret_cast<PeerInfo *> (m_pBuf + x_MaxXfer + x_HdrLen + sizeof(PeerInfo));
        pInfo->m_remoteToken = m_pMw->GetRemoteToken();
        pInfo->m_remoteAddress = reinterpret_cast<UINT64>(m_pBuf);
        sge.Buffer = pInfo;
        sge.BufferLength = sizeof(*pInfo);
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::Send(&sge, 1, 0, SEND_CTXT);
        WaitForCompletionAndCheckContext(SEND_CTXT);

        PeerInfo *pPeerInfo = reinterpret_cast<PeerInfo *>(m_pBuf + x_MaxXfer + x_HdrLen);
        m_remoteAddress = pPeerInfo->m_remoteAddress;
        m_remoteToken = pPeerInfo->m_remoteToken;

        // warmup
        DoPongs(x_HdrLen, 1000, true);

        for (ULONG szXfer = 1; szXfer <= x_MaxXfer; szXfer <<= 1)
        {

            ULONG iters = x_MaxIterations;
            if (iters > (x_MaxVolume / szXfer))
            {
                iters = x_MaxVolume / szXfer;
            }

            DoPongs(szXfer, iters, false);
        }

        if (!m_termReceived)
        {
            WaitForCompletionAndCheckContext(RECV_CTXT);
        }

        //tear down
        NdTestBase::Shutdown();
    }

    ~NdrPingPongServer()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }

        if (m_sgl != nullptr)
        {
            delete[] m_sgl;
        }
    }

private:
    char *m_pBuf = nullptr;
    bool m_blocking = false;
    bool m_termReceived = true;
    ULONG m_queueDepth = 0;
    ULONG m_inlineThreshold = 0;
    ULONG m_nMaxSge = 0;
    UINT64 m_remoteAddress = 0;
    UINT32 m_remoteToken = 0;
    ND2_SGE *m_sgl = nullptr;
};

class NdrPingPongClient : public NdTestClientBase
{
public:
    NdrPingPongClient(bool bUseBlocking) :
        m_bUseBlocking(bUseBlocking)
    {}

    ~NdrPingPongClient()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }

        if (m_Sgl != nullptr)
        {
            delete[] m_Sgl;
        }
    }

    void DoPings(ULONG szXfer, ULONG iters, bool isWarmup)
    {
        char clientVal = isWarmup ? CLIENT_WRMUP_VAL : CLIENT_TEST_VAL;
        char serverVal = isWarmup ? SERVER_WRMUP_VAL : SERVER_TEST_VAL;

        DWORD flags = szXfer < m_inlineThreshold ? ND_OP_FLAG_INLINE : 0;
        bool doPongs = true;
        DWORD nSge = NdTestBase::PrepareSge(m_Sgl, m_nMaxSge, m_pBuf,
            szXfer, x_HdrLen, m_pMr->GetLocalToken());

        while (iters > 0)
        {
            // set contents and issue rdma
            m_pBuf[szXfer - 1] = clientVal;
            NdTestBase::Write(m_Sgl, nSge, m_remoteAddress, m_remoteToken, flags, WRITE_CTXT);

            // wait until incoming RMA
            while ((m_pBuf[szXfer - 1]) != serverVal);
            WaitForCompletion();
            iters--;
        }
    }

    void RunTest(
        const struct sockaddr_in& v4Src,
        const struct sockaddr_in& v4Dst,
        DWORD queueDepth,
        DWORD nSge)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        // Make sure adapter supports in-order RDMA
        if ((adapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0)
        {
            LOG_FAILURE_AND_EXIT(L"Adapter does not support in-order RDMA.", __LINE__);
        }

        m_queueDepth = (queueDepth > 0) ?
            min(queueDepth, adapterInfo.MaxCompletionQueueDepth) :
            adapterInfo.MaxCompletionQueueDepth;
        m_queueDepth = min(m_queueDepth, adapterInfo.MaxInitiatorQueueDepth);
        m_nMaxSge = min(nSge, adapterInfo.MaxInitiatorSge);
        m_inlineThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo)));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }

        m_Sgl = new (std::nothrow) ND2_SGE[m_nMaxSge];
        if (m_Sgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        ULONG flags = ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo), flags);

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(m_queueDepth, adapterInfo.MaxReceiveQueueDepth),
            nSge, m_inlineThreshold);

        // post reveive for peerInfo message
        ND2_SGE sge = { 0 };
        sge.Buffer = m_pBuf + x_MaxXfer + x_HdrLen;
        sge.BufferLength = sizeof(PeerInfo);
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestClientBase::Connect(v4Src, v4Dst, 0, 0);
        NdTestClientBase::CompleteConnect();

        NdTestBase::CreateMW();
        NdTestBase::Bind(m_pBuf, x_MaxXfer + x_HdrLen + 2 * sizeof(PeerInfo), ND_OP_FLAG_ALLOW_WRITE);

        // send remote token and address
        PeerInfo *pInfo = reinterpret_cast<PeerInfo *> (m_pBuf + x_MaxXfer + x_HdrLen + sizeof(PeerInfo));
        pInfo->m_remoteToken = m_pMw->GetRemoteToken();
        pInfo->m_remoteAddress = reinterpret_cast<UINT64>(m_pBuf);
        sge.Buffer = pInfo;
        sge.BufferLength = sizeof(*pInfo);
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::Send(&sge, 1, 0, SEND_CTXT);

        // wait for send completion and incoming peer info message
        bool gotSendCompletion = false, gotPeerInfoMsg = false;
        while (!gotSendCompletion || !gotPeerInfoMsg)
        {
            WaitForCompletion([&gotSendCompletion, &gotPeerInfoMsg](ND2_RESULT *pCompletion)
            {
                if (pCompletion->RequestContext == SEND_CTXT)
                {
                    gotSendCompletion = true;
                }
                else if (pCompletion->RequestContext == RECV_CTXT)
                {
                    gotPeerInfoMsg = true;
                }
                else
                {
                    LOG_FAILURE_AND_EXIT(L"unexpected completion event\n", __LINE__);
                }
            }, true);
        }

        pInfo = reinterpret_cast<PeerInfo *>(m_pBuf + x_MaxXfer + x_HdrLen);
        m_remoteToken = pInfo->m_remoteToken;
        m_remoteAddress = pInfo->m_remoteAddress;

        printf("Using %u processors. Sender Frequency is %I64d\n\n"
            " %9s %9s %9s %7s %11s\n",
            CpuMonitor::CpuCount(),
            Timer::Frequency(),
            "Size", "Iter", "Latency", "CPU", "Bytes/Sec"
        );

        // warmup
        DoPings(x_HdrLen, 1000, true);
        Sleep(1000);

        Timer timer;
        CpuMonitor cpu;
        for (ULONG szXfer = 1; szXfer <= x_MaxXfer; szXfer <<= 1)
        {
            ULONG iterations = x_MaxIterations;
            if (iterations > (x_MaxVolume / szXfer))
            {
                iterations = x_MaxVolume / szXfer;
            }

            cpu.Start();
            timer.Start();

            DoPings(szXfer, iterations, false);

            timer.End();
            cpu.End();

            printf(
                " %9ul %9ul %9.2f %7.2f %11.0f\n",
                szXfer,
                iterations,
                timer.Report() / iterations,
                cpu.Report(),
                (double) szXfer * iterations / (timer.Report() / 1000000)
            );
        }

        // send terminate message
        NdTestBase::Send(nullptr, 0, 0);

        bool bTerminate = false;
        while (!bTerminate)
        {
            WaitForCompletion([&bTerminate](ND2_RESULT *pCompletion)
            {
                if ((pCompletion->Status == ND_SUCCESS && pCompletion->RequestContext == RECV_CTXT) ||
                    pCompletion->Status == ND_CANCELED)
                {
                    bTerminate = true;
                }
                else
                {
                    LOG_FAILURE_AND_EXIT(L"unexpected completion\n", __LINE__);
                }
            }, true);
        }
        NdTestBase::Shutdown();
    }
private:
    char *m_pBuf = nullptr;
    bool m_bUseBlocking = false;
    ND2_SGE *m_Sgl = nullptr;
    ULONG m_nMaxSge = 0;
    ULONG m_queueDepth = 0;
    UINT64 m_remoteAddress = 0;
    UINT32 m_remoteToken = 0;
    ULONG m_inlineThreshold = 0;
};

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    bool bServer = false;
    bool bClient = false;
    struct sockaddr_in v4Server = { 0 };
    DWORD nSge = 1;
    bool bPolling = false;
    bool bBlocking = false;
    bool bOpRead = false;
    bool bOpWrite = false;
    SIZE_T nPipeline = 128;

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
            nPipeline = _ttol(argv[++i]);
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

    if (bOpRead && bOpWrite)
    {
        printf("Exactly one of read (r) or write (w) op must be specified\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (bOpRead == false && bOpWrite == false)
    {
        bOpWrite = true;
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

    if (bPolling && bBlocking)
    {
        printf("Exactly one of blocking (b) or polling (p) must be specified\n\n");
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

    if (bServer)
    {
        NdrPingPongServer server(bBlocking);
        server.RunTest(v4Server, 0, nSge);
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

        NdrPingPongClient client(bBlocking);
        client.RunTest(v4Src, v4Server, 0, nSge);
    }

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
