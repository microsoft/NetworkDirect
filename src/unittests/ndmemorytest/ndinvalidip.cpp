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
    HRESULT hr;
    // prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_LOCAL_WRITE);
    NdTestBase::CreateCQ(queueDepth*2, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    // connect to invalid ip address (address has been changed outside)
    hr = NdTestClientBase::Connect(v4Src, v4Dst, 0, 0, FALSE);

    /*
      The NetworkDirect Service Provider Interface Version 2 specification says that
      ND_NETWORK_UNREACHABLE should be returned. Some vendors' NDSPI providers return
      STATUS_BAD_NETWORK_NAME instead so accept both values.
    */

    if ((hr != STATUS_BAD_NETWORK_NAME) && (hr != ND_NETWORK_UNREACHABLE))
    {
        LogErrorExit(hr, "Expecting STATUS_BAD_NETWORK_NAME or ND_NETWORK_UNREACHABLE when destination IP is invalid", __LINE__);
    }

    printf("NdInvalidIP: passed\n");
}
