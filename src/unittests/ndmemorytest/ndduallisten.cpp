// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// nddualisten.cpp - Open two adaptors listening to the same endpoint,
// second listen should fail

#include "ndmemorytest.h"
#include <logging.h>

void NdDualListenServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);

    //open another adaptor listening to the same endpoint
    IND2Adapter* pAdapter;
    IND2Listener* pListen;
    HRESULT hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&v4Src),
        sizeof(v4Src),
        reinterpret_cast<void**>(&pAdapter)
    );
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Failed to open adapter", __LINE__);
    }

    hr = m_pAdapter->CreateListener(
        IID_IND2Listener,
        m_hAdapterFile,
        reinterpret_cast<VOID**>(&pListen)
    );
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Failed to create listener", __LINE__);
    }

    hr = m_pListen->Bind(
        reinterpret_cast<const sockaddr*>(&v4Src),
        sizeof(v4Src)
    );
    //second bind should fail
    LogIfErrorExit(hr, STATUS_ADDRESS_ALREADY_ASSOCIATED, "Second listen should fail", __LINE__);

    hr = m_pListen->Listen(0);
    if (hr == ND_SUCCESS)
    {
        LogErrorExit("Expected second listen to fail, but succeeded\n", __LINE__);
    }

    printf("NdDualListen: passed\n");
}

void NdDualListenClient::RunTest(
    _In_ const struct sockaddr_in& /*v4Src*/,
    _In_ const struct sockaddr_in& /*v4Dst*/,
    _In_ DWORD /*queueDepth*/,
    _In_ DWORD /*nSge*/
)
{
    printf("NdDualListen is server-only test\n");
}
