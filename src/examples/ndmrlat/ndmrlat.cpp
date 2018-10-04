// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmrlat.cpp - NetworkDirect memory registration latency test
//

#include "ndcommon.h"
#include <logging.h>
#include "ndtestutil.h"

const SIZE_T x_MaxSize = (4 * 1024 * 1024);
const SIZE_T x_Iterations = 10000;

const LPCWSTR TESTNAME = L"ndmrlat.exe";

void ShowUsage()
{
    printf("ndmrlat [options] <IPv4 Address>\n"
        "Options:\n"
        "\t-l,--logFile <logFile>   Log output to a given file\n"
        "\t-h,--help                Show this message\n");
}

class NDMrLatencyTest : public NdTestBase
{
public:
    ~NDMrLatencyTest()
    {
        if (m_pBuf != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, m_pBuf);
        }

        if (m_hIocp != nullptr)
        {
            CloseHandle(m_hIocp);
        }
    }

    void InitTest(const struct sockaddr_in &ipAddress)
    {
        NdTestBase::Init(ipAddress);
        NdTestBase::CreateMR();

        m_pBuf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, x_MaxSize));
        if (m_pBuf == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate memeory\n", __LINE__);
        }

        m_hIocp = CreateIoCompletionPort(m_hAdapterFile, nullptr, 0, 0);
        if (m_hIocp == nullptr)
        {
            printf("Failed to bind adapter to IOCP, error %u\n", GetLastError());
            exit(__LINE__);
        }
    }

    void RunTest(OVERLAPPED *pOv)
    {
        Timer timer;
        CpuMonitor cpu;

        double totalRegTime;
        double totalRegCallTime;
        double totalDeregTime;
        double totalDeregCallTime;

        for (SIZE_T szXfer = 1; szXfer <= x_MaxSize; szXfer <<= 1)
        {
            totalRegTime = 0;
            totalRegCallTime = 0;
            totalDeregTime = 0;
            totalDeregCallTime = 0;

            cpu.Start();
            HRESULT hr;
            for (SIZE_T i = 0; i < x_Iterations; i++)
            {
                // Register
                timer.Start();
                hr = m_pMr->Register(m_pBuf, szXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE, pOv);
                timer.Split();

                if (FAILED(hr))
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"RegisterMemory failed with %08x\n", __LINE__);
                }

                if (pOv->hEvent == nullptr)
                {
                    DWORD bytesRet;
                    ULONG_PTR key;
                    OVERLAPPED* pOv2;
                    BOOL fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv2, INFINITE);
                    if (fSuccess == FALSE)
                    {
                        printf("GetQueuedCompletionStatus failed with %u\n", GetLastError());
                        exit(__LINE__);
                    }
                    if (pOv2 != pOv)
                    {
                        printf("Unexpected overlapped %p returned but expected %p\n", pOv2, pOv);
                        exit(__LINE__);
                    }
                }

                hr = m_pMr->GetOverlappedResult(pOv, pOv->hEvent != nullptr);
                timer.End();
                if (FAILED(hr))
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"INDAdapter->GetOverlappedResult failed with %08x\n", __LINE__);
                }

                totalRegCallTime += timer.ReportPreSplit();
                totalRegTime += timer.Report();

                // Deregister
                timer.Start();
                hr = m_pMr->Deregister(pOv);
                timer.Split();
                if (FAILED(hr))
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"DeregisterMemory failed with %08x\n", __LINE__);
                }

                if (pOv->hEvent == nullptr)
                {
                    DWORD bytesRet;
                    ULONG_PTR key;
                    OVERLAPPED* pOv2;
                    BOOL fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv2, INFINITE);
                    if (fSuccess == FALSE)
                    {
                        printf("GetQueuedCompletionStatus failed with %u\n", GetLastError());
                        exit(__LINE__);
                    }

                    if (pOv2 != pOv)
                    {
                        printf("Unexpected overlapped %p returned but expected %p\n", pOv2, pOv);
                        exit(__LINE__);
                    }
                }

                hr = m_pMr->GetOverlappedResult(pOv, pOv->hEvent != nullptr);
                timer.End();
                if (FAILED(hr))
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetOverlappedResult failed with %08x\n", __LINE__);
                }

                totalDeregCallTime += timer.ReportPreSplit();
                totalDeregTime += timer.Report();
            }
            cpu.End();

            printf(
                "%9Id %9.2f %9.2f %9.2f %9.2f %7.2f\n",
                szXfer,
                totalRegCallTime / x_Iterations,
                totalRegTime / x_Iterations,
                totalDeregCallTime / x_Iterations,
                totalDeregTime / x_Iterations,
                cpu.Report());
        }
    }

private:
    void *m_pBuf = nullptr;
    HANDLE m_hIocp = nullptr;
};

void InvokeTest(const struct sockaddr_in& v4)
{
    NDMrLatencyTest mrlatencyTest;
    mrlatencyTest.InitTest(v4);

    printf(
        "Using %u processors. Sender Frequency is %I64d\n",
        CpuMonitor::CpuCount(),
        Timer::Frequency());

    printf(
        "\n Event Driven:\n"
        "%29s %19s %7s\n"
        "%9s %9s %9s %9s %9s\n",
        "Register (usec)",
        "Deregister (usec)",
        "CPU",
        "Size",
        "call",
        "total",
        "call",
        "total"
    );

    OVERLAPPED Ov;

    //
    // Note that we run the test using GetOverlappedResult even though the adapter
    // is bound to an I/O completion port to make sure that the IOCP semantics are
    // working - that is, setting the lower bit of the hEvent member of the
    // OVERLAPPED structure prevents the completion from being reported to the IOCP.
    //
    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr)
    {
        printf("Create event failed with %u\n", GetLastError());
        exit(__LINE__);
    }

    //
    // Set the lower bit of the event handle so completions don't go to the IOCP.
    //
    Ov.hEvent = (HANDLE)(((SIZE_T)Ov.hEvent) | 0x1);

    mrlatencyTest.RunTest(&Ov);

    //
    // Now we run again, using the IOCP, to see if performance is any different.
    //
    CloseHandle(Ov.hEvent);
    Ov.hEvent = nullptr;

    printf(
        "\n IOCP:\n"
        "%29s %19s %7s\n"
        "%9s %9s %9s %9s %9s\n",
        "Register (usec)",
        "Deregister (usec)",
        "CPU",
        "Size",
        "call",
        "total",
        "call",
        "total"
    );

    mrlatencyTest.RunTest(&Ov);
}

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    WSADATA wsaData;
    INIT_LOG(TESTNAME);
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        printf("Failed to initialize Windows Sockets: %d\n", ret);
        exit(__LINE__);
    }

    for (int i = 1; i < argc; i++)
    {
        TCHAR *arg = argv[i];
        if ((wcscmp(arg, L"-l") == 0) || (wcscmp(arg, L"--logFile") == 0))
        {
            RedirectLogsToFile(argv[++i]);
        }
        else if ((wcscmp(arg, L"-h") == 0) || (wcscmp(arg, L"--help") == 0))
        {
            ShowUsage();
            exit(0);
        }
    }

    TCHAR *ipAddress = argv[argc - 1];
    struct sockaddr_in v4 = { 0 };
    int addrLen = sizeof(v4);
    WSAStringToAddress(
        ipAddress,
        AF_INET,
        nullptr,
        reinterpret_cast<struct sockaddr*>(&v4),
        &addrLen
    );

    if (v4.sin_addr.s_addr == 0)
    {
        printf("Bad address.\n");
        ShowUsage();
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdStartup failed with %08x\n", __LINE__);
    }

    InvokeTest(v4);

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
