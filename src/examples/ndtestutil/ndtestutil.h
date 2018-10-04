//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

#include "ndcommon.h"
#include <stdio.h>
#include <functional>

#ifndef _ND_UTIL
#define _ND_UTIL

#define RECV    0
#define SEND    1

#define UNUSED(x) (void)(x)

//logger info
_Analysis_noreturn_
inline void LogIfErrorExit(
    HRESULT result,
    HRESULT expectedResult,
    const char* errorMessage,
    int exitCode)
{
    if (result != expectedResult)
    {
        printf("Test failed.Test Result: %#x, Expected Result:%#x, %s\n", result, expectedResult, errorMessage);
        exit(exitCode);
    }
}

_Analysis_noreturn_
inline void LogErrorExit(
    HRESULT result,
    const char* errorMessage,
    int exitCode)
{
    printf("Test failed. Test Result:%#x,%s\n", result, errorMessage);
    exit(exitCode);
}

_Analysis_noreturn_
inline void LogErrorExit(
    const char* errorMessage,
    int exitCode)
{
    printf("Test Failed.\n");
    printf("%s\n", errorMessage);
    exit(exitCode);
}

_Analysis_noreturn_
inline void LogIfNoErrorExit(
    HRESULT result,
    const char* errorMessage,
    int exitCode)
{
    if (!FAILED(result))
    {
        printf("Test failed. Test Result:%#x,%s\n", result, errorMessage);
        exit(exitCode);
    }
}

inline void LogInfo(
    const char* message)
{
    printf("%s\n", message);
}


//base class
class NdTestBase
{
protected:
    IND2Adapter *m_pAdapter;
    IND2MemoryRegion *m_pMr;
    IND2CompletionQueue *m_pCq;
    IND2QueuePair *m_pQp;
    IND2Connector *m_pConnector;
    HANDLE m_hAdapterFile;
    DWORD m_Buf_Len = 0;
    void* m_Buf;
    IND2MemoryWindow* m_pMw;
    OVERLAPPED m_Ov;

protected:
    NdTestBase();
    ~NdTestBase();

    //Initialize the adaptor, overlapped handler
    void Init(
        _In_ const struct sockaddr_in& v4Src);

    //Create a memowy window
    void CreateMW(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateMemoryWindow failed");

    //invalidate a memory window
    void InvalidateMW(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair:InvalidateMemoryWindow failed");

    // get adapter info
    void GetAdapterInfo(
        ND2_ADAPTER_INFO* pAdapterInfo);

    //create a MR for ND2
    //will report error if return value is not same expected
    void CreateMR(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateMemoryRegion failed");

    //allocate and register data buffer with MR
    void RegisterDataBuffer(
        DWORD bufferLength,
        ULONG type,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2MemoryRegion::Register failed");

    // register user data buffer with MR
    void RegisterDataBuffer(
        void *pBuffer,
        DWORD bufferLength,
        ULONG type,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2MemoryRegion::Register failed");

    //create completion queue for given depth
    void CreateCQ(
        DWORD depth,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateCompletionQueue failed");

    //create copletion queue for given depth
    void CreateCQ(
        IND2CompletionQueue **pCq,
        DWORD depth,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateCompletionQueue failed");

    //Create connector with adaptor, must call after init
    void CreateConnector(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateConnector failed");

    //create Queue pair, only take maxReceiveRequestSge and use the same nSge for both send and receive
    void CreateQueuePair(
        DWORD queueDepth,
        DWORD nSge,
        DWORD inlineDataSize = 0,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateQueuePair failed");

    //Create Queue pair,allowing all arguments of the CreateQueuePair method
    void CreateQueuePair(
        DWORD receiveQueueDepth,
        DWORD initiatorQueueDepth,
        DWORD maxReceiveRequestSge,
        DWORD maxInitiatorRequestSge,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateQueuePair failed");

    //Disconnect Connector and release it
    //No error check
    void DisconnectConnector();

    //Deregister memory
    //No error check
    void DeregisterMemory();

    //Get result from completion queue
    void GetResult(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2CompletionQueue::GetOverlappedResult failed");

    // Prepare sge based on message size and return number of Sge's used.
    DWORD PrepareSge(
        ND2_SGE *pSge,
        const DWORD nSge,
        char *pBuf,
        ULONG buffSize,
        ULONG headerSize,
        UINT32 memoryToken);

    //Post receive Sges
    void PostReceive(
        const ND2_SGE* Sge,
        const DWORD nSge,
        void *requestContext = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Receive failed");

    //Send to remote side
    void Send(
        const ND2_SGE* Sge,
        const ULONG nSge,
        ULONG flags,
        void* requestContext = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Send failed");

    //Send to remote side
    void Send(
        const ND2_SGE* Sge,
        const ULONG nSge,
        ULONG flags,
        bool expectFail,
        void* requestContext = nullptr,
        const char* errorMessage = "IND2QueuePair::Send failed");

    //Write to remote peer
    void Write(
        const ND2_SGE* Sge,
        const ULONG nSge,
        UINT64 remoteAddress,
        UINT32 remoteToken,
        DWORD flag,
        void *requestContext = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Write failed");

    //Read from remote peer
    void Read(
        const ND2_SGE* Sge,
        const ULONG nSge,
        UINT64 remoteAddress,
        UINT32 remoteToken,
        DWORD flag,
        void *requestContext = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Read failed");

    // Wait for event notification
    void WaitForEventNotification();

    // wait for CQ entry and get the result
    void WaitForCompletion(ND2_RESULT *pResult, bool bBlocking = true);

    // wait for CQ entry and get the result
    void WaitForCompletion(
        const std::function<void (ND2_RESULT *)>& processCompletionFn,
        bool bBlocking);

    // wait for CQ entry
    void WaitForCompletion(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair:GetResults failed");

    // wait for CQ entry and check context
    void WaitForCompletionAndCheckContext(void *expectedContext);

    //bind buffer to MW
    void Bind(
        DWORD bufferLength,
        ULONG type,
        void *context = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Bind failed");

    //bind buffer to MW
    void Bind(
        const void *pBuf,
        DWORD bufferLength,
        ULONG flags,
        void *context = nullptr,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair::Bind failed");

    //tear down, no error check
    void Shutdown();

    //Flush Queue Pair
    void FlushQP(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2QueuePair:Flush failed");

    //Call Connector::Reject  to reject a connection
    void Reject(
        const VOID *pPrivateData,
        DWORD cbPrivateData,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Connector:Reject failed");
};


class NdTestServerBase : public NdTestBase
{
protected:
    IND2Listener *m_pListen;

public:
    NdTestServerBase();
    ~NdTestServerBase();

    //virtual method that each test case must implement
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    ) = 0;

    //Create listener
    void CreateListener(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Adapter::CreateListener failed");

    //listen to a socket address
    void Listen(
        _In_ const sockaddr_in& v4Src,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Listen::Listen failed");

    //Get connection request from client
    void GetConnectionRequest(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Listen::GetConnectionRequest failed");

    //Accept the connection request from client
    void Accept(DWORD inboundReadLimit,
        DWORD outboundReadLimit,
        const VOID *pPrivateData = nullptr,
        DWORD cbPrivateData = 0,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Connector::Accept failed");
};

class NdTestClientBase : public NdTestBase
{

public:
    //virtual method RunTest, all test cases must implement this method
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    ) = 0;


    //connect to server
    //bind then connect
    void Connect(
        _In_ const sockaddr_in& v4Src,
        _In_ const sockaddr_in& v4Dst,
        DWORD inboundReadLimit,
        DWORD outboundReadLimit,
        const VOID *pPrivateData = nullptr,
        DWORD cbPrivateData = 0,
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Connector::Connect failed");

    // Complete the connection - this transitions the endpoint so it can send.
    void CompleteConnect(
        HRESULT expectedResult = ND_SUCCESS,
        const char* errorMessage = "IND2Connector::CompleteConnect failed");
};

#endif
