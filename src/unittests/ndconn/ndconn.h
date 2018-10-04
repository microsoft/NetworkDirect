// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndconn.h - NetworkDirect connection scalability test
//

#pragma once

#include "ndtestutil.h"

class NDConnOverlapped : public OVERLAPPED
{
public:
    typedef void(*CompletionRoutine)(_In_ NDConnOverlapped* This);

    NDConnOverlapped(_In_ CompletionRoutine pfnSucceeded, _In_ CompletionRoutine pfnFailed) :
        m_pfnSucceeded(pfnSucceeded),
        m_pfnFailed(pfnFailed)
    {
        Internal = 0;
        InternalHigh = 0;
        Pointer = NULL;
        hEvent = NULL;
    };

    void Succeeded() { m_pfnSucceeded(this); }
    void Failed() { m_pfnFailed(this); }

protected:
    CompletionRoutine m_pfnSucceeded;
    CompletionRoutine m_pfnFailed;
};

class NDConnServer : public NdTestServerBase
{
    friend class NDConnReq;
    friend class NDConnServerQp;

public:
    NDConnServer(DWORD numThreads) :
        m_nThreads(numThreads),
        m_SendOv(SendSucceeded, SendFailed),
        m_RecvOv(RecvSucceeded, RecvFailed)
    {
    }

    ~NDConnServer()
    {
        ULONG_PTR Key;
        OVERLAPPED* pOv;
        DWORD BytesRet;

        if (m_pSendCq != NULL)
        {
            m_pSendCq->CancelOverlappedRequests();
            GetQueuedCompletionStatus(m_hIocp, &BytesRet, &Key, &pOv, INFINITE);
            m_pSendCq->Release();
        }

        if (m_pRecvCq != NULL)
        {
            m_pRecvCq->CancelOverlappedRequests();
            GetQueuedCompletionStatus(m_hIocp, &BytesRet, &Key, &pOv, INFINITE);
            m_pRecvCq->Release();
        }

        CloseHandle(m_hIocp);
        NdTestBase::Shutdown();
    }

    void Init(_In_ const struct sockaddr_in& v4Src);
    void RunTest(_In_ const struct sockaddr_in& v4Src, _In_ DWORD queueDepth, _In_ DWORD nSge);

protected:
    volatile LONG m_nOv = 0;
    NDConnOverlapped m_SendOv;
    NDConnOverlapped m_RecvOv;

private:
    __callback static void SendSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void SendFailed(_In_ NDConnOverlapped* pOv);
    __callback static void RecvSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void RecvFailed(_In_ NDConnOverlapped* pOv);
    __callback static DWORD CALLBACK ServerTestRoutine(_In_ LPVOID This);

    DWORD m_nThreads = 0;
    volatile bool m_bEndTest = false;

    IND2CompletionQueue *m_pSendCq = nullptr;
    IND2CompletionQueue *m_pRecvCq = nullptr;

    volatile long m_nQpCreated = 0;
    volatile long m_nQpDestroyed = 0;
    volatile long m_nConnFailure = 0;

    volatile long long m_AcceptTime = 0;
    volatile long long m_DisconnectTime = 0;
    HANDLE m_hIocp = nullptr;
};

class NDConnReq : public NDConnOverlapped
{
public:
    NDConnReq(NDConnServer* pTest) :
        NDConnOverlapped(GetConnSucceeded, GetConnFailed),
        m_pTest(pTest),
        m_pConnector(NULL)
    {
        GetNextRequest();
    };

    ~NDConnReq()
    {
        if (m_pConnector)
        {
            m_pConnector->Release();
        }
    }

    NDConnServer  *m_pTest;
    IND2Connector *m_pConnector;

private:
    void GetNextRequest();
    __callback static void GetConnSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void GetConnFailed(_In_ NDConnOverlapped* pOv);
};

class __declspec(novtable) NDConnReference
{
public:
    NDConnReference() : m_nRef(1) {};

    void AddRef()
    {
        InterlockedIncrement(&m_nRef);
    }

    void Release()
    {
        if (InterlockedDecrement(&m_nRef) > 0)
        {
            return;
        }

        delete this;
    }

protected:
    virtual ~NDConnReference() {};

private:
    LONG m_nRef;
};

class NDConnServerQp : public NDConnReference
{
    friend class NDConnServer;

private:
    NDConnServerQp(_In_ NDConnServer* pTest) :
        m_DisconnectOv(DisconnectSucceeded, DisconnectFailed),
        m_NotifyDisconnectOv(NotifyDisconnectSucceeded, NotifyDisconnectFailed),
        m_pTest(pTest),
        m_AcceptOv(AcceptSucceeded, AcceptFailed)
    {
    }

    ~NDConnServerQp()
    {
        if (m_pQp != NULL)
        {
            m_pQp->Release();
        }
        if (m_pConnector != NULL)
        {
            m_pConnector->Release();
        }
        InterlockedIncrement(&m_pTest->m_nQpDestroyed);
    }

    HRESULT Init(_In_ IND2Connector* pConnector);

public:
    static HRESULT Create(_In_ NDConnServer* pTest, _In_ IND2Connector* pConnector);

private:
    void Send();
    void AcceptError(_In_ HRESULT hr);

    __callback static void AcceptSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void AcceptFailed(_In_ NDConnOverlapped* pOv);
    __callback static void DisconnectSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void DisconnectFailed(_In_ NDConnOverlapped* pOv);
    __callback static void NotifyDisconnectSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void NotifyDisconnectFailed(_In_ NDConnOverlapped* pOv);

    static void SendDone(_In_ ND2_RESULT* pResult);
    static void RecvDone(_In_ ND2_RESULT* pResult);

    Timer m_Timer;
    NDConnServer* m_pTest = nullptr;

    IND2Connector *m_pConnector = nullptr;
    IND2QueuePair *m_pQp = nullptr;

    NDConnOverlapped m_AcceptOv;
    NDConnOverlapped m_DisconnectOv;
    NDConnOverlapped m_NotifyDisconnectOv;

    volatile LONG m_fDoSend = 0;
};

class NDConnClient : public NdTestClientBase
{
    friend class NDConnClientQp;

public:
    NDConnClient(DWORD nThreads) :
        m_SendOv(SendSucceeded, SendFailed),
        m_RecvOv(RecvSucceeded, RecvFailed),
        m_nThreads(nThreads)
    {
    }

    ~NDConnClient()
    {
        DWORD BytesRet;
        ULONG_PTR Key;
        OVERLAPPED* pOv;

        if (m_pSendCq != NULL)
        {
            m_pSendCq->CancelOverlappedRequests();
            GetQueuedCompletionStatus(m_hIocp, &BytesRet, &Key, &pOv, INFINITE);
            m_pSendCq->Release();
        }

        if (m_pRecvCq != NULL)
        {
            m_pRecvCq->CancelOverlappedRequests();
            GetQueuedCompletionStatus(m_hIocp, &BytesRet, &Key, &pOv, INFINITE);
            m_pRecvCq->Release();
        }

        CloseHandle(m_hIocp);
        NdTestBase::Shutdown();
    }
    void Init(_In_ const struct sockaddr_in& v4Src, _In_ const struct sockaddr_in& v4Dst);
    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD /*queueDepth*/,
        _In_ DWORD /*nSge*/);

private:
    __callback static void SendSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void SendFailed(_In_ NDConnOverlapped* pOv);
    __callback static void RecvSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void RecvFailed(_In_ NDConnOverlapped* pOv);

    LONG m_nThreads;

protected:
    struct sockaddr_in m_serverAddr = {0};
    struct sockaddr_in m_srcAddr = {0};

    IND2CompletionQueue *m_pSendCq = nullptr;
    IND2CompletionQueue *m_pRecvCq = nullptr;
    HANDLE m_hIocp = nullptr;
    
    NDConnOverlapped m_SendOv;
    NDConnOverlapped m_RecvOv;

    volatile LONGLONG m_ConnectTime = 0;
    volatile LONGLONG m_CompleteConnectTime = 0;
    volatile LONGLONG m_DisconnectTime = 0;

    volatile LONG m_nQpCreated = 0;
    volatile LONG m_nQpDestroyed = 0;

    volatile LONG m_nConnFailure = 0;
    volatile LONG m_nConnTimeout = 0;
    volatile bool m_bEndTest = false;

    __callback static DWORD CALLBACK ClientTestRoutine(_In_ LPVOID This);
};

class NDConnClientQp : public NDConnReference
{
    friend class NDConnClient;

public:
    static HRESULT Create(_In_ NDConnClient* pTest);

private:
    NDConnClientQp(_In_ NDConnClient* pTest) :
        m_ConnectOv(ConnectSucceeded, ConnectFailed),
        m_CompleteConnectOv(CompleteConnectSucceeded, CompleteConnectFailed),
        m_DisconnectOv(DisconnectSucceeded, DisconnectFailed),
        m_pTest(pTest)
    {
    }

    ~NDConnClientQp()
    {
        if (m_pQp != NULL)
        {
            m_pQp->Release();
        }

        if (m_pConnector != NULL)
        {
            m_pConnector->Release();
        }

        InterlockedIncrement(&m_pTest->m_nQpDestroyed);
    }

    HRESULT Init();
    void ConnectError(_In_ HRESULT hr);
    __callback static void ConnectSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void ConnectFailed(_In_ NDConnOverlapped* pOv);
    __callback static void CompleteConnectSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void CompleteConnectFailed(_In_ NDConnOverlapped* pOv);
    __callback static void DisconnectSucceeded(_In_ NDConnOverlapped* pOv);
    __callback static void DisconnectFailed(_In_ NDConnOverlapped* pOv);

    static void SendDone(_In_ ND2_RESULT* pResult);
    static void RecvDone(_In_ ND2_RESULT* pResult);

    Timer m_Timer;
    NDConnClient* m_pTest = nullptr;
    IND2Connector* m_pConnector = nullptr;
    IND2QueuePair* m_pQp = nullptr;
    
    NDConnOverlapped m_ConnectOv;
    NDConnOverlapped m_CompleteConnectOv;
    NDConnOverlapped m_DisconnectOv;
};
