// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndconnectlistenerclosing.cpp - Test client reject connection
// after server accepts connection. Client get ND_SUCCESS, and
// server get ND_CONNECTION_REFUSED in CQ


#include "ndmemorytest.h"

void NdConnRejectCloseServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
    NdTestBase::CreateCQ(nSge);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    //accept connection
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    //expect connection refused, because the other side has rejected the connection
    NdTestServerBase::Accept(0, 0, nullptr, 0, ND_CONNECTION_REFUSED, "Expecting ND_CONNECTION_REFUSED in CQ");

    //tear down
    NdTestBase::Shutdown();
    printf("%S: passed\n", m_testName);
}

void NdConnRejectClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep    
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
    NdTestBase::CreateCQ(nSge, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    //connect
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);

    //rejcts connection
    NdTestBase::Reject(nullptr, 0);

    //Wait 5 seconds
    Sleep(5 * 1000);

    //tear down
    NdTestBase::Shutdown();
    printf("NdConnReject: passed\n");
}

void NdConnCloseClient::RunTest(
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

    //connect
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);

    //close connector by releasing it
    HRESULT hr = m_pConnector->Release();
    LogIfErrorExit(hr, ND_SUCCESS, "Closing connector should return ND_SCUCESS", __LINE__);
    m_pConnector = nullptr;
    //Wait 5 seconds
    Sleep(5 * 1000);
    printf("NdConnClose: passed\n");
}