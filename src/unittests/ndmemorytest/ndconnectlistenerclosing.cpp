// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndconnectlistenerclosing.cpp - Test connect to listener that is
// closing/ has closed expecting ND_CONNECTION_REFUSED event


#include "ndmemorytest.h"

void NdConnectListenerClosingServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer + 1, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
    NdTestBase::CreateCQ(nSge);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    //listen
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);

    //close listener
    m_pListen->Release();
    m_pListen = nullptr;
    //wait 5 seconds
    Sleep(5 * 1000);
    printf("NdConnListenClosing: passed\n");
}

void NdConnectListenerClosingClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE, ND_SUCCESS);
    NdTestBase::CreateCQ(nSge, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    //wait 1 second
    Sleep(1000);

    //connect shoud get connection refused, because the other side has closed the listener
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1, nullptr, 0,
        ND_CONNECTION_REFUSED, "Connecting to closed/closing listener should get connection refused");
    printf("NdConnListenClosing: passed\n");
}
