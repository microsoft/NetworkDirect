// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndsendnoreceive.cpp - Test send with no posted recv
//  - Post a send on local QP without posting receive on remote QP. 
//  - Verify a CQ notification is generated


#include "ndmemorytest.h"
#include <logging.h>

void NdSendNoReceiveServer::RunTest(
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

    // accept connection
    NdTestServerBase::Accept(1, 1);

    // sleep 5 seconds
    Sleep(5 * 1000);

    // we are not expecting any CQ notification, because a connection is not established
    ND2_RESULT ndResult;
    if (m_pCq->GetResults(&ndResult, 1) != 0)
    {
        LOG_FAILURE_AND_EXIT(L"Unexpected completion\n", __LINE__);
    }

    //tear down
    NdTestBase::Shutdown();
    printf("NdSendNoReceive: passed\n");
}


void NdSendNoReceiveClient::RunTest(
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
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);
    NdTestClientBase::CompleteConnect();

    //prepare Sge
    ND2_SGE Sge[1];
    Sge[0].Buffer = m_Buf;
    Sge[0].BufferLength = x_MaxXfer;
    Sge[0].MemoryRegionToken = m_pMr->GetLocalToken();

    //send it over
    NdTestBase::Send(Sge, 1, 0);

    // There's no receive posted on the other side, the test case should not
    // wait indefinitely because we should get a notification in the CQ.

    //expect failure to of the send in the completion queue
    NdTestBase::WaitForCompletion(ND_IO_TIMEOUT);
    NdTestBase::Shutdown();
    printf("NdSendNoReceive: passed\n");
}
