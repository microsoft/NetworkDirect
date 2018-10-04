// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmrderegister.cpp - Test memory region deregisteration when memory
// window is bound to it

#include "ndmemorytest.h"

//test cases for Memory region deregisteration when memory window is bound to it
void NdMRDeregisterServer::RunTest(
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

    //connect
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    //accept connection			
    NdTestServerBase::Accept(1, 1);

    //create MW and bind it to memory
    NdTestBase::CreateMW();
    NdTestBase::Bind(x_MaxXfer, 0);

    //this call should fail, because there's still memory window bound to it
    HRESULT hr = m_pMr->Deregister(&m_Ov);
    LogIfErrorExit(hr, ND_DEVICE_BUSY,
        "Deregistering memory region with memory window bound it should get ND_DEVICE_BUSY!", __LINE__);

    //tear down
    NdTestBase::Shutdown();
    printf("NdMRDeregister: passed\n");
}

void NdMRDeregisterClient::RunTest(
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
    NdTestClientBase::CompleteConnect();

    //sleep 5 seconds to let the other end deregister memory
    Sleep(5 * 1000);

    NdTestBase::Shutdown();
    printf("NdMRDeregister: passed\n");
}