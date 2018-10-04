// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndlargeprivatedata.cpp - Test creation of QP that exceeds
// maxReceiveQueueDepth advertised, CreateQueuePair() should fail


#include "ndmemorytest.h"

void NdLargeQPDepthServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD /*nSge*/
)
{
    //prep  
    NdTestBase::Init(v4Src);
    NdTestBase::CreateCQ(2);

    //get the max supported depth QP
    ND2_ADAPTER_INFO adaptorInfo;
    adaptorInfo.InfoVersion = ND_VERSION_2;
    ULONG infoSize = sizeof(ND2_ADAPTER_INFO);
    HRESULT hr = m_pAdapter->Query(&adaptorInfo, &infoSize);
    LogIfErrorExit(hr, ND_SUCCESS, "Querying dataptor info failed!", __LINE__);

    //create QP with depth exceeding max-supported depth
    queueDepth = adaptorInfo.MaxReceiveQueueDepth + 1;

    hr = m_pAdapter->CreateQueuePair(
        IID_IND2QueuePair,
        m_pCq,
        m_pCq,
        nullptr,
        queueDepth,
        1,
        2,
        2,
        0,
        reinterpret_cast<VOID**>(&m_pQp)
    );

    LogIfNoErrorExit(hr, "Receiving queue depth exceeding max value should error!", __LINE__);
    printf("NdLargeQPDepth: passed\n");
}

void NdLargeQPDepthClient::RunTest(
    _In_ const struct sockaddr_in& /* v4Src */,
    _In_ const struct sockaddr_in& /* v4Dst */,
    _In_ DWORD /* queueDepth */,
    _In_ DWORD /* nSge */
)
{
    printf("NdLargeQPDepth is server-only\n");
}
