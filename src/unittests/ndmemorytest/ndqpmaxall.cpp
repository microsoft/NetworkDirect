// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndqpmaxall.cpp - Test QP creation with all parameters set to
// maximum advertised, shoud get no error message


#include "ndmemorytest.h"

void NdQPMaxAllServer::RunTest(
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

    //get the max supported depth QP, and receive Sgn number
    ND2_ADAPTER_INFO adaptorInfo;
    adaptorInfo.InfoVersion = ND_VERSION_2;
    ULONG infoSize = sizeof(ND2_ADAPTER_INFO);
    HRESULT hr = m_pAdapter->Query(&adaptorInfo, &infoSize);
    LogIfErrorExit(hr, ND_SUCCESS, "Querying dataptor info failed!", __LINE__);
    queueDepth = adaptorInfo.MaxReceiveQueueDepth;
    nSge = adaptorInfo.MaxReceiveSge;

    //create queue pair, expecting success
    NdTestBase::CreateQueuePair(queueDepth, adaptorInfo.MaxInitiatorQueueDepth, nSge, adaptorInfo.MaxInitiatorSge);

    //tear down
    NdTestBase::Shutdown();
    printf("NdQpMax: passed\n");
}

void NdQPMaxAllClient::RunTest(
    _In_ const struct sockaddr_in& /* v4Src */,
    _In_ const struct sockaddr_in& /*v4Dst*/,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    //do nothing on the client
    printf("NdQpMax is server-only test\n");
}
