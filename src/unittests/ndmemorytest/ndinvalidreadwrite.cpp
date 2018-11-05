// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndinvalidip.cpp - Test read/write to invalidates memory window


#include "ndmemorytest.h"

void NdInvalidReadServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
        
    NdTestBase::CreateCQ(2);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(1, 1);
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    // Pre-post receive request.
    ND2_SGE Sge;
    const MemoryWindowDesc* ndmd = reinterpret_cast<const MemoryWindowDesc*>(m_Buf);
    Sge.Buffer = m_Buf;
    Sge.BufferLength = sizeof(*ndmd);
    Sge.MemoryRegionToken = m_pMr->GetLocalToken();
    NdTestBase::PostReceive(&Sge, 1);

    //accept connection
    NdTestServerBase::Accept(1, 1);

    //Get result for the pre-posted receive
    NdTestBase::WaitForCompletion(ND_SUCCESS);
    //get remote address and token
    UINT64 addr = ndmd->base;
    UINT32 token = ndmd->token;

    //Wait 5 seconds for memory window to be invalidated
    Sleep(5 * 1000);

    //try to read from the remote MW, should get error
    Sge.BufferLength = x_HdrLen + x_MaxXfer + 1;
    NdTestBase::Read(&Sge, 1, addr, token, 0);
    //wait for read completion, expecting error
    NdTestBase::WaitForCompletion(
        ND_ACCESS_VIOLATION,
        "Read from memory window when it's being invalidated should result in error"
    );

    //tear down
    NdTestBase::Shutdown();
    printf("NdInvalidRead: passed\n");
}

void NdInvalidWriteServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

    NdTestBase::CreateCQ(2);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(1, 1);
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    // Pre-post receive request.
    ND2_SGE Sge;
    const MemoryWindowDesc* ndmd = reinterpret_cast<const MemoryWindowDesc*>(m_Buf);
    Sge.Buffer = m_Buf;
    Sge.BufferLength = sizeof(*ndmd);
    Sge.MemoryRegionToken = m_pMr->GetLocalToken();
    NdTestBase::PostReceive(&Sge, 1);

    //accept connection
    NdTestServerBase::Accept(1, 1);

    //Get result for the pre-posted receive
    NdTestBase::WaitForCompletion(ND_SUCCESS);

    //get remote address and token
    UINT64 addr = ndmd->base;
    UINT32 token = ndmd->token;

    //Wait 5 seconds for memory window to be invalidated
    Sleep(5 * 1000);

    //try to write to the remote MW, should get error
    Sge.BufferLength = x_HdrLen + x_MaxXfer + 1;
    NdTestBase::Write(&Sge, 1, addr, token, 0);
    //wait for write completion,expecting error
    NdTestBase::WaitForCompletion(
        ND_ACCESS_VIOLATION,
        "Write to memory window when it's being invalidated should result in error"
    );

    //tear down
    NdTestBase::Shutdown();
    printf("NdInvalidWrite: passed\n");
}


void NdInvalidReadWriteClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer,
        ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE);

    NdTestBase::CreateCQ(2, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(2, 1);
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);
    NdTestClientBase::CompleteConnect();
    NdTestBase::CreateMW();
    NdTestBase::Bind(x_MaxXfer, ND_OP_FLAG_ALLOW_WRITE | ND_OP_FLAG_ALLOW_READ);

    //create memory descriptor
    MemoryWindowDesc* ndmd = (MemoryWindowDesc*)m_Buf;
    ndmd->base = (UINT64)m_Buf;
    ndmd->token = m_pMw->GetRemoteToken();
    ndmd->length = x_MaxXfer;

    //prepare Sge
    ND2_SGE Sge;
    Sge.Buffer = ndmd;
    Sge.BufferLength = sizeof(*ndmd);
    Sge.MemoryRegionToken = m_pMr->GetLocalToken();

    //post receive to capture connection teardown
    NdTestBase::PostReceive(nullptr, 0);

    //send it over
    NdTestBase::Send(&Sge, 1, 0);

    //wait for send completion
    NdTestBase::WaitForCompletion();

    //invalidate memory window
    NdTestBase::InvalidateMW();

    //wait for invalidation completion
    NdTestBase::WaitForCompletion();

    //wait for receive be flushed
    NdTestBase::WaitForCompletion(
        ND_CANCELED,
        "Receive should have been cancelled because the peer is writing to invalid MW"
    );

    printf("%S: passed\n", m_testName);
}
