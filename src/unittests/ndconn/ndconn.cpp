// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndconn.cpp - NetworkDirect connection scalability test
//

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ndcommon.h"
#include "ndconn.h"
#include "logging.h"

const USHORT x_DefaultPort = 54321;
const SIZE_T x_XferLen = 4096;

const LPCWSTR TESTNAME = L"ndconn.exe";

void ShowUsage()
{
    printf("ndconn [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s              - Start as server (listen on IP/Port)\n"
        "\t-c              - Start as client (connect to server IP/Port)\n"
        "\t-t <numThreads> - Number of threads for the test (default: 2)\n"
        "\t-l <logFile>    - Log output to a file named <logFile>\n"
        "<ip>              - IPv4 Address\n"
        "<port>            - Port number, (default: %hu)\n",
        x_DefaultPort
    );
}

void NDConnReq::GetNextRequest()
{
    HRESULT hr = m_pTest->m_pAdapter->CreateConnector(IID_IND2Connector,
        m_pTest->m_hAdapterFile, reinterpret_cast<void **>(&m_pConnector));
    if (FAILED(hr))
    {
        printf("IND2Adapter::CreateConnector failed with %08x\n", hr);
        exit(__LINE__);
    }

    InterlockedIncrement(&m_pTest->m_nOv);
    hr = m_pTest->m_pListen->GetConnectionRequest(m_pConnector, this);
    if (FAILED(hr))
    {
        InterlockedDecrement(&m_pTest->m_nOv);
        printf("IND2Listen::GetConnectionRequest failed with %08x\n", hr);
        exit(__LINE__);
    }
}

void NDConnReq::GetConnSucceeded(_In_ NDConnOverlapped *pOv)
{
    NDConnReq* This = static_cast<NDConnReq*>(pOv);

    NDConnServerQp::Create(This->m_pTest, This->m_pConnector);
    This->m_pConnector = nullptr;

    // Issue the next GetConnectionRequest.
    This->GetNextRequest();

    InterlockedDecrement(&This->m_pTest->m_nOv);
}

void NDConnReq::GetConnFailed(_In_ NDConnOverlapped *pOv)
{
    NDConnReq* This = static_cast<NDConnReq*>(pOv);
    HRESULT hr = This->m_pTest->m_pListen->GetOverlappedResult(pOv, FALSE);
    if (hr != ND_CANCELED)
    {
        printf("IND2Listen::GetConnectionRequest failed with %08x\n", hr);
        exit(__LINE__);
    }

    InterlockedDecrement(&This->m_pTest->m_nOv);
}

HRESULT NDConnServerQp::Init(_In_ IND2Connector* pConnector)
{
    m_pConnector = pConnector;

    HRESULT hr = m_pTest->m_pAdapter->CreateQueuePair(IID_IND2QueuePair,
        m_pTest->m_pRecvCq, m_pTest->m_pSendCq, nullptr,
        1, 1, 1, 1, 0, reinterpret_cast<void **>(&m_pQp));
    if (FAILED(hr))
    {
        printf("CreateQueuePair failed with %08x\n", hr);
        return hr;
    }

    // Pre-post receive request.
    ND2_SGE sge;
    sge.Buffer = m_pTest->m_Buf;
    sge.BufferLength = m_pTest->m_Buf_Len;
    sge.MemoryRegionToken = m_pTest->m_pMr->GetLocalToken();
    hr = m_pQp->Receive(this, &sge, 1);
    AddRef();
    if (FAILED(hr))
    {
        printf("IND2QueuePair::Receive failed with %08x\n", hr);
        Release();
        return hr;
    }

    InterlockedIncrement(&m_pTest->m_nQpCreated);

    // Accept the connection
    AddRef();
    m_Timer.Start();
    hr = m_pConnector->Accept(m_pQp, 0, 0, nullptr, 0, &m_AcceptOv);
    if (FAILED(hr))
    {
        AcceptError(hr);
        Release();
    }

    return ND_SUCCESS;
}

HRESULT NDConnServerQp::Create(_In_ NDConnServer* pTest, _In_ IND2Connector* pConnector)
{
    NDConnServerQp* pQp = new (std::nothrow) NDConnServerQp(pTest);
    if (pQp == nullptr)
    {
        return ND_NO_MEMORY;
    }

    HRESULT hr = pQp->Init(pConnector);
    if (FAILED(hr))
    {
        pQp->Release();
    }
    return hr;
}

void NDConnServerQp::AcceptSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_AcceptOv);

    pQp->AddRef();
    HRESULT hr = pQp->m_pConnector->NotifyDisconnect(&pQp->m_NotifyDisconnectOv);
    if (FAILED(hr))
    {
        pQp->Release(); // NotifyDisconnect Reference
        pQp->AcceptError(ND_CONNECTION_ABORTED);
        pQp->Release(); // Accept Reference
        return;
    }

    pQp->m_Timer.End();
    InterlockedExchangeAdd64(&pQp->m_pTest->m_AcceptTime, (LONGLONG)pQp->m_Timer.Report());
    if (InterlockedIncrement(&pQp->m_fDoSend) == 2)
    {
        pQp->Send();
    }
    pQp->Release();
}

void NDConnServerQp::AcceptFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_AcceptOv);
    HRESULT hr = pQp->m_pConnector->GetOverlappedResult(pOv, FALSE);
    pQp->AcceptError(hr);
    pQp->Release();
}

void NDConnServerQp::DisconnectSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_DisconnectOv);
    pQp->Release();

    pQp->m_Timer.End();
    InterlockedExchangeAdd64(&pQp->m_pTest->m_DisconnectTime, (LONGLONG)pQp->m_Timer.Report());

    // Cancel the NotifyDisconnect request.
    pQp->m_pConnector->CancelOverlappedRequests();

    // Release again to destroy it.
    pQp->Release();
}

void NDConnServerQp::DisconnectFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_DisconnectOv);
    printf(
        "IND2Connector::Disconnect failed with %08x\n",
        pQp->m_pConnector->GetOverlappedResult(pOv, FALSE)
    );
    exit(__LINE__);
}

void NDConnServerQp::NotifyDisconnectSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_NotifyDisconnectOv);
    if (pQp->m_fDoSend == 2)
    {
        pQp->Release();
        return;
    }
    pQp->AcceptError(ND_CONNECTION_ABORTED);
}

void NDConnServerQp::NotifyDisconnectFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnServerQp* pQp = CONTAINING_RECORD(pOv, NDConnServerQp, m_NotifyDisconnectOv);
    HRESULT hr = pQp->m_pConnector->GetOverlappedResult(pOv, FALSE);
    if (hr != ND_CANCELED && hr != STATUS_CONNECTION_DISCONNECTED)
    {
        printf("IND2Connector::NotifyDisconnect failed with %08x\n", hr);
        exit(__LINE__);
    }
    pQp->Release();
}

void NDConnServerQp::AcceptError(_In_ HRESULT hr)
{
    if (hr == ND_CONNECTION_ABORTED)
    {
        InterlockedIncrement(&m_pTest->m_nConnFailure);
        m_pQp->Flush();

        // Release again to destroy it.
        Release();
        return;
    }

    printf("IND2Connector::Accept failed with %08x\n", hr);
    exit(__LINE__);
}

void NDConnServerQp::Send()
{
    ND2_SGE sge;
    sge.Buffer = m_pTest->m_Buf;
    sge.BufferLength = m_pTest->m_Buf_Len;
    sge.MemoryRegionToken = m_pTest->m_pMr->GetLocalToken();

    AddRef();
    HRESULT hr = m_pQp->Send(this, &sge, 1, 0);
    if (FAILED(hr))
    {
        Release();
        printf("IND2Endpoint::Send failed with %08x\n", hr);
        exit(__LINE__);
    }
}

void NDConnServerQp::SendDone(_In_ ND2_RESULT* pResult)
{
    if (pResult->Status != ND_SUCCESS)
    {
        printf("IND2QueuePair::Send failed with %08x\n", pResult->Status);
        exit(__LINE__);
    }

    // We have a reference from the send - reuse it for the disconnect.
    NDConnServerQp* pQp = static_cast<NDConnServerQp *>(pResult->RequestContext);
    pQp->m_Timer.Start();

    HRESULT hr = pQp->m_pConnector->Disconnect(&pQp->m_DisconnectOv);
    if (FAILED(hr))
    {
        pQp->Release();
        printf("IND2Connector::Disconnect failed with %08x\n", hr);
        exit(__LINE__);
    }
}

void NDConnServerQp::RecvDone(_In_ ND2_RESULT* pResult)
{
    if (pResult->Status != ND_SUCCESS && pResult->Status != ND_CANCELED)
    {
        printf("IND2QueuePair::Receive failed with %08x\n", pResult->Status);
        exit(__LINE__);
    }

    NDConnServerQp* pQp = static_cast<NDConnServerQp *>(pResult->RequestContext);
    if (InterlockedIncrement(&pQp->m_fDoSend) == 2)
    {
        pQp->Send();
    }
    pQp->Release();
}


void NDConnServer::SendSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnServer* pTest = CONTAINING_RECORD(pOv, NDConnServer, m_SendOv);
    ND2_RESULT result;

    for (;; )
    {
        SIZE_T nResults = pTest->m_pSendCq->GetResults(&result, 1);
        if (nResults == 0)
        {
            HRESULT hr = pTest->m_pSendCq->Notify(ND_CQ_NOTIFY_ANY, pOv);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            return;
        }
        NDConnServerQp::SendDone(&result);
    }
}

void NDConnServer::SendFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnServer* pTest = CONTAINING_RECORD(pOv, NDConnServer, m_SendOv);
    HRESULT hr = pTest->m_pSendCq->GetOverlappedResult(pOv, FALSE);

    pTest->m_pSendCq->Release();
    if (hr == ND_CANCELED)
    {
        return;
    }

    printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
    exit(__LINE__);
}

void NDConnServer::RecvSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnServer* pTest = CONTAINING_RECORD(pOv, NDConnServer, m_RecvOv);
    ND2_RESULT result;
    for (;; )
    {
        SIZE_T nResults = pTest->m_pRecvCq->GetResults(&result, 1);
        if (nResults == 0)
        {
            HRESULT hr = pTest->m_pRecvCq->Notify(ND_CQ_NOTIFY_ANY, pOv);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            return;
        }
        NDConnServerQp::RecvDone(&result);
    }
}

void NDConnServer::RecvFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnServer* pTest = CONTAINING_RECORD(pOv, NDConnServer, m_RecvOv);
    HRESULT hr = pTest->m_pRecvCq->GetOverlappedResult(pOv, FALSE);

    pTest->m_pRecvCq->Release();
    if (hr == ND_CANCELED)
    {
        return;
    }

    printf("IND2CompletionQueue::Recv failed with %08x\n", hr);
    exit(__LINE__);
}

DWORD CALLBACK NDConnServer::ServerTestRoutine(_In_ LPVOID This)
{
    NDConnServer *pTest = (NDConnServer*) This;
    NDConnReq ConnReq(pTest);

    for (;;)
    {
        DWORD bytesRet;
        ULONG_PTR key;
        OVERLAPPED* pOv;
        bool fSuccess = GetQueuedCompletionStatus(pTest->m_hIocp, &bytesRet, &key, &pOv, INFINITE);
        if (pOv == nullptr)
        {
            return 0;
        }

        if (!fSuccess)
        {
            static_cast<NDConnOverlapped*>(pOv)->Failed();
        }
        else
        {
            static_cast<NDConnOverlapped*>(pOv)->Succeeded();
        }
    }
}

void NDConnServer::Init(_In_ const struct sockaddr_in& v4Src)
{
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_XferLen, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

    ND2_ADAPTER_INFO adapterInfo = { 0 };
    NdTestBase::GetAdapterInfo(&adapterInfo);
    ULONG queueDepth = min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth);

    NdTestBase::CreateCQ(&m_pSendCq, queueDepth);
    NdTestBase::CreateCQ(&m_pRecvCq, queueDepth);

    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);

    m_hIocp = CreateIoCompletionPort(m_hAdapterFile, nullptr, 0, 0);
    if (m_hIocp == nullptr)
    {
        printf("Failed to bind adapter to IOCP, error %u\n", GetLastError());
        exit(__LINE__);
    }
}

void NDConnServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/)
{
    NDConnServer::Init(v4Src);

    // Request CQ notifications.
    HRESULT hr = m_pSendCq->Notify(ND_CQ_NOTIFY_ANY, &m_SendOv);
    if (FAILED(hr))
    {
        printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
        exit(__LINE__);
    }
    hr = m_pRecvCq->Notify(ND_CQ_NOTIFY_ANY, &m_RecvOv);
    if (FAILED(hr))
    {
        printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
        exit(__LINE__);
    }

    // Create and launch test threads.
    for (DWORD i = 0; i < m_nThreads; i++)
    {
        HANDLE hThread = CreateThread(nullptr, 0, ServerTestRoutine, this, 0, nullptr);
        if (hThread == nullptr)
        {
            printf("CreateThread for thread %d of %d failed with %u.\n", i + 1, m_nThreads, GetLastError());
            exit(__LINE__);
        }
    }

    Timer timer;
    timer.Start();

    // Run for a minute and a bit.  The extra bit allows the client to finish up first.
    Sleep(65000);

    // Indicate to the connecting threads that they should stop accepting connections.
    m_bEndTest = true;

    // Let things run down.
    while (m_nQpCreated > m_nQpDestroyed)
    {
        Sleep(0);
    }

    timer.End();

    while (m_nOv > 0)
    {
        // Cancel outstanding connection requests.
        m_pListen->CancelOverlappedRequests();
        Sleep(100);
    }

    // Post a nullptr completion so the threads exit.
    for (DWORD i = 0; i < m_nThreads; i++)
    {
        PostQueuedCompletionStatus(m_hIocp, 0, 0, nullptr);
    }

    long nEpConnected = m_nQpCreated - m_nConnFailure;
    double ConnRate = ((double)m_nQpCreated - (double)m_nConnFailure) / (timer.Report() / 1000000.0);
    printf("%7.2f connections per second\n", ConnRate);

    // Print results
    printf(
        "Per connection times (microsec):\nAccept: %9.2f\nDisconnect: %9.2f\n",
        (double)m_AcceptTime / nEpConnected,
        (double)m_DisconnectTime / nEpConnected
    );

    printf("%d connection failures.\n", m_nConnFailure);
}


void NDConnClientQp::ConnectSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp* pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_ConnectOv);

    // ND_TIMEOUT is a success return value.
    HRESULT hr = pQp->m_pConnector->GetOverlappedResult(pOv, FALSE);
    if (hr == ND_TIMEOUT)
    {
        InterlockedIncrement(&pQp->m_pTest->m_nConnTimeout);
        // Retry, but don't reset the connect time.
        hr = pQp->m_pConnector->Connect(pQp->m_pQp,
            (const struct sockaddr*)&pQp->m_pTest->m_serverAddr, sizeof(pQp->m_pTest->m_serverAddr),
            IPPROTO_TCP, 0, nullptr, 0, pOv);
        if (FAILED(hr))
        {
            pQp->ConnectError(hr);
        }
        return;
    }

    pQp->m_Timer.End();
    InterlockedExchangeAdd64(&pQp->m_pTest->m_ConnectTime, (LONGLONG)pQp->m_Timer.Report());

    pQp->m_Timer.Start();
    hr = pQp->m_pConnector->CompleteConnect(&pQp->m_CompleteConnectOv);

    if (FAILED(hr))
    {
        printf("IND2Connector::CompleteConnect failed with %08x\n", hr);
        exit(__LINE__);
    }

    if (hr == ND_SUCCESS)
    {
        PostQueuedCompletionStatus(pQp->m_pTest->m_hIocp, 0, 0, &pQp->m_CompleteConnectOv);
    }
}

void NDConnClientQp::ConnectFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp* pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_ConnectOv);
    HRESULT hr = pQp->m_pConnector->GetOverlappedResult(pOv, FALSE);
    pQp->ConnectError(hr);
}

void NDConnClientQp::ConnectError(_In_ HRESULT hr)
{
    if (hr != ND_CONNECTION_REFUSED)
    {
        printf("Warning: IND2QueuePair::Connect failed with %08x\n", hr);
    }

    if (!m_pTest->m_bEndTest)
    {
        InterlockedIncrement(&m_pTest->m_nConnFailure);
    }
    Release();
    // Release again to destroy it.
    Release();
}

void NDConnClientQp::CompleteConnectSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp* pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_CompleteConnectOv);

    // ND_TIMEOUT is a success return value.
    HRESULT hr = pQp->m_pConnector->GetOverlappedResult(pOv, TRUE);
    if (hr != ND_SUCCESS)
    {
        printf("IND2Connector::CompleteConnect failed with %08x\n", hr);
        exit(__LINE__);
    }

    pQp->m_Timer.End();
    InterlockedExchangeAdd64(&pQp->m_pTest->m_CompleteConnectTime, (LONGLONG)pQp->m_Timer.Report());

    // Send a message.
    // We have a reference from the connection - reuse it for the send.
    ND2_SGE sge;
    sge.Buffer = pQp->m_pTest->m_Buf;
    sge.BufferLength = pQp->m_pTest->m_Buf_Len;
    sge.MemoryRegionToken = pQp->m_pTest->m_pMr->GetLocalToken();

    hr = pQp->m_pQp->Send(pQp, &sge, 1, 0);
    if (FAILED(hr))
    {
        printf("IND2QueuePair::Send failed with %08x\n", hr);
        exit(__LINE__);
    }
}

void NDConnClientQp::CompleteConnectFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp *pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_CompleteConnectOv);
    printf("IND2Connector::CompleteConnect failed with %08x\n",
        pQp->m_pConnector->GetOverlappedResult(pOv, FALSE));
    exit(__LINE__);
}

void NDConnClientQp::SendDone(_In_ ND2_RESULT* pResult)
{
    if (pResult->Status != ND_SUCCESS)
    {
        printf("IND2QueuePair::Send failed with %08x\n", pResult->Status);
        exit(__LINE__);
    }

    NDConnClientQp *pQp = static_cast<NDConnClientQp *>(pResult->RequestContext);
    pQp->Release();
}

void NDConnClientQp::RecvDone(_In_ ND2_RESULT* pResult)
{
    if (pResult->Status != ND_SUCCESS)
    {
        printf("IND2QueuePair::Receive failed with %08x\n", pResult->Status);
        exit(__LINE__);
    }

    // We have a reference from the receive - reuse it for the disconnect.
    NDConnClientQp *pQp = static_cast<NDConnClientQp *>(pResult->RequestContext);
    pQp->m_Timer.Start();
    HRESULT hr = pQp->m_pConnector->Disconnect(&pQp->m_DisconnectOv);
    if (FAILED(hr))
    {
        printf("IND2QueuePair::Disconnect failed with %08x\n", hr);
        exit(__LINE__);
    }
}

void NDConnClientQp::DisconnectSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp *pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_DisconnectOv);
    pQp->Release();

    pQp->m_Timer.End();
    InterlockedExchangeAdd64(&pQp->m_pTest->m_DisconnectTime, (LONGLONG)pQp->m_Timer.Report());

    // Release again to destroy it.
    pQp->Release();
}

void NDConnClientQp::DisconnectFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnClientQp* pQp = CONTAINING_RECORD(pOv, NDConnClientQp, m_DisconnectOv);
    printf("INDEndpoint::Disconnect failed with %08x\n",
        pQp->m_pConnector->GetOverlappedResult(pOv, FALSE));
    exit(__LINE__);
}

HRESULT NDConnClientQp::Init()
{
    HRESULT hr = m_pTest->m_pAdapter->CreateConnector(IID_IND2Connector,
        m_pTest->m_hAdapterFile, reinterpret_cast<void **>(&m_pConnector));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = m_pTest->m_pAdapter->CreateQueuePair(IID_IND2QueuePair, m_pTest->m_pRecvCq, m_pTest->m_pSendCq,
        nullptr, 1, 1, 1, 1, 0, reinterpret_cast<void **>(&m_pQp));

    // In the client side test, EP creation failure is not an
    // error (we're trying to connect as fast as possible).
    if (FAILED(hr))
    {
        return hr;
    }

    // Pre-post receive request.
    ND2_SGE sge;
    sge.Buffer = m_pTest->m_Buf;
    sge.BufferLength = m_pTest->m_Buf_Len;
    sge.MemoryRegionToken = m_pTest->m_pMr->GetLocalToken();
    AddRef();

    hr = m_pQp->Receive(this, &sge, 1);
    if (FAILED(hr))
    {
        printf("IND2QueuePair::Receive failed with %08x\n", hr);
        exit(__LINE__);
    }
    InterlockedIncrement(&m_pTest->m_nQpCreated);

    // Connect the endpoint
    AddRef();

    hr = m_pConnector->Bind(reinterpret_cast<const sockaddr*>(&m_pTest->m_srcAddr), sizeof(m_pTest->m_srcAddr));
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_pTest->m_Ov, TRUE);
    }

    m_Timer.Start();
    hr = m_pConnector->Connect(m_pQp, reinterpret_cast<const struct sockaddr*>(&m_pTest->m_serverAddr),
        sizeof(m_pTest->m_serverAddr), IPPROTO_TCP, 0, nullptr, 0, &m_ConnectOv);
    if (FAILED(hr))
    {
        printf("IND2Connector::Connect failed with %08x\n", hr);
        exit(__LINE__);
    }
    return hr;
}

HRESULT NDConnClientQp::Create(_In_ NDConnClient* pTest)
{
    NDConnClientQp* pQp = new (std::nothrow) NDConnClientQp(pTest);
    if (pQp == nullptr)
    {
        return ND_NO_MEMORY;
    }

    HRESULT hr = pQp->Init();
    if (FAILED(hr))
    {
        pQp->Release();
    }
    return hr;
}


void NDConnClient::SendSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnClient* pTest = CONTAINING_RECORD(pOv, NDConnClient, m_SendOv);
    ND2_RESULT result;

    for (;; )
    {
        SIZE_T nResults = pTest->m_pSendCq->GetResults(&result, 1);
        if (nResults == 0)
        {
            HRESULT hr = pTest->m_pSendCq->Notify(ND_CQ_NOTIFY_ANY, pOv);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            return;
        }
        NDConnClientQp::SendDone(&result);
    }
}

void NDConnClient::SendFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnClient* pTest = CONTAINING_RECORD(pOv, NDConnClient, m_SendOv);
    HRESULT hr = pTest->m_pSendCq->GetOverlappedResult(pOv, FALSE);

    pTest->m_pSendCq->Release();
    if (hr == ND_CANCELED)
    {
        return;
    }

    printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
    exit(__LINE__);
}

void NDConnClient::RecvSucceeded(_In_ NDConnOverlapped* pOv)
{
    NDConnClient* pTest = CONTAINING_RECORD(pOv, NDConnClient, m_RecvOv);
    ND2_RESULT result;

    for (;; )
    {
        SIZE_T nResults = pTest->m_pRecvCq->GetResults(&result, 1);
        if (nResults == 0)
        {
            HRESULT hr = pTest->m_pRecvCq->Notify(ND_CQ_NOTIFY_ANY, pOv);
            if (FAILED(hr))
            {
                printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
                exit(__LINE__);
            }
            return;
        }
        NDConnClientQp::RecvDone(&result);
    }
}

void NDConnClient::RecvFailed(_In_ NDConnOverlapped* pOv)
{
    NDConnClient* pTest = CONTAINING_RECORD(pOv, NDConnClient, m_RecvOv);
    HRESULT hr = pTest->m_pRecvCq->GetOverlappedResult(pOv, FALSE);

    pTest->m_pRecvCq->Release();
    if (hr == ND_CANCELED)
    {
        return;
    }

    printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
    exit(__LINE__);
}

DWORD CALLBACK NDConnClient::ClientTestRoutine(_In_ LPVOID This)
{
    NDConnClient* pTest = (NDConnClient*)This;
    // Artificially increase the number of EPs by 1.
    // This prevents the main thread from exiting prematurely without
    // the worker threads properly detecting that the connection
    // establishment should stop.
    InterlockedIncrement(&pTest->m_nQpCreated);
    bool fExit = false;

    for (;; )
    {
        DWORD timeout = 0;
        // Create a new EP and start connection establishment.
        // Note that creation could fail if the adapter under test
        // or system is out of resources, and we don't treat that as
        // a test failure.
        HRESULT hr;
        if (pTest->m_bEndTest == false)
        {
            hr = NDConnClientQp::Create(pTest);
        }
        else
        {
            if (fExit == false)
            {
                fExit = true;
                // We will not create any more endpoints, and all existing
                // endpoints are accounted for in the EP count.
                // We can release our artificial extra count.
                InterlockedDecrement(&pTest->m_nQpCreated);
            }
            hr = ND_CANCELED;
        }

        if (FAILED(hr))
        {
            timeout = INFINITE;
        }
        else
        {
            timeout = 0;
        }

        BOOL fSuccess;
        do
        {
            DWORD bytesRet;
            ULONG_PTR key;
            OVERLAPPED* pOv;

            fSuccess = GetQueuedCompletionStatus(pTest->m_hIocp, &bytesRet, &key, &pOv, timeout);
            if (!fSuccess)
            {
                DWORD lastError = GetLastError();
                if (lastError == WAIT_TIMEOUT)
                {
                    break;
                }

                if (pOv == nullptr)
                {
                    return 0;
                }

                static_cast<NDConnOverlapped*>(pOv)->Failed();
            }
            else
            {
                if (pOv == nullptr)
                {
                    return 0;
                }

                static_cast<NDConnOverlapped*>(pOv)->Succeeded();
            }
        } while (fSuccess == TRUE);
    }
}

void NDConnClient::Init(_In_ const struct sockaddr_in& v4Src, _In_ const struct sockaddr_in& v4Dst)
{
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_XferLen, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

    ND2_ADAPTER_INFO adapterInfo = { 0 };
    NdTestBase::GetAdapterInfo(&adapterInfo);
    ULONG queueDepth = min(adapterInfo.MaxCompletionQueueDepth, adapterInfo.MaxReceiveQueueDepth);

    NdTestBase::CreateCQ(&m_pSendCq, queueDepth);
    NdTestBase::CreateCQ(&m_pRecvCq, queueDepth);

    memcpy(&m_serverAddr, &v4Dst, sizeof(m_serverAddr));
    memcpy(&m_srcAddr, &v4Src, sizeof(m_srcAddr));

    m_hIocp = CreateIoCompletionPort(m_hAdapterFile, nullptr, 0, 0);
    if (m_hIocp == nullptr)
    {
        printf("Failed to bind adapter to IOCP, error %u\n", GetLastError());
        exit(__LINE__);
    }
}

void NDConnClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/)
{
    NDConnClient::Init(v4Src, v4Dst);

    // Request CQ notifications.
    HRESULT hr = m_pSendCq->Notify(ND_CQ_NOTIFY_ANY, &m_SendOv);
    if (FAILED(hr))
    {
        printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
        exit(__LINE__);
    }
    hr = m_pRecvCq->Notify(ND_CQ_NOTIFY_ANY, &m_RecvOv);
    if (FAILED(hr))
    {
        printf("IND2CompletionQueue::Notify failed with %08x\n", hr);
        exit(__LINE__);
    }

    // Wait 5 seconds for the server to be ready.
    Sleep(5000);

    // Create and launch test threads.
    for (LONG i = 0; i < m_nThreads; i++)
    {
        HANDLE hThread = CreateThread(nullptr, 0, ClientTestRoutine, this, 0, nullptr);
        if (hThread == nullptr)
        {
            printf("CreateThread for thread %d of %d failed with %u.\n", i + 1, m_nThreads, GetLastError());
            exit(__LINE__);
        }
    }

    Timer timer;
    timer.Start();

    // Run for a minute.
    Sleep(60000);

    // Signal the end of the test.
    m_bEndTest = true;

    // Let things run down.
    while (m_nQpCreated > m_nQpDestroyed)
    {
        Sleep(0);
    }

    // Post a nullptr completion so the threads exit.
    for (LONG i = 0; i < m_nThreads; i++)
    {
        PostQueuedCompletionStatus(m_hIocp, 0, 0, nullptr);
    }

    // Print results
    timer.End();
    long nEpConnected = m_nQpCreated - m_nConnFailure;
    double ConnRate = (double)nEpConnected / (timer.Report() / 1000000.0);
    printf("%7.2f connections per second\n", ConnRate);

    printf("Per connection times (microsec):\nConnect: %9.2f\nCompleteConnect: %9.2f\nDisconnect: %9.2f\n",
        (double)m_ConnectTime / nEpConnected,
        (double)m_CompleteConnectTime / nEpConnected,
        (double)m_DisconnectTime / nEpConnected);

    printf("%d connection failures.\n", m_nConnFailure);
    printf("%d connection timeouts.\n", m_nConnTimeout);
}

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

    DWORD nThreads = 2;
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
        else if ((wcscmp(arg, L"-t") == 0) || (wcscmp(arg, L"--threads") == 0))
        {
            nThreads = _ttol(argv[++i]);
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
        printf("Exactly one of client (c or server (s) must be specified.\n");
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

    if (nThreads == 0)
    {
        printf("Invalid or missing number of threads, expected positive count.\n");
        ShowUsage();
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        printf("NdStartup failed with %08x\n", hr);
        exit(__LINE__);
    }

    if (bServer)
    {
        NDConnServer server(nThreads);
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

        NDConnClient client(nThreads);
        client.RunTest(v4Src, v4Server, 0, 0);
    }

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
