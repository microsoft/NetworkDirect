// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmrlat.cpp - NetworkDirect memory registration bandwidth test
//

#include "ndcommon.h"
#include <logging.h>
#include "ndtestutil.h"
#include <map>

const unsigned long x_MaxSize = (4 * 1024 * 1024);
const unsigned long x_MaxReg = 10000;
const unsigned long x_MinReg = 2000; // Number of iterations for x_MaxSize.

const LPCWSTR TESTNAME = L"ndmrrate.exe";

void ShowUsage()
{
    printf("ndmrrate [options] <IPv4 Address>\n"
        "Options:\n"
        "\t-t,--threads <numThreads>   Number of threads for the test (default: 2)\n"
        "\t-l,--logFile <logFile>      Log output to a given file\n"
        "\t-h,--help                   Show this message\n");
}

std::map <DWORD, volatile unsigned> g_ThreadStatusMap;

class NDMrRateTest;
struct ThreadParam
{
    DWORD         m_tId;
    NDMrRateTest *m_pTest;
};

class NDMrRateTest : public NdTestBase
{
public:
    NDMrRateTest(DWORD numThreads) :
        m_nThreads(numThreads)
    {
    }

    void InitTest(const struct sockaddr_in &ip)
    {
        NdTestBase::Init(ip);
        m_hIocp = CreateIoCompletionPort(m_hAdapterFile, nullptr, 0, m_nThreads);
        if (m_hIocp == nullptr)
        {
            printf("Failed to bind adapter to IOCP, error %u\n", GetLastError());
            exit(__LINE__);
        }

        // reset counters
        for (unsigned long szXfer = 0; szXfer <= x_MaxSize; szXfer == 0 ? szXfer = 1 : szXfer <<= 1)
        {
            g_ThreadStatusMap[szXfer] = 0;
        }
    }

    ~NDMrRateTest()
    {
        if (m_hIocp != nullptr)
        {
            CloseHandle(m_hIocp);
        }
    }

    struct MemRegContext
    {
        OVERLAPPED ov;
        IND2MemoryRegion *pMr;
    };

    static inline void GetCompletion(HANDLE& hIocp, IND2MemoryRegion *pMr)
    {
        DWORD bytes;
        ULONG_PTR key;
        OVERLAPPED *pOv;

        if (!GetQueuedCompletionStatus(hIocp, &bytes, &key, &pOv, INFINITE))
        {
            LOG_FAILURE_AND_EXIT(L"GetQueuedCompletionStatus failed\n", __LINE__);
        }

        HRESULT hr = pMr->GetOverlappedResult(pOv, FALSE);
        if (hr != ND_SUCCESS)
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetOverlappedResult failed\n", __LINE__);
        }
    }

    static void ThreadBarrier(DWORD size, DWORD nThreads)
    {
        // signal completion of current thread
        InterlockedIncrement(&g_ThreadStatusMap[size]);

        // wait until other threads are done
        while (InterlockedCompareExchange(&g_ThreadStatusMap[size], 0, 0) != nThreads);
    }

    static DWORD WINAPI MrRateTest(void *param)
    {
        ThreadParam *threadParam = static_cast<ThreadParam *>(param);
        if (threadParam == nullptr || threadParam->m_pTest == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Invalid thread param\n", __LINE__);
        }

        void *pBuf = HeapAlloc(GetProcessHeap(), 0, x_MaxSize);
        if (pBuf == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"failed to allocate memory\n", __LINE__);
        }

#pragma warning (suppress: 6011) // threadParam and threadParam->m_pTest are already checked for nullptr
        IND2Adapter *pAdapter = threadParam->m_pTest->m_pAdapter;
        DWORD numThreads = threadParam->m_pTest->m_nThreads;

        // wait for other threads to be ready
        ThreadBarrier(0, numThreads);

        unsigned long iters;
        MemRegContext *pMrContext = new (std::nothrow) MemRegContext[x_MaxReg];
        if (pMrContext == nullptr)
        {
            HeapFree(GetProcessHeap(), 0, pBuf);
            LOG_FAILURE_AND_EXIT(L"failed to allocate memory for MemRegContext\n", __LINE__);
            return -1;
        }
        memset(pMrContext, 0, sizeof(MemRegContext) * x_MaxReg);

        for (unsigned long szXfer = 1; szXfer <= x_MaxSize; szXfer <<= 1)
        {
            Timer timer;
            CpuMonitor cpu;
            if ((x_MaxSize / szXfer) > (x_MaxReg / x_MinReg))
            {
                iters = x_MaxReg;
            }
            else
            {
                iters = static_cast<ULONG>((x_MaxSize / szXfer) * x_MinReg);
            }

            // register
            HRESULT hr;
            cpu.Start();
            timer.Start();
            for (unsigned long int i = 0; i < iters; i++)
            {
                hr = pAdapter->CreateMemoryRegion(IID_IND2MemoryRegion,
                    threadParam->m_pTest->m_hAdapterFile, reinterpret_cast<VOID**>(&(pMrContext[i].pMr)));
                if (hr != ND_SUCCESS)
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Failed to create memory region: %x\n", __LINE__);
                }

#pragma warning (suppress: 6387) // pBuf is already checked for nullptr
                hr = pMrContext[i].pMr->Register(pBuf, szXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE, &pMrContext[i].ov);
                if (FAILED(hr))
                {
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Failed to register memory: %x", __LINE__);
                }
                GetCompletion(threadParam->m_pTest->m_hIocp, pMrContext[i].pMr);
            }

            ThreadBarrier(szXfer, numThreads);
            timer.Split();
            cpu.Split();

            // deregister
            for (unsigned long int i = 0; i < iters; i++)
            {
                hr = pMrContext[i].pMr->Deregister(&pMrContext[i].ov);
                if (FAILED(hr))
                {
                    LOG_FAILURE_AND_EXIT(L"Failed to deregister memory", __LINE__);
                }
                GetCompletion(threadParam->m_pTest->m_hIocp, pMrContext[i].pMr);
            }

            ThreadBarrier(szXfer, numThreads * 2);
            timer.End();
            cpu.End();

            if (threadParam->m_tId == 0)
            {
                printf(
                    "%9ul %9ul %9.2f %9.2f  %9.2f %9.2f\n",
                    szXfer,
                    iters,
                    timer.ReportPreSplit() / iters,
                    cpu.ReportPreSplit(),
                    timer.ReportPostSplit() / iters,
                    cpu.ReportPostSplit()
                );
            }

            // release memory region
            for (unsigned long int i = 0; i < iters; i++)
            {
                pMrContext[i].pMr->Release();
            }
        }

        HeapFree(GetProcessHeap(), 0, pBuf);
        delete[] pMrContext;
        return 0;
    }

    void RunTest()
    {
        HANDLE* hThreads = new (std::nothrow) HANDLE[m_nThreads];
        ThreadParam *params = new (std::nothrow) ThreadParam[m_nThreads];

        if (hThreads == nullptr || params == nullptr)
        {
            printf("Failed to allocate memory for threads\n");
            exit(-1);
        }

        for (DWORD i = 0; i < m_nThreads; i++)
        {
            params[i].m_tId = i;
            params[i].m_pTest = this;
            hThreads[i] = CreateThread(nullptr, 0, &MrRateTest, &params[i], 0, nullptr);
            if (hThreads[i] == nullptr)
            {
                LOG_FAILURE_AND_EXIT(L"CreateThread failed\n", __LINE__);
            }
        }

        // Wait for the threads to exit.
        for (DWORD i = 0; i < m_nThreads; i++)
        {
#pragma warning(push)
#pragma warning(disable: 6387) //hThreads[i] is already nullptr checked
            WaitForSingleObject(hThreads[i], INFINITE);
            CloseHandle(hThreads[i]);
            hThreads[i] = nullptr;
#pragma warning(pop)
        }

        delete[] hThreads;
        delete[] params;
    }

private:
    HANDLE m_hIocp = nullptr;
    DWORD m_nThreads;
};

void InvokeTest(const struct sockaddr_in& ip, DWORD numThreads)
{
    NDMrRateTest mrRateTest(numThreads);
    mrRateTest.InitTest(ip);

    printf(
        "Using %u processors. Sender Frequency is %I64d\n",
        CpuMonitor::CpuCount(),
        Timer::Frequency());

    printf(
        "                        Register            Deregister\n"
        "     Size     Iter      usec        CPU     usec         CPU\n"
    );

    mrRateTest.RunTest();
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

    DWORD numThreads = 1;
    for (int i = 1; i < argc; i++)
    {
        TCHAR *arg = argv[i];
        if ((wcscmp(arg, L"-l") == 0) || (wcscmp(arg, L"--logFile") == 0))
        {
            RedirectLogsToFile(argv[++i]);
        }
        else if ((wcscmp(arg, L"-t") == 0) || (wcscmp(arg, L"--threads") == 0))
        {
            numThreads = _ttol(argv[++i]);
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
    WSAStringToAddress(ipAddress, AF_INET, nullptr,
        reinterpret_cast<struct sockaddr*>(&v4), &addrLen);

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

    printf("Running test with %d threads\n", numThreads);
    InvokeTest(v4, numThreads);

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
