// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmpic.cpp - NetworkDirect MS-MPI connection emulator test
//
// This test establishes connections following the same logic as MS-MPI.
//
// Operation:
//  1. Create 2 'ranks', {0,2} for client, {1,3} for server.
//  2. Establish connections:
//      - {0} actively connects to {1,3}
//      - {2} actively connects to {1}, waits for connection from {3}
//      - {1} actively connects to {2}, waits for connection from {0}
//      - {3} actively connects to {0,2}
//
//     This gives:
//      - one active-passive connections between {0,1}
//      - one active-passive connections between {3,2}
//      - two active-active connections between {0,3} and {1,2} with the higher
//        numbered rank taking the active role.
//  3. Transfer a 'close' message
//  4. On each connection, acknowledge the 'close' message with a 'close_ack'
//  5. Wait for completion of 'close_ack' send
//  6. Tear-down connection

#include "ndcommon.h"
#include <logging.h>

#define RECV_CTXT ((void *) 0x1000)
#define SEND_CTXT ((void *) 0x2000)

const USHORT x_DefaultPort = 54322;

struct PrivateData
{
    USHORT rank;
    UINT8 key[37];
};

const PrivateData x_DefaultData = {
    0,
    64,
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
    30,
    31,
    32,
    33,
    34,
    35
};

const LPCWSTR TESTNAME = L"ndmpic.exe";

class __declspec(novtable) CReference
{
public:
    CReference() : m_nRef(1) {};

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
    virtual ~CReference() {};

protected:
    LONG m_nRef;
};

class COverlapped : public OVERLAPPED
{
public:
    typedef void(*CompletionRoutine)(_In_ COverlapped* This);

public:
    COverlapped() :
        m_pfnSucceeded(nullptr),
        m_pfnFailed(nullptr)
    {
        Internal = 0;
        InternalHigh = 0;
        Pointer = nullptr;
        hEvent = nullptr;
    };

    void Set(_In_ CompletionRoutine pfnSucceeded, _In_ CompletionRoutine pfnFailed)
    {
        m_pfnSucceeded = pfnSucceeded;
        m_pfnFailed = pfnFailed;
    }

    void Complete(_In_ bool fResult)
    {
        if (!fResult)
        {
            m_pfnFailed(this);
        }
        else
        {
            m_pfnSucceeded(this);
        }
    }

protected:
    CompletionRoutine m_pfnSucceeded;
    CompletionRoutine m_pfnFailed;
};

// CConn: Class representing a connection between two ranks.
// A single rank can have multiple CConn objects.
class CConn : public CReference
{
public:
    CConn(_In_ IND2Adapter* pAdapter, _In_ HANDLE hAdapterFile, _In_ HANDLE hIocp,
        _In_ USHORT Rank, _In_ CReference* pParent);
    ~CConn();

    void Accept(
        _In_ IND2Connector* pConnector,
        _In_ USHORT PeerRank
    );

    void Connect(
        _In_ const struct sockaddr_in& addr,
        _In_ const struct sockaddr_in& srcAddr,
        _In_ USHORT PeerRank
    );

    UINT32 GetPeerRank() const { return m_PeerRank; }
    bool IsDone() const { return m_fDone; }

private:
    void DoConnect();
    void SendClose();

    __callback static void AcceptSucceeded(COverlapped* pOv);
    __callback static void AcceptFailed(COverlapped* pOv);
    __callback static void ConnectSucceeded(COverlapped* pOv);
    __callback static void ConnectFailed(COverlapped* pOv);
    __callback static void CompleteConnectSucceeded(COverlapped* pOv);
    __callback static void CompleteConnectFailed(COverlapped* pOv);
    __callback static void NotifySucceeded(COverlapped* pOv);
    __callback static void NotifyFailed(COverlapped* pOv);
    __callback static void DisconnectSucceeded(COverlapped* pOv);
    __callback static void DisconnectFailed(COverlapped* pOv);

private:
    CReference * m_pParent;
    IND2Adapter* m_pAdapter;
    HANDLE m_hAdapterFile;
    HANDLE m_hIocp;
    IND2CompletionQueue* m_pCq;
    COverlapped m_NotifyOv;
    IND2Connector* m_pConn;
    COverlapped m_Ov;
    IND2QueuePair *m_pQp;
    IND2MemoryRegion *m_pMr;

    UINT32 m_Buf[2];
    int m_nIoOperationsUntilDisconnect;
    enum SendType
    {
        Close,
        CloseAck
    };

    struct sockaddr_in m_Addr = { 0 };
    struct sockaddr_in m_srcAddr = { 0 };
    USHORT m_Rank;
    USHORT m_PeerRank;
    bool m_Active;
    bool m_fDone;
};

CConn::CConn(_In_ IND2Adapter* pAdapter, _In_ HANDLE hAdapterFile, _In_ HANDLE hIocp,
    _In_ USHORT Rank, _In_ CReference* pParent)
    : m_pParent(pParent),
    m_pAdapter(pAdapter),
    m_hAdapterFile(hAdapterFile),
    m_hIocp(hIocp),
    m_pCq(nullptr),
    m_pConn(nullptr),
    m_pQp(nullptr),
    m_pMr(nullptr),
    m_nIoOperationsUntilDisconnect(4),
    m_Rank(Rank),
    m_PeerRank((USHORT)-1),
    m_Active(false),
    m_fDone(false)
{
    m_pParent->AddRef();
    m_pAdapter->AddRef();

    // Synchronous memory registration.
    OVERLAPPED Ov = { 0 };
    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(GetLastError(), L"CreateEvent failed with %d", __LINE__);
    }

    // Prevent completion to the IOCP.
    Ov.hEvent = (HANDLE)((ULONG_PTR)Ov.hEvent | 1);

    HRESULT hr = m_pAdapter->CreateMemoryRegion(IID_IND2MemoryRegion, m_hAdapterFile,
        reinterpret_cast<VOID**>(&m_pMr));
    if (hr != ND_SUCCESS)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateMemoryRegion failed with %08x", __LINE__);
    }

    hr = m_pMr->Register(m_Buf, sizeof(m_Buf), ND_MR_FLAG_ALLOW_LOCAL_WRITE, &Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pMr->GetOverlappedResult(&Ov, TRUE);
    }

    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"RegisterMemory failed with %08x", __LINE__);
    }
    CloseHandle(Ov.hEvent);

    hr = m_pAdapter->CreateCompletionQueue(IID_IND2CompletionQueue, m_hAdapterFile, 4, 0, 0,
        reinterpret_cast<VOID**>(&m_pCq));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateCompletionQueue failed with %08x", __LINE__);
    }

    m_NotifyOv.Set(NotifySucceeded, NotifyFailed);
    AddRef();
    hr = m_pCq->Notify(ND_CQ_NOTIFY_ANY, &m_NotifyOv);
    if (FAILED(hr))
    {
        Release();
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Notify failed with %08x", __LINE__);
    }
}

void CConn::NotifySucceeded(COverlapped* pOv)
{
    HRESULT hr = ND_SUCCESS;
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_NotifyOv);

    for (;; )
    {
        ND2_RESULT result;
        SIZE_T nResults = pConn->m_pCq->GetResults(&result, 1);
        if (nResults == 0)
        {
            break;
        }

        pConn->m_nIoOperationsUntilDisconnect--;
        pConn->Release();

        hr = result.Status;
        if (result.RequestContext == RECV_CTXT)
        {
            if (hr == ND_CANCELED)
            {
                continue;
            }
            else if (hr != ND_SUCCESS)
            {
                LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Receive failed with %08x", __LINE__);
            }

            if (result.BytesTransferred != 0)
            {
                // Received 'close' message, send close-ack.  If we took the passive connection role,
                // send our own close.
                if (!pConn->m_Active)
                {
                    pConn->SendClose();
                }

                pConn->AddRef();
                printf("%hu close from %hu\n", pConn->m_Rank, pConn->m_PeerRank);
                hr = pConn->m_pQp->Send(SEND_CTXT, nullptr, 0, 0);
                if (FAILED(hr))
                {
                    pConn->Release();
                    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Send failed with %08x", __LINE__);
                }
            }
            else
            {
                printf("%hu close_ack from %hu\n", pConn->m_Rank, pConn->m_PeerRank);
            }
        }
        else
        {
            if (result.Status != ND_SUCCESS)
            {
                LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Send failed with %08x", __LINE__);
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        if (pConn->m_nIoOperationsUntilDisconnect == 0)
        {
            printf("%hu disconnect from %hu\n", pConn->m_Rank, pConn->m_PeerRank);
            pConn->m_Ov.Set(DisconnectSucceeded, DisconnectFailed);
            pConn->AddRef();
            hr = pConn->m_pConn->Disconnect(&pConn->m_Ov);
            if (FAILED(hr))
            {
                pConn->Release();
                LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Disconnect failed with %08x", __LINE__);
            }
        }
        else
        {
            pConn->AddRef();
            hr = pConn->m_pCq->Notify(ND_CQ_NOTIFY_ANY, &pConn->m_NotifyOv);
            if (FAILED(hr))
            {
                pConn->Release();
                LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Notify failed with %08x", __LINE__);
            }
        }
    }
    pConn->Release();
}

void CConn::NotifyFailed(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_NotifyOv);
    HRESULT hr = pConn->m_pCq->GetOverlappedResult(pOv, FALSE);
    if (hr == ND_CANCELED)
    {
        pConn->Release();
        return;
    }

    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Notify failed with %08x", __LINE__);
}

void CConn::DisconnectSucceeded(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    printf("%u disconnected from %u\n", pConn->m_Rank, pConn->m_PeerRank);
    pConn->m_fDone = true;
    pConn->m_pCq->CancelOverlappedRequests();
    pConn->Release();
}

void CConn::DisconnectFailed(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    HRESULT hr = pConn->m_pConn->GetOverlappedResult(pOv, FALSE);

    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Disconnect failed with %08x", __LINE__);
}

CConn::~CConn()
{
    // Synchronous memory deregistration.
    OVERLAPPED Ov = { 0 };
    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(GetLastError(), L"CreateEvent failed with %d", __LINE__);
    }

    // Prevent completion to the IOCP.
    Ov.hEvent = (HANDLE)((ULONG_PTR)Ov.hEvent | 1);

    HRESULT hr = m_pMr->Deregister(&Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pMr->GetOverlappedResult(&Ov, TRUE);
    }
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"DeregisterMemory failed with %08x", __LINE__);
    }

    CloseHandle(Ov.hEvent);

    m_pQp->Release();
    m_pConn->Release();
    m_pCq->Release();
    m_pAdapter->Release();
    m_pParent->Release();
}

void CConn::Accept(
    _In_ IND2Connector* pConnector,
    _In_ USHORT PeerRank
)
{
    HRESULT hr;

    m_pConn = pConnector;
    m_PeerRank = PeerRank;

    hr = m_pAdapter->CreateQueuePair(IID_IND2QueuePair, m_pCq, m_pCq, nullptr, 2, 2, 1, 1, 0,
        reinterpret_cast<VOID**>(&m_pQp));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateQueuePair failed with %08x", __LINE__);
    }

    ND2_SGE sge;
    sge.MemoryRegionToken = m_pMr->GetLocalToken();

    for (int i = 0; i < 2; i++)
    {
        sge.Buffer = &m_Buf[i];
        sge.BufferLength = sizeof(m_Buf[i]);

        AddRef();
        hr = m_pQp->Receive(RECV_CTXT, &sge, 1);
        if (FAILED(hr))
        {
            Release();
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Receive failed with %08x", __LINE__);
        }
    }

    m_Ov.Set(AcceptSucceeded, AcceptFailed);
    printf("%hu accept from %hu\n", m_Rank, m_PeerRank);
    AddRef();

    hr = pConnector->Accept(m_pQp, 0, 0, nullptr, 0, &m_Ov);
    if (FAILED(hr))
    {
        Release();
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Accept failed with %08x", __LINE__);
    }

    m_Active = false;
    m_nIoOperationsUntilDisconnect++;
}

void CConn::AcceptSucceeded(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    printf("%hu connected to %hu\n", pConn->m_Rank, pConn->m_PeerRank);

    if (--pConn->m_nIoOperationsUntilDisconnect == 0)
    {
        printf("Warning: %hu:%hu accept succeeded after data transfer!!!\n", pConn->m_Rank, pConn->m_PeerRank);
        printf("%hu disconnect from %hu\n", pConn->m_Rank, pConn->m_PeerRank);
        pConn->m_Ov.Set(DisconnectSucceeded, DisconnectFailed);
        pConn->AddRef();
        HRESULT hr = pConn->m_pConn->Disconnect(&pConn->m_Ov);
        if (FAILED(hr))
        {
            pConn->Release();
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Disconnect failed with %08x", __LINE__);
        }
    }

    // Release the reference for the Accept
    pConn->Release();
}

void CConn::SendClose()
{
    // Send the 'close' message.
    ND2_SGE sge;
    sge.Buffer = &m_Buf[0];
    sge.BufferLength = sizeof(m_Buf[0]);
    sge.MemoryRegionToken = m_pMr->GetLocalToken();

    AddRef();
    printf("%hu close to %hu\n", m_Rank, m_PeerRank);

    HRESULT hr = m_pQp->Send(SEND_CTXT, &sge, 1, 0);
    if (FAILED(hr))
    {
        Release();
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Send failed with %08x", __LINE__);
    }
}

void CConn::AcceptFailed(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    HRESULT hr = pConn->m_pConn->GetOverlappedResult(pOv, FALSE);

    pConn->m_nIoOperationsUntilDisconnect--;

    if (hr == ND_CONNECTION_ABORTED)
    {
        // The other side will retry the connection, which will cause this object
        // to be destroyed by its owning CRank.
        //
        // Flush the endpoint to release the Receive references, and release
        // the Accept reference.
        pConn->m_pQp->Flush();
        pConn->Release();
        return;
    }
    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Accept failed with %08x", __LINE__);
}

void CConn::Connect(
    _In_ const struct sockaddr_in& addr,
    _In_ const struct sockaddr_in& srcAddr,
    _In_ USHORT PeerRank
)
{
    HRESULT hr = m_pAdapter->CreateConnector(IID_IND2Connector, m_hAdapterFile, reinterpret_cast<VOID**>(&m_pConn));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateConnector failed with %08x", __LINE__);
    }

    hr = m_pAdapter->CreateQueuePair(IID_IND2QueuePair, m_pCq, m_pCq, nullptr, 2, 2, 1, 1, 0,
        reinterpret_cast<VOID**>(&m_pQp));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateQueuePair failed with %08x", __LINE__);
    }

    ND2_SGE sge;
    sge.MemoryRegionToken = m_pMr->GetLocalToken();
    for (int i = 0; i < 2; i++)
    {
        sge.Buffer = &m_Buf[i];
        sge.BufferLength = sizeof(m_Buf[i]);

        AddRef();
        hr = m_pQp->Receive(RECV_CTXT, &sge, 1);
        if (FAILED(hr))
        {
            Release();
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Receive failed with %08x", __LINE__);
        }
    }

    m_PeerRank = PeerRank;
    m_Addr = addr;
    m_Addr.sin_port = htons(x_DefaultPort + m_PeerRank);
    m_srcAddr = srcAddr;
    DoConnect();
}

void CConn::DoConnect()
{
    PrivateData data = x_DefaultData;
    data.rank = m_Rank;

    m_Ov.Set(ConnectSucceeded, ConnectFailed);
    AddRef();
    printf("%hu connect to %hu\n", m_Rank, m_PeerRank);

    m_srcAddr.sin_port = htons(x_DefaultPort + m_Rank * 2 + m_PeerRank);
    HRESULT hr = m_pConn->Bind(reinterpret_cast<const sockaddr*>(&m_srcAddr), sizeof(m_srcAddr));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Bind failed with %08x", __LINE__);
    }

    hr = m_pConn->Connect(m_pQp, reinterpret_cast<const sockaddr*>(&m_Addr), sizeof(m_Addr),
        0, 0, &data, sizeof(data), &m_Ov);
    if (FAILED(hr))
    {
        Release();
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Connect failed with %08x", __LINE__);
    }
    m_Active = true;
}

void CConn::ConnectSucceeded(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    HRESULT hr = pConn->m_pConn->GetOverlappedResult(pOv, FALSE);

    if (hr == ND_TIMEOUT)
    {
        // Try again.
        pConn->DoConnect();
    }
    else
    {
        pOv->Set(CompleteConnectSucceeded, CompleteConnectFailed);
        pConn->AddRef();
        hr = pConn->m_pConn->CompleteConnect(&pConn->m_Ov);
        if (FAILED(hr))
        {
            pConn->Release();
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CompleteConnect failed with %08x", __LINE__);
        }

        if (hr == ND_SUCCESS)
        {
            PostQueuedCompletionStatus(pConn->m_hIocp, 0, 0, pOv);
        }
    }
    pConn->Release();
}

void CConn::ConnectFailed(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);
    HRESULT hr = pConn->m_pConn->GetOverlappedResult(pOv, FALSE);

    switch (hr)
    {
    case ND_CONNECTION_REFUSED:
        if (pConn->m_Rank > pConn->m_PeerRank)
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Connect failed with %08x", __LINE__);
        }
        // We won't be doing any sends, but the receives will flush out.
        pConn->m_nIoOperationsUntilDisconnect -= 2;
        pConn->m_pQp->Flush();
        __fallthrough;

    case ND_CANCELED:
        pConn->Release();
        break;

    default:
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Connect failed with %08x", __LINE__);
    }
}

void CConn::CompleteConnectSucceeded(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);

    printf("%hu connected to %hu\n", pConn->m_Rank, pConn->m_PeerRank);
    pConn->SendClose();

    // Release the CompleteConnect reference.
    pConn->Release();
}

void CConn::CompleteConnectFailed(COverlapped* pOv)
{
    CConn* pConn = CONTAINING_RECORD(pOv, CConn, m_Ov);

    HRESULT hr = pConn->m_pConn->GetOverlappedResult(pOv, FALSE);
    LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Connect failed with %08x", __LINE__);
}

class CRank : public CReference
{
public:
    CRank(_In_ IND2Adapter* pAdapter, _In_ HANDLE hAdapterFile, _In_ HANDLE hIocp,
        _In_ const struct sockaddr_in& v4Src, _In_ USHORT Rank);
    ~CRank();

    void Connect(
        _In_ const struct sockaddr_in& addr,
        _In_ const struct sockaddr_in& srcAddr,
        _In_ USHORT Rank
    );

    bool IsDone();

private:
    __callback static void GetConnectionRequestSucceeded(COverlapped* pOv);
    __callback static void GetConnectionRequestFailed(COverlapped* pOv);

private:
    IND2Adapter  *m_pAdapter = nullptr;
    IND2Listener *m_pListen = nullptr;
    COverlapped m_ListenOv;
    IND2Connector *m_pConnector = nullptr;
    HANDLE m_hAdapterFile = nullptr;
    HANDLE m_hIocp = nullptr;

    CConn* m_pConn[2] = {0};
    USHORT m_Rank = 0;
};

// CRank: represents a rank in the test.
CRank::CRank(_In_ IND2Adapter* pAdapter, _In_ HANDLE hAdapterFile, _In_ HANDLE hIocp,
    _In_ const struct sockaddr_in& v4Src, _In_ USHORT Rank) :
    m_pAdapter(pAdapter),
    m_hAdapterFile(hAdapterFile),
    m_hIocp(hIocp),
    m_Rank(Rank)
{
    USHORT port = htons(x_DefaultPort + m_Rank);

    pAdapter->AddRef();

    for (int i = 0; i < _countof(m_pConn); i++)
    {
        m_pConn[i] = nullptr;
    }

    HRESULT hr = m_pAdapter->CreateListener(IID_IND2Listener, m_hAdapterFile, reinterpret_cast<VOID**>(&m_pListen));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateListener failed with %08x", __LINE__);
    }

    hr = m_pAdapter->CreateConnector(IID_IND2Connector, m_hAdapterFile, reinterpret_cast<VOID**>(&m_pConnector));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateConnector failed with %08x", __LINE__);
    }

    struct sockaddr_in bindAddr = v4Src;
    bindAddr.sin_port = port;
    hr = m_pListen->Bind(reinterpret_cast<const sockaddr*>(&bindAddr), sizeof(bindAddr));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Bind failed with %08x", __LINE__);
    }

    hr = m_pListen->Listen(0);
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Listen failed with %08x", __LINE__);
    }

    m_ListenOv.Set(GetConnectionRequestSucceeded, GetConnectionRequestFailed);
    AddRef();
    hr = m_pListen->GetConnectionRequest(m_pConnector, &m_ListenOv);
    if (FAILED(hr))
    {
        Release();
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetConnectionRequest failed with %08x", __LINE__);
    }
}

CRank::~CRank()
{
    m_pListen->Release();
    if (m_pConnector != nullptr)
    {
        m_pConnector->Release();
    }
    m_pAdapter->Release();
}

void CRank::Connect(
    _In_ const struct sockaddr_in& addr,
    _In_ const struct sockaddr_in& srcAddr,
    _In_ USHORT Rank
)
{
    // Find the index into the connection array.
    int i = Rank >> 1;

    if (m_pConn[i] != nullptr)
    {
        LOG_FAILURE_AND_EXIT(L"Connection is not nullptr", __LINE__);
    }

    m_pConn[i] = new (std::nothrow) CConn(m_pAdapter, m_hAdapterFile, m_hIocp, m_Rank, this);
    if (m_pConn[i] == nullptr)
    {
        LOG_FAILURE_AND_EXIT(L"Failed to allocate CConn", __LINE__);
    }

    m_pConn[i]->Connect(addr, srcAddr, Rank);
}

bool CRank::IsDone()
{
    for (int i = 0; i < _countof(m_pConn); i++)
    {
        if (m_pConn[i] != nullptr)
        {
            if (m_pConn[i]->IsDone())
            {
                m_pConn[i]->Release();
                m_pConn[i] = nullptr;
            }
            else
            {
                return false;
            }
        }
    }

    // Cancel the GetConnectionRequest request once it's the last operation left.
    if (m_nRef == 2)
    {
        m_pListen->CancelOverlappedRequests();
        return false;
    }

    // We're done when everything has unwound and the only reference left is the
    // lifetime reference.
    return (m_nRef == 1);
}

void CRank::GetConnectionRequestSucceeded(COverlapped* pOv)
{
    CRank* pRank = CONTAINING_RECORD(pOv, CRank, m_ListenOv);

    HRESULT hr;
    PrivateData data;
    ULONG len = sizeof(data);

    hr = pRank->m_pConnector->GetPrivateData(&data, &len);
    if (FAILED(hr) && hr != ND_BUFFER_OVERFLOW || len < sizeof(data))
    {
        // May have timed out...
        pRank->m_pConnector->Reject(nullptr, 0);
        pRank->m_pConnector->Release();
        pRank->m_pConnector = nullptr;

        if (hr != ND_CONNECTION_ABORTED)
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetConnectionData failed with %08x", __LINE__);
        }
        goto next;
    }

    switch (pRank->m_Rank)
    {
    case 0:
        switch (data.rank)
        {
        case 3:
            break;
        default:
            LOG_FAILURE_AND_EXIT(L"0 received connection request from invalid rank", __LINE__);
        }
        break;

    case 1:
        switch (data.rank)
        {
        case 0:
        case 2:
            break;
        default:
            LOG_FAILURE_AND_EXIT(L"1 received connection request from invalid rank", __LINE__);
        }
        break;

    case 2:
        switch (data.rank)
        {
        case 1:
        case 3:
            break;
        default:
            LOG_FAILURE_AND_EXIT(L"2 received connection request from invalid rank", __LINE__);
        }
        break;

    case 3:
        switch (data.rank)
        {
        case 0:
            break;
        default:
            LOG_FAILURE_AND_EXIT(L"1 received connection request from invalid rank", __LINE__);
        }
        break;
    }

    {
        int iConn = data.rank >> 1;
        if (pRank->m_pConn[iConn] != nullptr)
        {
            // Head to head.  See who wins.
            if (pRank->m_Rank > data.rank)
            {
                //
                // Our connection request will take the active role.  Reject.
                //
                printf("%hu reject %hu\n", pRank->m_Rank, data.rank);
                pRank->m_pConnector->Reject(nullptr, 0);
                pRank->m_pConnector->Release();
                pRank->m_pConnector = nullptr;
                goto next;
            }

            // Our connection request will be rejected by the other side.
            pRank->m_pConn[iConn]->Release();
            pRank->m_pConn[iConn] = nullptr;
        }

        struct sockaddr_in DestAddr;
        len = sizeof(DestAddr);
        hr = pRank->m_pConnector->GetPeerAddress(
            (struct sockaddr*)&DestAddr,
            &len);
        if (FAILED(hr))
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetPeerAddress failed with %08x", __LINE__);
        }

        pRank->m_pConn[iConn] = new (std::nothrow) CConn(pRank->m_pAdapter, pRank->m_hAdapterFile, pRank->m_hIocp, pRank->m_Rank, pRank);
        if (pRank->m_pConn[iConn] == nullptr)
        {
            LOG_FAILURE_AND_EXIT(L"Failed to allocate CConn", __LINE__);
        }

        pRank->m_pConn[iConn]->Accept(pRank->m_pConnector, data.rank);
    }

next:
    hr = pRank->m_pAdapter->CreateConnector(IID_IND2Connector, pRank->m_hAdapterFile, reinterpret_cast<VOID**>(&pRank->m_pConnector));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateConnector failed with %08x", __LINE__);
    }

    hr = pRank->m_pListen->GetConnectionRequest(pRank->m_pConnector, &pRank->m_ListenOv);
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetConnectionRequest failed with %08x", __LINE__);
    }
}

void CRank::GetConnectionRequestFailed(COverlapped* pOv)
{
    CRank* pRank = CONTAINING_RECORD(pOv, CRank, m_ListenOv);
    HRESULT hr = pRank->m_pListen->GetOverlappedResult(pOv, FALSE);

    if (hr != ND_CANCELED)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"GetConnectionRequest failed with %08x", __LINE__);
    }
    pRank->Release();
}

void ShowUsage()
{
    printf("ndmpic [options] <local ip> <remote ip>\n"
        "Options:\n"
        "\t-s            - Start as server (start ranks 2 & 3)\n"
        "\t-c            - Start as client (start ranks 0 & 1)\n"
        "\t-l <logFile>  - Log output to a file named <logFile>\n"
        "\t<local ip>    - IPv4 Address on which to listen for incoming connections\n"
        "\t<remote ip>   - IPv4 Address of other process\n"
    );
}

void TestRoutine(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ USHORT Rank
)
{
    IND2Adapter* pAdapter;
    HRESULT hr = NdOpenAdapter(IID_IND2Adapter, reinterpret_cast<const struct sockaddr*>(&v4Src), sizeof(v4Src),
        reinterpret_cast<void**>(&pAdapter));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdOpenAdapter failed with %08x", __LINE__);
    }

    HANDLE hAdapterFile;
    hr = pAdapter->CreateOverlappedFile(&hAdapterFile);
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"CreateOverlappedFile failed with %08x", __LINE__);
    }

    HANDLE hIocp = CreateIoCompletionPort(hAdapterFile, nullptr, 0, 0);
    if (hIocp == nullptr)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(GetLastError(), L"CreateIoCompletionPort failed with %u", __LINE__);
    }

    CRank* pRank[2];
#pragma warning (suppress: 6387) // hIocp is already checked for nullptr
    pRank[0] = new (std::nothrow) CRank(pAdapter, hAdapterFile, hIocp, v4Src, Rank);
    if (pRank[0] == nullptr)
    {
        LOG_FAILURE_AND_EXIT(L"Failed to allocate CRank[0]", __LINE__);
    }
#pragma warning (suppress: 6387) // hIocp is already checked for nullptr
    pRank[1] = new (std::nothrow) CRank(pAdapter, hAdapterFile, hIocp, v4Src, Rank + 2);
    if (pRank[1] == nullptr)
    {
        LOG_FAILURE_AND_EXIT(L"Failed to allocate CRank[1]", __LINE__);
    }

    // Give the other side a little time to come up...
    Sleep(5000);

    // Start connections.
    if (Rank == 0)
    {
        pRank[0]->Connect(v4Dst, v4Src, 1);
        pRank[0]->Connect(v4Dst, v4Src, 3);
        pRank[1]->Connect(v4Dst, v4Src, 1);
    }
    else
    {
        pRank[0]->Connect(v4Dst, v4Src, 2);
        pRank[1]->Connect(v4Dst, v4Src, 0);
        pRank[1]->Connect(v4Dst, v4Src, 2);
    }

    bool fDone;
    do
    {
        DWORD bytesRet;
        ULONG_PTR key;
        OVERLAPPED* pOv = nullptr;
#pragma warning (suppress: 6387) // hIocp is already checked for nullptr
        bool fSuccess = GetQueuedCompletionStatus(hIocp, &bytesRet, &key, &pOv, INFINITE);
        if (pOv)
        {
            static_cast<COverlapped*>(pOv)->Complete(fSuccess);
        }
        else if (GetLastError() != WAIT_TIMEOUT)
        {
            LOG_FAILURE_HRESULT_AND_EXIT(GetLastError(), L"GetQueuedCompletionStatus returned %u", __LINE__);
        }

        fDone = true;
        for (int i = 0; i < _countof(pRank); i++)
        {
            if (!pRank[i]->IsDone())
            {
                fDone = false;
                break;
            }
        }

    } while (!fDone);

    pRank[1]->Release();
    pRank[0]->Release();
    pAdapter->Release();
}

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    USHORT baseRank = 0;
    struct sockaddr_in v4Src = { 0 };
    struct sockaddr_in v4Dst = { 0 };

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
            baseRank = 1;
        }
        else if ((wcscmp(arg, L"-c") == 0) || (wcscmp(arg, L"-C") == 0))
        {
            baseRank = 0;
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

    int len = sizeof(v4Src);
    WSAStringToAddress(argv[argc - 2], AF_INET, nullptr, reinterpret_cast<struct sockaddr*>(&v4Src), &len);

    len = sizeof(v4Dst);
    WSAStringToAddress(argv[argc - 1], AF_INET, nullptr, reinterpret_cast<struct sockaddr*>(&v4Dst), &len);

    if (v4Src.sin_addr.s_addr == 0)
    {
        printf("Bad source address.\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (v4Dst.sin_addr.s_addr == 0)
    {
        printf("Bad destination address.\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (baseRank == -1)
    {
        printf("Either '-c' or '-s' needs to be specified.\n");
        ShowUsage();
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdStartup failed with %08x", __LINE__);
    }

    TestRoutine(v4Src, v4Dst, baseRank);

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
