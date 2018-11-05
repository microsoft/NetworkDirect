//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

#include "ndtestutil.h"

//initializer
NdTestBase::NdTestBase() :
    m_pAdapter(nullptr),
    m_pMr(nullptr),
    m_pCq(nullptr),
    m_pQp(nullptr),
    m_pConnector(nullptr),
    m_hAdapterFile(nullptr),
    m_Buf(nullptr),
    m_pMw(nullptr)
{
    RtlZeroMemory(&m_Ov, sizeof(m_Ov));
}

//tear down
NdTestBase::~NdTestBase()
{
    if (m_pMr != nullptr)
    {
        m_pMr->Release();
    }

    if (m_pMw != nullptr)
    {
        m_pMw->Release();
    }

    if (m_pCq != nullptr)
    {
        m_pCq->Release();
    }

    if (m_pQp != nullptr)
    {
        m_pQp->Release();
    }

    if (m_pConnector != nullptr)
    {
        m_pConnector->Release();
    }

    if (m_hAdapterFile != nullptr)
    {
        CloseHandle(m_hAdapterFile);
    }

    if (m_pAdapter != nullptr)
    {
        m_pAdapter->Release();
    }

    if (m_Ov.hEvent != nullptr)
    {
        CloseHandle(m_Ov.hEvent);
    }

    if (m_Buf != nullptr)
    {
        delete[] m_Buf;
    }
}

void NdTestBase::CreateMR(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateMemoryRegion(
        IID_IND2MemoryRegion,
        m_hAdapterFile,
        reinterpret_cast<VOID**>(&m_pMr)
    );

    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::RegisterDataBuffer(
    DWORD bufferLength,
    ULONG type,
    HRESULT expectedResult,
    const char* errorMessage)
{
    m_Buf_Len = bufferLength;
    m_Buf = new (std::nothrow) char[m_Buf_Len];
    if (m_Buf == nullptr)
    {
        printf("Failed to allocate buffer.\n");
        exit(__LINE__);
    }

    RegisterDataBuffer(m_Buf, m_Buf_Len, type, expectedResult, errorMessage);
}

void NdTestBase::RegisterDataBuffer(
    void *pBuf,
    DWORD bufferLength,
    ULONG type,
    HRESULT expectedResult,
    const char* errorMessage)
{
#pragma warning(suppress: 6001)
    HRESULT hr = m_pMr->Register(
        pBuf,
        bufferLength,
        type,
        &m_Ov
    );
    if (hr == ND_PENDING)
    {
        hr = m_pMr->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::CreateMW(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateMemoryWindow(
        IID_IND2MemoryWindow,
        reinterpret_cast<VOID**>(&m_pMw)
    );

    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::InvalidateMW(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr;
    hr = m_pQp->Invalidate(nullptr, m_pMw, 0);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Bind(DWORD bufferLength, ULONG flags, void *context, HRESULT expectedResult, const char* errorMessage)
{
    Bind(m_Buf, bufferLength, flags, context, expectedResult, errorMessage);
}

void NdTestBase::Bind(const void *pBuf, DWORD bufferLength, ULONG flags, void *context, HRESULT expectedResult, const char* errorMessage)
{
#pragma warning(suppress: 6001)
    if (m_pQp->Bind(context, m_pMr, m_pMw, pBuf, bufferLength, flags) != ND_SUCCESS)
    {
        LogErrorExit("Bind failed\n", __LINE__);
    }

    ND2_RESULT ndRes;
    NdTestBase::WaitForCompletion(&ndRes);
    LogIfErrorExit(ndRes.Status, expectedResult, errorMessage, -1);
    if (ndRes.Status == ND_SUCCESS && ndRes.RequestContext != context)
    {
        LogErrorExit("Invalid context", __LINE__);
    }
}

void NdTestBase::CreateCQ(DWORD depth, HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateCompletionQueue(
        IID_IND2CompletionQueue,
        m_hAdapterFile,
        depth,
        0,
        0,
        reinterpret_cast<VOID**>(&m_pCq)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::CreateCQ(IND2CompletionQueue **pCq, DWORD depth, HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateCompletionQueue(
        IID_IND2CompletionQueue,
        m_hAdapterFile,
        depth,
        0,
        0,
        reinterpret_cast<VOID**>(pCq)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::CreateConnector(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateConnector(
        IID_IND2Connector,
        m_hAdapterFile,
        reinterpret_cast<VOID**>(&m_pConnector)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::CreateQueuePair(DWORD queueDepth, DWORD nSge, DWORD inlineDataSize,
    HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateQueuePair(
        IID_IND2QueuePair,
        m_pCq,
        m_pCq,
        nullptr,
        queueDepth,
        queueDepth,
        nSge,
        nSge,
        inlineDataSize,
        reinterpret_cast<VOID**>(&m_pQp)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}


void NdTestBase::CreateQueuePair(
    DWORD receiveQueueDepth,
    DWORD initiatorQueueDepth,
    DWORD maxReceiveRequestSge,
    DWORD maxInitiatorRequestSge,
    HRESULT expectedResult,
    const char* errorMessage)
{

    HRESULT hr = m_pAdapter->CreateQueuePair(
        IID_IND2QueuePair,
        m_pCq,
        m_pCq,
        nullptr,
        maxReceiveRequestSge,
        initiatorQueueDepth,
        maxReceiveRequestSge,
        maxInitiatorRequestSge,
        0,
        reinterpret_cast<VOID**>(&m_pQp)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
    UNUSED(receiveQueueDepth);
}

void NdTestBase::Init(_In_ const struct sockaddr_in& v4Src)
{
    HRESULT hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&v4Src),
        sizeof(v4Src),
        reinterpret_cast<void**>(&m_pAdapter)
    );
    if (FAILED(hr))
    {
        LogErrorExit("Failed open adapter.\n", __LINE__);
    }

    m_Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_Ov.hEvent == nullptr)
    {
        LogErrorExit("Failed to allocate event for overlapped operations.\n", __LINE__);
    }

    // Get the file handle for overlapped operations on this adapter.
    hr = m_pAdapter->CreateOverlappedFile(&m_hAdapterFile);
    if (FAILED(hr))
    {
        LogErrorExit(hr, "IND2Adapter::CreateOverlappedFile failed", __LINE__);
    }
}


void NdTestBase::GetAdapterInfo(ND2_ADAPTER_INFO *pAdapterInfo)
{
    memset(pAdapterInfo, 0, sizeof(*pAdapterInfo));
    pAdapterInfo->InfoVersion = ND_VERSION_2;
    ULONG adapterInfoSize = sizeof(*pAdapterInfo);
    HRESULT hr = m_pAdapter->Query(pAdapterInfo, &adapterInfoSize);
    if (FAILED(hr))
    {
        LogErrorExit(hr, "IND2Adapter::GetAdapterInfo failed", __LINE__);
    }
}


DWORD NdTestBase::PrepareSge(
    ND2_SGE *pSge,
    const DWORD nSge,
    char *buff,
    ULONG buffSize,
    ULONG headerSize,
    UINT32 memoryToken)
{
    DWORD currSge = 0;
    ULONG buffIdx = 0;
    ULONG currLen = 0;
    while (buffSize != 0 && currSge < nSge)
    {
        pSge[currSge].Buffer = buff + buffIdx;
        currLen = min(buffSize, headerSize);
        pSge[currSge].BufferLength = currLen;
        pSge[currSge].MemoryRegionToken = memoryToken;
        buffSize -= currLen;
        buffIdx += currLen;
        currSge++;
    }
    // add any remaining buffSize to the last sge
    if (buffSize > 0 && currSge > 0)
    {
        pSge[currSge - 1].BufferLength += buffSize;
    }
    return currSge;
}

void NdTestBase::DisconnectConnector()
{
    if (m_pConnector != nullptr)
    {
        m_pConnector->Disconnect(&m_Ov);
    }
}

void NdTestBase::DeregisterMemory()
{
    m_pMr->Deregister(&m_Ov);
}

void NdTestBase::GetResult(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pCq->GetOverlappedResult(&m_Ov, TRUE);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Shutdown()
{
    NdTestBase::DisconnectConnector();
    NdTestBase::DeregisterMemory();
}

void NdTestBase::PostReceive(
    const ND2_SGE* Sge,
    const DWORD nSge,
    void *requestContext,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr = m_pQp->Receive(requestContext, Sge, nSge);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Write(
    const ND2_SGE* Sge,
    const ULONG nSge,
    UINT64 remoteAddress,
    UINT32 remoteToken,
    DWORD flag,
    void *requestContext,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr;
    hr = m_pQp->Write(requestContext, Sge, nSge, remoteAddress, remoteToken, flag);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Read(
    const ND2_SGE* Sge,
    const ULONG nSge,
    UINT64 remoteAddress,
    UINT32 remoteToken,
    DWORD flag,
    void *requestContext,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr;
    hr = m_pQp->Read(requestContext, Sge, nSge, remoteAddress, remoteToken, flag);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Send(
    const ND2_SGE* Sge,
    const ULONG nSge,
    ULONG flags,
    void* requestContext,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr = m_pQp->Send(requestContext, Sge, nSge, flags);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Send(
    const ND2_SGE* Sge,
    const ULONG nSge,
    ULONG flags,
    bool expectFail,
    void* requestContext,
    const char* errorMessage)
{
    HRESULT hr = m_pQp->Send(requestContext, Sge, nSge, flags);

    if (expectFail && !FAILED(hr))
    {
        LogErrorExit(hr, errorMessage, __LINE__);
    }
    else if (!expectFail && FAILED(hr))
    {
        LogErrorExit(hr, errorMessage, __LINE__);
    }
}

void NdTestBase::WaitForEventNotification()
{
    HRESULT hr = m_pCq->Notify(ND_CQ_NOTIFY_ANY, &m_Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pCq->GetOverlappedResult(&m_Ov, TRUE);
    }
}

void NdTestBase::WaitForCompletion(
    const std::function<void(ND2_RESULT *)>& processCompletionFn,
    bool bBlocking)
{
    for (;;)
    {
        ND2_RESULT ndRes;
        if (m_pCq->GetResults(&ndRes, 1) == 1)
        {
            processCompletionFn(&ndRes);
            break;
        }
        if (bBlocking)
        {
            WaitForEventNotification();
        }
    };
}

// wait for CQ entry and check context
void NdTestBase::WaitForCompletionAndCheckContext(void *expectedContext)
{
    WaitForCompletion([&expectedContext](ND2_RESULT *pComp)
    {
        if (ND_SUCCESS != pComp->Status)
        {
            LogIfErrorExit(pComp->Status, ND_SUCCESS, "Unexpected completion status", __LINE__);
        }
        if (expectedContext != pComp->RequestContext)
        {
            LogErrorExit("Unexpected completion\n", __LINE__);
        }
    }, true);
}

// wait for CQ entry and get the result
void NdTestBase::WaitForCompletion(ND2_RESULT *pResult, bool bBlocking)
{
    WaitForCompletion([&pResult](ND2_RESULT *pCompRes)
    {
        *pResult = *pCompRes;
    }, bBlocking);
}

// wait for CQ entry
void NdTestBase::WaitForCompletion(HRESULT expectedResult, const char* errorMessage)
{
    ND2_RESULT ndRes;
    WaitForCompletion(&ndRes, true);
    LogIfErrorExit(ndRes.Status, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::FlushQP(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pQp->Flush();
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestBase::Reject(
    const VOID *pPrivateData,
    DWORD cbPrivateData,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr = m_pConnector->Reject(pPrivateData, cbPrivateData);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}
NdTestServerBase::NdTestServerBase() :
    m_pListen(nullptr)
{
}

NdTestServerBase::~NdTestServerBase()
{
    if (m_pListen != nullptr)
    {
        m_pListen->Release();
    }
}

void NdTestServerBase::CreateListener(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pAdapter->CreateListener(
        IID_IND2Listener,
        m_hAdapterFile,
        reinterpret_cast<VOID**>(&m_pListen)
    );
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestServerBase::Listen(
    _In_ const sockaddr_in& v4Src,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr = m_pListen->Bind(
        reinterpret_cast<const sockaddr*>(&v4Src),
        sizeof(v4Src)
    );
    LogIfErrorExit(hr, expectedResult, "Bind failed", __LINE__);
    hr = m_pListen->Listen(0);
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestServerBase::GetConnectionRequest(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pListen->GetConnectionRequest(m_pConnector, &m_Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pListen->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

void NdTestServerBase::Accept(
    DWORD inboundReadLimit,
    DWORD outboundReadLimit,
    const VOID *pPrivateData,
    DWORD cbPrivateData,
    HRESULT expectedResult,
    const char* errorMessage)
{
    //
    // Accept the connection.
    //
    HRESULT hr = m_pConnector->Accept(
        m_pQp,
        inboundReadLimit,
        outboundReadLimit,
        pPrivateData,
        cbPrivateData,
        &m_Ov
    );
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}


//
// Connect to the server.
//
void NdTestClientBase::Connect(
    _In_ const sockaddr_in& v4Src,
    _In_ const sockaddr_in& v4Dst,
    DWORD inboundReadLimit,
    DWORD outboundReadLimit,
    const VOID *pPrivateData,
    DWORD cbPrivateData,
    HRESULT expectedResult,
    const char* errorMessage)
{
    HRESULT hr = m_pConnector->Bind(
        reinterpret_cast<const sockaddr*>(&v4Src),
        sizeof(v4Src)
    );
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_Ov, TRUE);
    }

    hr = m_pConnector->Connect(
        m_pQp,
        reinterpret_cast<const sockaddr*>(&v4Dst),
        sizeof(v4Dst),
        inboundReadLimit,
        outboundReadLimit,
        pPrivateData,
        cbPrivateData,
        &m_Ov
    );
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}

//
// Complete the connection - this transitions the endpoint so it can send.
//
void NdTestClientBase::CompleteConnect(HRESULT expectedResult, const char* errorMessage)
{
    HRESULT hr = m_pConnector->CompleteConnect(&m_Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, expectedResult, errorMessage, __LINE__);
}
