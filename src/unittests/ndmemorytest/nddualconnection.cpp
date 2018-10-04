// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndconnectlistenerclosing.cpp - Test connect when another connect
// event is in progress. Client should get ND_CONNECTION_ACTIVE event.


#include "ndmemorytest.h"

void NdDualConnectionServer::RunTest(
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

    //establish connection
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    //accept connection, expecting to be refused, since the peer has corrupted it
    NdTestServerBase::Accept(1, 1, nullptr, 0, STATUS_CONNECTION_REFUSED,
        "Expecting remote peer to refuse connection");
    printf("NdDualConnection: passed\n");
}

void NdDualConnectionClient::RunTest(
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

    //do first connection
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);

    //try another connection, should get ND_CONNECTION_ACTIVE
    HRESULT hr = m_pConnector->Bind(
        reinterpret_cast<const sockaddr*>(&v4Src),
        sizeof(v4Src)
    );
    LogIfErrorExit(hr, STATUS_ADDRESS_ALREADY_ASSOCIATED,
        "Another connection is in progress, current connection should get ND_CONNECTION_ACTIVE", __LINE__);

    //wait 5 seconds
    Sleep(5 * 1000);

    hr = m_pConnector->Connect(m_pQp, reinterpret_cast<const sockaddr*>(&v4Dst), sizeof(v4Dst),
        0, 0, nullptr, 0, &m_Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pConnector->GetOverlappedResult(&m_Ov, TRUE);
    }
    LogIfErrorExit(hr, ND_CONNECTION_ACTIVE,"Unexpected event, expected ND_CONNECTION_ACTIVE", __LINE__);
    printf("NdDualConnection: passed\n");
}
