// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndinvalidip.cpp - Test connector using invalid IP address


#include "ndmemorytest.h"
#include <ntstatus.h>

void NdInvalidIPServer::RunTest(
    _In_ const struct sockaddr_in& /*v4Src*/,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    printf("NdInvalidIP is client-only test\n");
}

void NdInvalidIPClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    // prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
    NdTestBase::CreateCQ(nSge, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    // connect to invalid ip address (address has been changed outside)
    NdTestClientBase::Connect(v4Src, v4Dst, 0, 0, nullptr, 0, STATUS_BAD_NETWORK_NAME,
        "Expecting STATUS_BAD_NETWORK_NAME when destination IP is invalid");
    printf("NdInvalidIP: passed\n");
}
