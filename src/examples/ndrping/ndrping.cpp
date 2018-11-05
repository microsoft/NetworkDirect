//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndrping.cpp - NetworkDirect unidirectional RDMA ping test
//
// For this test, the server side simply allocates a buffer and
// advertises it to the client.  The client then performs RDMA
// operations to its heart's content with no further involvement
// of the server.
//

#include "ndcommon.h"
#include "ndtestutil.h"
#include <logging.h>

const USHORT x_DefaultPort = 54326;
const SIZE_T x_MaxXfer = (4 * 1024 * 1024);
const SIZE_T x_HdrLen = 40;
const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
const SIZE_T x_MaxIterations = 500000;

const LPCWSTR TESTNAME = L"ndrping.exe";

#define RECV_CTXT ((void *) 0x1000)
#define SEND_CTXT ((void *) 0x2000)
#define READ_CTXT ((void *) 0x3000)
#define WRITE_CTXT ((void *) 0x4000)

void ShowUsage()
{
    printf("nrdping [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-b            - Blocking I/O (wait for CQ notification)\n"
        "\t-p            - Polling I/O (poll on the CQ) (default)\n"
        "\t-w            - Use RMA Write (client only, default)\n"
        "\t-r            - Use RMA Read (client only)\n"
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
    DWORD  m_nIncomingReadLimit;
    UINT64 m_remoteAddress;
};

class NdrPingServer : public NdTestServerBase
{
public:
    NdrPingServer(bool opRead) :
        m_opRead(opRead)
    {}

    void RunTest(const struct sockaddr_in& v4Src, DWORD queueDepth, DWORD /*nSge */)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        // Make sure adapter supports in-order RDMA
        if ((adapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0)
        {
            LOG_FAILURE_AND_EXIT(L"Adapter does not support in-order RDMA.", __LINE__);
        }

        m_maxIncomingReads = adapterInfo.MaxInboundReadLimit;
        NdTestBase::CreateCQ(adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth), 1);

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer + x_HdrLen));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }

        ULONG flags = m_opRead ?
            ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_READ :
            ND_MR_FLAG_ALLOW_LOCAL_WRITE | ND_MR_FLAG_ALLOW_REMOTE_WRITE;
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen, flags);

        // post reveive for the terminate message
        ND2_SGE sge = { 0 };
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer + x_HdrLen;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();
        NdTestServerBase::Accept(adapterInfo.MaxInboundReadLimit, 0);

        NdTestBase::CreateMW();
        NdTestBase::Bind(m_pBuf, x_MaxXfer + x_HdrLen,
            m_opRead ? ND_OP_FLAG_ALLOW_READ : ND_OP_FLAG_ALLOW_WRITE);

        // send remote token and address
        PeerInfo *pInfo = static_cast<PeerInfo *> (m_pBuf);
        pInfo->m_remoteToken = m_pMw->GetRemoteToken();
        pInfo->m_nIncomingReadLimit = m_maxIncomingReads;
        pInfo->m_remoteAddress = reinterpret_cast<UINT64>(m_pBuf);
        NdTestBase::Send(&sge, 1, 0, SEND_CTXT);

        // wait for send completion
        WaitForCompletionAndCheckContext(SEND_CTXT);

        // wait for terminate message
        WaitForCompletionAndCheckContext(RECV_CTXT);

        //tear down
        NdTestBase::Shutdown();
    }

    ~NdrPingServer()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }
    }

private:
    ULONG m_maxIncomingReads = 0;
    void *m_pBuf = nullptr;
    bool m_opRead = false;
};

class NdrPingClient : public NdTestClientBase
{
public:
    NdrPingClient(bool bUseBlocking, bool opRead) :
        m_bUseBlocking(bUseBlocking),
        m_opRead(opRead)
    {}

    ~NdrPingClient()
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

    DWORD IssuePings(ULONG& iters, DWORD nSge, bool bRead, DWORD flags)
    {
        DWORD numIssued = 0;
        while (m_availCredits > 0 && iters > 0)
        {
            if (bRead)
            {
                NdTestBase::Read(m_Sgl, nSge, m_remoteAddress, m_remoteToken, flags, READ_CTXT);
            }
            else
            {
                NdTestBase::Write(m_Sgl, nSge, m_remoteAddress, m_remoteToken, flags, WRITE_CTXT);
            }
            m_availCredits--; iters--;
            numIssued++;
        }
        return numIssued;
    }

    void DoPings(ULONG size, ULONG iterations, DWORD nSge, bool bRead, bool bUseEvents)
    {
        HRESULT hr = ND_SUCCESS;
        DWORD numIssued = 0, numCompleted = 0;
        DWORD writeFlags = (!bRead && size < m_inlineThreshold) ? ND_OP_FLAG_INLINE : 0;
        numIssued = IssuePings(iterations, nSge, bRead, writeFlags);
        do
        {
            ND2_RESULT ndRes;
            WaitForCompletion(&ndRes, bUseEvents);
            hr = ndRes.Status;
            switch (hr)
            {
            case ND_SUCCESS:
                if (ndRes.RequestContext != (bRead ? READ_CTXT : WRITE_CTXT))
                {
                    LOG_FAILURE_AND_EXIT(L"Invalid completion context\n", __LINE__);
                }
                numCompleted++; m_availCredits++;
                break;

            case ND_CANCELED:
                break;

            default:
                LOG_FAILURE_HRESULT_AND_EXIT(
                    hr, L"INDCompletionQueue::GetResults returned result with %08x.", __LINE__);
            }
            numIssued += IssuePings(iterations, nSge, bRead, writeFlags);
        } while ((numIssued != numCompleted || iterations != 0) && hr == ND_SUCCESS);
    }

    void RunTest(const struct sockaddr_in& v4Src, const struct sockaddr_in& v4Dst, DWORD queueDepth, DWORD nSge)
    {
        NdTestBase::Init(v4Src);
        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        // Make sure adapter supports in-order RDMA
        if ((adapterInfo.AdapterFlags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED) == 0)
        {
            LOG_FAILURE_AND_EXIT(L"Adapter does not support in-order RDMA.", __LINE__);
        }

        m_queueDepth = (queueDepth > 0) ? min(queueDepth, adapterInfo.MaxCompletionQueueDepth) : adapterInfo.MaxCompletionQueueDepth;
        m_queueDepth = min(m_queueDepth, adapterInfo.MaxInitiatorQueueDepth);
        m_nMaxSge = min(nSge, adapterInfo.MaxInitiatorSge);
        m_inlineThreshold = adapterInfo.InlineRequestThreshold;
        if (m_opRead)
        {
            m_queueDepth = min(m_queueDepth, adapterInfo.MaxOutboundReadLimit);
            m_nMaxSge = min(nSge, adapterInfo.MaxReadSge);
        }

        NdTestBase::CreateMR();
        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxXfer + x_HdrLen));
        if (!m_pBuf)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate data buffer.", __LINE__);
        }

        m_Sgl = new (std::nothrow) ND2_SGE[m_nMaxSge];
        if (m_Sgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        ULONG flags = m_opRead ? ND_MR_FLAG_RDMA_READ_SINK | ND_MR_FLAG_ALLOW_LOCAL_WRITE : ND_MR_FLAG_ALLOW_LOCAL_WRITE;
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen, flags);

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(min(m_queueDepth, adapterInfo.MaxReceiveQueueDepth), nSge, m_inlineThreshold);

        ND2_SGE sge;
        sge.Buffer = m_pBuf;
        sge.BufferLength = x_MaxXfer + x_HdrLen;
        sge.MemoryRegionToken = m_pMr->GetLocalToken();
        NdTestBase::PostReceive(&sge, 1, RECV_CTXT);

        NdTestClientBase::Connect(v4Src, v4Dst, 0, m_opRead ? m_queueDepth : 0);
        NdTestClientBase::CompleteConnect();

        // wait for incoming peer info message
        WaitForCompletionAndCheckContext(RECV_CTXT);

        PeerInfo *pInfo = reinterpret_cast<PeerInfo *>(m_pBuf);
        m_remoteToken = pInfo->m_remoteToken;
        m_remoteAddress = pInfo->m_remoteAddress;
        if (m_opRead)
        {
            m_queueDepth = min(m_queueDepth, pInfo->m_nIncomingReadLimit);
        }

        printf("Using %u processors. Sender Frequency is %I64d\n\n"
            " %9s %9s %9s %7s %11s\n",
            CpuMonitor::CpuCount(),
            Timer::Frequency(),
            "Size", "Iter", "Latency", "CPU", "Bytes/Sec"
        );

        m_availCredits = m_queueDepth;

        // warmup
        DWORD nSgesUsed = NdTestBase::PrepareSge(m_Sgl, m_nMaxSge, m_pBuf, x_HdrLen, x_HdrLen, m_pMr->GetLocalToken());
        DoPings(x_HdrLen, 1000, nSgesUsed, m_opRead, m_bUseBlocking);
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

            nSgesUsed = NdTestBase::PrepareSge(m_Sgl, m_nMaxSge, m_pBuf, szXfer, x_HdrLen, m_pMr->GetLocalToken());

            cpu.Start();
            timer.Start();

            DoPings(szXfer, iterations, nSgesUsed, m_opRead, m_bUseBlocking);

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
        WaitForCompletion();


        NdTestBase::Shutdown();
    }
private:
    char *m_pBuf = nullptr;
    bool m_opRead = false;
    bool m_bUseBlocking = false;
    ND2_SGE *m_Sgl = nullptr;
    ULONG m_queueDepth = 0;
    ULONG m_availCredits = 0;
    ULONG m_nMaxSge = 0;
    ULONG m_inlineThreshold = 0;
    UINT64 m_remoteAddress = 0;
    UINT32 m_remoteToken = 0;
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
        else if ((wcscmp(arg, L"-r") == 0) || (wcscmp(arg, L"--read") == 0))
        {
            bOpRead = true;
        }
        else if ((wcscmp(arg, L"-w") == 0) || (wcscmp(arg, L"--write") == 0))
        {
            bOpRead = true;
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
        NdrPingServer server(bOpRead);
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

        NdrPingClient client(bBlocking, bOpRead);
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
