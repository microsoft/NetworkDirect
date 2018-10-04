// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndoverreadwrite.cpp - Test server read/write more than
// the size of its buffer


#include "ndmemorytest.h"

void NdOverReadServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer + 1, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

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
    UINT64 addr = ndmd->base;
    UINT32 token = ndmd->token;

    //prepare buffer to read/write

    //read more than the remote peer has
    Sge.BufferLength = x_HdrLen + x_MaxXfer + 1;

    //read
    NdTestBase::Read(&Sge, 1, addr, token, 0);

    //wait for read completion
    NdTestBase::WaitForCompletion(
        ND_ACCESS_VIOLATION,
        "Read from memory region more its size should get error ND_ACCESS_VIOLATION"
    );

    //tear down
    NdTestBase::Shutdown();
    printf("NdOverRead: passed\n");
}

void NdOverWriteServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{

    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_HdrLen + x_MaxXfer + 1, ND_MR_FLAG_ALLOW_LOCAL_WRITE);

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
    UINT64 addr = ndmd->base;
    UINT32 token = ndmd->token;

    //prepare buffer to read/write

    //write more than the remote peer has
    Sge.BufferLength = x_HdrLen + x_MaxXfer + 1;

    //write
    NdTestBase::Write(&Sge, 1, addr, token, 0);

    //wait for write completion
    NdTestBase::WaitForCompletion(
        ND_ACCESS_VIOLATION,
        "Write to memory region when it's being invalidated should result in error"
    );

    NdTestBase::Shutdown();
    printf("NdOverWrite: passed\n");
}

void NdOverReadWriteClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE);

    NdTestBase::CreateCQ(2, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(2, 1);
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1);
    NdTestClientBase::CompleteConnect();
    NdTestBase::CreateMW();
    NdTestBase::Bind(x_MaxXfer, ND_OP_FLAG_ALLOW_WRITE | ND_OP_FLAG_ALLOW_READ);

    //prepare memory descriptor
    MemoryWindowDesc* ndmd = (MemoryWindowDesc*)m_Buf;
    ndmd->base = (UINT64)m_Buf;
    ndmd->token = m_pMw->GetRemoteToken();
    ndmd->length = x_MaxXfer;

    //prepare Sge
    ND2_SGE Sge;
    Sge.Buffer = ndmd;
    Sge.BufferLength = sizeof(*ndmd);
    Sge.MemoryRegionToken = m_pMr->GetLocalToken();

    //send it over
    NdTestBase::Send(&Sge, 1, 0);

    //Wait for Send completion
    NdTestBase::WaitForCompletion();

    //wait 5 seconds
    Sleep(5 * 1000);

    //tear down
    NdTestBase::Shutdown();
    printf("%S: passed\n", m_testName);
}
