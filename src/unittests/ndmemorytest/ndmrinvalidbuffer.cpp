// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndmrinvalidbuffer.cpp - Test register memory with buffer = nullptr(server),
// or buffer size= 0(client), expect Get ND_ACCESS_VIOLATION


#include "ndmemorytest.h"

//Try register memory with buffer = nullptr(server), or buffer size= 0(client)
//expect Get ND_ACCESS_VIOLATION

void NdMRInvalidBufferServer::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)

{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();

    //register buffer = nullptr
    HRESULT hr = m_pMr->Register(
        nullptr,
        0,
        ND_MR_FLAG_ALLOW_LOCAL_WRITE,
        &m_Ov
    );
    if (hr == ND_PENDING)
    {
        hr = m_pMr->GetOverlappedResult(&m_Ov, TRUE);
    }

    LogIfErrorExit(hr, ND_INVALID_PARAMETER, "register empty buffer should error", __LINE__);
    printf("NdMRInvalidBuffer: passed\n");
}

//client will try the buffersize<realsize
void NdMRInvalidBufferClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();

    //allocate buffer of size 10
    m_Buf = new (std::nothrow) char[10];
    if (m_Buf == nullptr)
    {
        printf("Failed to allocate buffer.\n");
        exit(__LINE__);
    }

    //register it with size 0
    HRESULT hr = m_pMr->Register(
        m_Buf,
        0,
        ND_MR_FLAG_ALLOW_LOCAL_WRITE,
        &m_Ov
    );
    if (hr == ND_PENDING)
    {
        hr = m_pMr->GetOverlappedResult(&m_Ov, TRUE);
    }

    //expect error message
    LogIfErrorExit(hr, ND_INVALID_BUFFER_SIZE, "register buffer of size 10 given size 20 should error", __LINE__);
    printf("NdMRInvalidBuffer: passed\n");
}
