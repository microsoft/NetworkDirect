//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndping.cpp - NetworkDirect unidirectional send/recv ping test
//

#include "ndcommon.h"
#include "ndtestutil.h"
#include <logging.h>

const USHORT x_DefaultPort = 54324;
const SIZE_T x_MaxXfer = (4 * 1024 * 1024);
const ULONG  x_HdrLen = 40;
const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
const SIZE_T x_MaxIterations = 500000;

const LPCWSTR TESTNAME = L"ndping.exe";

void ShowUsage()
{
    printf("ndping [options] <ip>[:<port>]\n"
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

class NdPingServer : public NdTestServerBase
{
public:

    NdPingServer(char *pBuf, bool useEvents) :
        m_pBuf(pBuf),
        m_bUseEvents(useEvents)
    {}

    ~NdPingServer()
    {
        if (m_sgl != nullptr)
        {
            delete[] m_sgl;
        }
    }

    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge)
    {
        NdPingServer::Init(v4Src);
        NdTestBase::CreateMR();
        NdTestBase::RegisterDataBuffer(m_pBuf, x_MaxXfer + x_HdrLen, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);
        m_queueDepth = min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth);
        m_queueDepth = (queueDepth != 0) ? min(queueDepth, m_queueDepth) : m_queueDepth;
        m_inlineSizeThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(m_queueDepth, nSge, m_inlineSizeThreshold);
        NdTestServerBase::CreateListener();
        NdTestServerBase::Listen(v4Src);
        NdTestServerBase::GetConnectionRequest();

        m_sgl = new (std::nothrow) ND2_SGE[nSge];
        if (m_sgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        m_nSge = NdTestBase::PrepareSge(m_sgl, nSge,
            m_pBuf, x_MaxXfer, x_HdrLen, m_pMr->GetLocalToken());
        for (DWORD i = 0; i < m_queueDepth; i++)
        {
            NdTestBase::PostReceive(m_sgl, m_nSge);
        }

        // advertise one less to account for incoming SYNC message
        ULONG advertisedQueueDepth = m_queueDepth - 1;
        NdTestServerBase::Accept(0, 0, &advertisedQueueDepth, sizeof(advertisedQueueDepth));
        ReceivePings();

        //tear down
        NdTestBase::Shutdown();
    }

    void ReceivePings()
    {
        // Prepare an SGE for sending credit updates.
        ND2_SGE creditSge;
        creditSge.Buffer = m_pBuf;
        creditSge.BufferLength = 1;
        creditSge.MemoryRegionToken = m_pMr->GetLocalToken();

        SIZE_T threshold = m_queueDepth / 2;
        HRESULT hr = ND_SUCCESS;
        do
        {
            ND2_RESULT ndRes;
            WaitForCompletion(&ndRes, m_bUseEvents);

            hr = ndRes.Status;
            switch (hr)
            {
            case ND_SUCCESS:
                // Ignore send completions
                if (ndRes.RequestType == Nd2RequestTypeSend)
                {
                    break;
                }

                // Check for SYNC
                if (ndRes.BytesTransferred == 0)
                {
                    // ack SYNC message
                    NdTestBase::Send(nullptr, 0, 0);
                }

                // Repost receive
                NdTestBase::PostReceive(m_sgl, m_nSge);

                // Check if credit update is needed.
                if (--threshold == 0)
                {
                    NdTestBase::Send(&creditSge, 1,
                        creditSge.BufferLength < m_inlineSizeThreshold ? ND_OP_FLAG_INLINE : 0);
                    threshold = m_queueDepth / 2;
                }

                __fallthrough;
            case ND_CANCELED:
                break;

            default:
                LOG_FAILURE_HRESULT_AND_EXIT(
                    hr,
                    L"INDCompletionQueue::GetResults returned result with %08x.",
                    __LINE__);
            }
        } while (hr == ND_SUCCESS);
    }

private:
    DWORD m_queueDepth = 0;
    ND2_SGE* m_sgl = nullptr;
    DWORD m_nSge = 0;
    char *m_pBuf = nullptr;
    bool m_bUseEvents = false;
    DWORD m_inlineSizeThreshold = 0;
};

class NdPingClient : public NdTestClientBase
{
public:
    NdPingClient(char *pBuf, bool bUseEvents, size_t nPipeline) :
        m_pBuf(pBuf),
        m_bUseEvents(bUseEvents),
        m_maxOutSends(nPipeline)
    {}

    ~NdPingClient()
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
        m_inlineSizeThreshold = adapterInfo.InlineRequestThreshold;

        NdTestBase::CreateCQ(m_queueDepth);
        NdTestBase::CreateConnector();
        NdTestBase::CreateQueuePair(m_queueDepth, nMaxSge, m_inlineSizeThreshold);

        NdTestClientBase::Connect(v4Src, v4Dst, 0, 0);

        // get peer queue depth
        m_peerQueueDepth = 0;
        ULONG len = 0;
        if (m_pConnector->GetPrivateData(nullptr, &len) != ND_BUFFER_OVERFLOW)
        {
            LOG_FAILURE_AND_EXIT(L"GetPrivateData failed\n", __LINE__);
        }

        void *tmpBuf = malloc(len);
        if (tmpBuf == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate memory\n", __LINE__);
        }

        HRESULT hr = m_pConnector->GetPrivateData(tmpBuf, &len);
        if (ND_SUCCESS != hr)
        {
            free(tmpBuf);
            LOG_FAILURE_AND_EXIT(L"Failed to GetPrivateData\n", __LINE__);
        }

        // 'tmpBuf' is already checked for null, but CodeAnalysis throws a warning here
        // also, ignore unitialized memory warning for tmpBuf
#pragma warning( suppress : 6001 6011 )
        m_peerQueueDepth = m_nCredits = *((ULONG*)tmpBuf);
        free(tmpBuf);

        NdTestClientBase::CompleteConnect();

        //prepare Sge lists
        m_sendSgl = new (std::nothrow) ND2_SGE[nMaxSge];
        m_recvSgl = new (std::nothrow) ND2_SGE[nMaxSge];
        if (m_sendSgl == nullptr || m_recvSgl == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate sgl.", __LINE__);
        }

        m_numRecvSge = NdTestBase::PrepareSge(m_recvSgl, nMaxSge,
            m_pBuf, x_MaxXfer, x_HdrLen, m_pMr->GetLocalToken());

        printf("Using %u processors. Sender Frequency is %I64d\n\n"
            " %9s %9s %9s %7s %11s\n",
            CpuMonitor::CpuCount(),
            Timer::Frequency(),
            "Size", "Iter", "Latency", "CPU", "Bytes/Sec"
        );

        // warmup iterations
        DWORD numSendSges = NdTestBase::PrepareSge(m_sendSgl, nMaxSge,
            m_pBuf, x_HdrLen, x_HdrLen, m_pMr->GetLocalToken());
        SendPings(1000, numSendSges, x_HdrLen);
        Sleep(1000);

        Timer timer;
        CpuMonitor cpu;
        for (ULONG szXfer = 1; szXfer <= x_MaxXfer; szXfer <<= 1)
        {
            numSendSges = NdTestBase::PrepareSge(m_sendSgl, nMaxSge,
                m_pBuf, szXfer, x_HdrLen, m_pMr->GetLocalToken());

            ULONG iterations = x_MaxIterations;
            if (iterations > (x_MaxVolume / szXfer))
            {
                iterations = x_MaxVolume / szXfer;
            }

            cpu.Start();
            timer.Start();
            HRESULT hr = SendPings(iterations, numSendSges, szXfer);
            if (FAILED(hr))
            {
                LOG_FAILURE_AND_EXIT(L"Connection unexpectedly aborted.", __LINE__);
            }

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

        //tear down
        NdTestBase::Shutdown();
    }

    HRESULT SendPings(size_t iters, DWORD nSge, DWORD msgSize)
    {
        HRESULT hr = ND_SUCCESS;
        bool bGotSyncAck = false, bSyncSent = false;

        // post recvs - sync msg and flow control message
        NdTestBase::PostReceive(m_recvSgl, m_numRecvSge);
        NdTestBase::PostReceive(m_recvSgl, m_numRecvSge);

        size_t maxOutSends = m_maxOutSends;
        maxOutSends = (maxOutSends > iters) ? iters : maxOutSends;
        maxOutSends = (maxOutSends > m_queueDepth) ? m_queueDepth : maxOutSends;

        do
        {
            SIZE_T nResults;
            ND2_RESULT ndRes;

            // send whaterver possible
            size_t numDone = BlastSend(iters, maxOutSends, nSge, msgSize);
            iters -= numDone;

            if (m_bUseEvents)
            {
                WaitForEventNotification();
            }

            do
            {
                nResults = m_pCq->GetResults(&ndRes, 1);
                if (nResults == 0)
                {
                    break;
                }

                hr = ndRes.Status;
                switch (hr)
                {
                case ND_SUCCESS:
                    if (ndRes.RequestType == Nd2RequestTypeReceive)
                    {
                        if (ndRes.BytesTransferred == 0)
                        {
                            // sync msg
                            bGotSyncAck = true;
                        }
                        else
                        {
                            // got flow control message, update credits
                            m_nCredits += (m_peerQueueDepth / 2);
                            NdTestBase::PostReceive(m_recvSgl, m_numRecvSge);
                        }
                    }
                    else
                    {
                        m_numOutSends--;
                        // send Ack msg if we have sent all the messages
                        if (iters == 0 && !bSyncSent)
                        {
                            NdTestBase::Send(nullptr, 0, 0);
                            m_numOutSends++;
                            m_nCredits--;
                            bSyncSent = true;
                        }
                    }
                    __fallthrough;

                case ND_CANCELED:
                    break;

                default:
                    LOG_FAILURE_HRESULT_AND_EXIT(
                        hr,
                        L"INDCompletionQueue::GetResults returned result with %08x.",
                        __LINE__);
                }
            } while (nResults != 0);

        } while ((!bGotSyncAck) && hr == ND_SUCCESS);
        return hr;
    }

    size_t BlastSend(size_t iters, size_t maxOutSends, DWORD nSge, DWORD msgSize)
    {
        size_t numSent = 0;
        while (m_nCredits != 0 && iters > 0 && m_numOutSends < maxOutSends)
        {
            NdTestBase::Send(m_sendSgl, nSge, msgSize < m_inlineSizeThreshold ? ND_OP_FLAG_INLINE : 0);
            m_nCredits--; iters--;
            numSent++; m_numOutSends++;
        }
        return numSent;
    }

private:
    char *m_pBuf = nullptr;
    DWORD m_queueDepth = 0;
    size_t m_maxOutSends = 0;
    size_t m_numOutSends = 0;
    bool m_bUseEvents = false;
    ULONG m_nCredits = 0;
    ULONG m_peerQueueDepth = 0;
    ND2_SGE *m_sendSgl = nullptr, *m_recvSgl = nullptr;
    DWORD m_numRecvSge = 0;
    DWORD m_inlineSizeThreshold = 0;
};

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    bool bServer = false;
    bool bClient = false;
    struct sockaddr_in v4Server = { 0 };
    DWORD nSge = 1;
    bool bPolling = false;
    bool bBlocking = false;
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
        printf("Exactly one of blocking (b or polling (p) must be specified.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (nSge == 0)
    {
        printf("Invalid or missing SGE length.\n\n");
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
#pragma warning (suppress: 6001) // ignore unitialized memory warning for pBuf
        NdPingServer server(pBuf, bBlocking);
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

#pragma warning (suppress: 6001) // ignore unitialized memory warning for pBuf
        NdPingClient client(pBuf, bBlocking, nPipeline);
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
