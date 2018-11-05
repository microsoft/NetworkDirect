// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndcq.cpp - NetworkDirect Completion Queue test
//

#include "ndcommon.h"
#include "ndtestutil.h"
#include <logging.h>

const SIZE_T x_Iterations = 1000;

void ShowUsage()
{
    printf("ndcq [options] <IPv4 Address>\n"
        "Options:\n"
        "\t-l,--logFile <logFile>   Log output to a given file\n"
        "\t-h,--help                Show this message\n");
}

class NDCQTest : public NdTestBase
{
public:
    ~NDCQTest()
    {
        if (m_hIocp != nullptr)
        {
            CloseHandle(m_hIocp);
        }

        if (m_pCq1 != nullptr)
        {
            m_pCq1->Release();
        }

        if (m_pCq2 != nullptr)
        {
            m_pCq2->Release();
        }
    }

    void InitTest(const struct sockaddr_in &ipAddress)
    {
        NdTestBase::Init(ipAddress);

        ND2_ADAPTER_INFO adapterInfo = { 0 };
        NdTestBase::GetAdapterInfo(&adapterInfo);

        NdTestBase::CreateCQ(&m_pCq1, adapterInfo.MaxCompletionQueueDepth);
        NdTestBase::CreateCQ(&m_pCq2, adapterInfo.MaxCompletionQueueDepth);

        m_hIocp = CreateIoCompletionPort(m_hAdapterFile, nullptr, 0, 0);
        if (m_hIocp == nullptr)
        {
            printf("Failed to bind adapter to IOCP, error %u\n", GetLastError());
            exit(__LINE__);
        }
    }

    void TestInorderNotifyCancel()
    {
        // Request notification and cancel in the same order as request.
        printf("Testing in-order notify/cancelation...");

        OVERLAPPED ov1 = { 0 };
        OVERLAPPED ov2 = { 0 };

        for (SIZE_T i = 0; i < x_Iterations; i++)
        {
            // Request CQ notifications.
            HRESULT hr = m_pCq1->Notify(ND_CQ_NOTIFY_ANY, &ov1);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            hr = m_pCq2->Notify(ND_CQ_NOTIFY_ANY, &ov2);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Cancel CQ notifications.
            hr = m_pCq1->CancelOverlappedRequests();
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::CancelOverlappedResult failed with %08x\n", hr);
                exit(__LINE__);
            }

            hr = m_pCq2->CancelOverlappedRequests();
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::CancelOverlappedResult failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Reap completed I/Os.
            // First should be Ov1, since we canceled it first.
            OVERLAPPED* pOv;
            DWORD bytesRet;
            ULONG_PTR key;
            bool fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov1)
            {
                printf("Expected Send OV %p, got %p\n", &ov1, pOv);
                exit(__LINE__);
            }

            hr = m_pCq1->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }

            // Second should be Ov2, since we canceled it second.
            fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov2)
            {
                printf("Expected Recv OV %p, got %p\n", &ov2, pOv);
                exit(__LINE__);
            }

            hr = m_pCq2->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }
        }
        printf("PASSED\n");
    }

    void TestReverseOrderNotifyCancel()
    {
        // Request notification and cancel in reverse order.
        printf("Testing reverse-order notify/cancelation...");

        OVERLAPPED ov1 = { 0 };
        OVERLAPPED ov2 = { 0 };

        for (SIZE_T i = 0; i < x_Iterations; i++)
        {
            // Request CQ notifications.
            HRESULT hr = m_pCq1->Notify(ND_CQ_NOTIFY_ANY, &ov1);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            hr = m_pCq2->Notify(ND_CQ_NOTIFY_ANY, &ov2);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Cancel CQ notifications.
            hr = m_pCq2->CancelOverlappedRequests();
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::CancelOverlappedResult failed with %08x\n", hr);
                exit(__LINE__);
            }

            hr = m_pCq1->CancelOverlappedRequests();
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::CancelOverlappedResult failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Reap completed I/Os.
            // First should be Ov2, since we canceled it first.
            OVERLAPPED* pOv;
            DWORD bytesRet;
            ULONG_PTR key;
            bool fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov2)
            {
                printf("Expected Send OV %p, got %p\n", &ov1, pOv);
                exit(__LINE__);
            }

            hr = m_pCq2->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }

            // Second should be Ov1, since we canceled it second.
            fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov1)
            {
                printf("Expected Recv OV %p, got %p\n", &ov2, pOv);
                exit(__LINE__);
            }

            hr = m_pCq1->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }
        }
        printf("PASSED\n");
    }

    void TestMultipleNotifyCancel()
    {
        // Test Multiple requests to the same CQ.
        printf("Testing multiple notify/cancelation on one CQ...");

        OVERLAPPED ov1 = { 0 };
        OVERLAPPED ov2 = { 0 };

        for (SIZE_T i = 0; i < x_Iterations; i++)
        {
            // Request CQ notifications.
            HRESULT hr = m_pCq1->Notify(ND_CQ_NOTIFY_ANY, &ov1);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }

            hr = m_pCq1->Notify(ND_CQ_NOTIFY_ANY, &ov2);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Cancel CQ notifications.
            hr = m_pCq1->CancelOverlappedRequests();
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::CancelOverlappedResult failed with %08x\n", hr);
                exit(__LINE__);
            }

            // Reap completed I/Os.
            // First should be Ov1, since we requested it first.
            OVERLAPPED* pOv;
            DWORD bytesRet;
            ULONG_PTR key;
            bool fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov1)
            {
                printf("Expected Send OV %p, got %p\n", &ov1, pOv);
                exit(__LINE__);
            }

            hr = m_pCq1->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }

            // Second should be Ov2, since we requested it second.
            fSuccess = GetQueuedCompletionStatus(m_hIocp, &bytesRet, &key, &pOv, 0);
            if (fSuccess == true)
            {
                printf("Expected failure due to canceled I/O\n");
                exit(__LINE__);
            }

            if (pOv != &ov2)
            {
                printf("Expected Recv OV %p, got %p\n", &ov2, pOv);
                exit(__LINE__);
            }

            hr = m_pCq1->GetOverlappedResult(pOv, false);
            if (hr != ND_CANCELED)
            {
                printf("Expected ND_CANCELED, got %08x\n", hr);
                exit(__LINE__);
            }
        }
        printf("PASSED\n");
    }

private:
    void *m_pBuf = nullptr;
    HANDLE m_hIocp = nullptr;
    IND2CompletionQueue *m_pCq1 = nullptr;
    IND2CompletionQueue *m_pCq2 = nullptr;
};

void InvokeTests(struct sockaddr_in& v4)
{

    NDCQTest cqTest;
    cqTest.InitTest(v4);

    cqTest.TestInorderNotifyCancel();
    cqTest.TestReverseOrderNotifyCancel();
    cqTest.TestMultipleNotifyCancel();
}


int __cdecl _tmain(int argc, TCHAR* argv[])
{
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
        printf("NdStartup failed with %08x\n", hr);
        exit(__LINE__);
    }

    InvokeTests(v4);

    hr = NdCleanup();
    if (FAILED(hr))
    {
        printf("NdCleanup failed with %08x\n", hr);
        exit(__LINE__);
    }

    _fcloseall();
    WSACleanup();
    return 0;
}
