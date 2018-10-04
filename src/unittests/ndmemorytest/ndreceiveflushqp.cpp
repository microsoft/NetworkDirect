// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndreceiveflushqp.cpp - Test Flush QP
//  - Post two receives on local QP and flush the QP. 
//  - Verify one CQ notification is generated
//  - Verify the next two receive requests completed with cancelled status
//    due to QP flush


#include "ndmemorytest.h"

void NdReceiveFlushQPServer::RunTest(
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

    // Pre-post 2 receive requests.
    NdTestBase::PostReceive(nullptr, 0);
    NdTestBase::PostReceive(nullptr, 0);

    //flush QP
    NdTestBase::FlushQP();

    //Expect 1 generated notification due to flush
    ND2_RESULT pResult[2];
    HRESULT hr = m_pCq->Notify(ND_CQ_NOTIFY_ANY, &m_Ov);
    if (hr == ND_PENDING)
    {
        hr = m_pCq->GetOverlappedResult(&m_Ov, TRUE);
    }

    //expecting two results,both being ND_CANCELED
    int numResults = m_pCq->GetResults(pResult, 2);
    if (numResults != 2) {
        printf("Expecting 2 results, but only getting %d", numResults);
        LogErrorExit("Error in CompletionQueue::GetResults", __LINE__);
    }
    else
    {
        LogIfErrorExit(pResult[0].Status, ND_CANCELED,
            "Expecting receive cancelled due to QP flush", __LINE__);
        LogIfErrorExit(pResult[1].Status, ND_CANCELED,
            "Expecting receive cancelled due to QP flush", __LINE__);
    }
    NdTestBase::Shutdown();
    printf("NdReceiveFlushQP: passed\n");
}

void NdReceiveFlushQPClient::RunTest(
    _In_ const struct sockaddr_in& /*v4Src*/,
    _In_ const struct sockaddr_in& /*v4Dst*/,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //do nothing on the client
    printf("NdReceiveFlushQP is server-only\n");
}

