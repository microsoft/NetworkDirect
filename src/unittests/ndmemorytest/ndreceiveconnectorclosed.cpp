// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndreceiveconnectorclosed.cpp - Test receive with connector closed
//  - Post a receive on local CP and close remote connector
//  - Verify a CQ notification with ND_CANCELED is generated


#include "ndmemorytest.h"

void NdReceiveConnectorClosedServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer + 1, ND_MR_FLAG_ALLOW_LOCAL_WRITE, ND_SUCCESS);
    NdTestBase::CreateCQ(nSge);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    //prepare Sge
    ND2_SGE* Sge = new ND2_SGE[nSge];
    Sge[0].Buffer = m_Buf;
    Sge[0].BufferLength = x_HdrLen + x_MaxXfer;
    Sge[0].MemoryRegionToken = m_pMr->GetLocalToken();

    //Post receive  - should succeed
    NdTestBase::PostReceive(Sge, 1);

    //Accept connection
    NdTestServerBase::Accept(1, 1);

    //sleep 5 seconds to let client close the remote connector
    Sleep(5 * 1000);

    //expect 1 message for the posted receive
    WaitForCompletion(ND_CANCELED);

    //tear down
    NdTestBase::Shutdown();
    printf("NdReceiveConnClosed: passed\n");
}

void NdReceiveConnectorClosedClient::RunTest(
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
    NdTestClientBase::CompleteConnect();

    //shutdown immediately
    NdTestBase::Shutdown();
    printf("NdReceiveConnClosed: passed\n");
}
