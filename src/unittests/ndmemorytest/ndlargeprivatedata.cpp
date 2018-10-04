// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndlargeprivatedata.cpp - Test Connect/Accept with the length of private
// data exceeding the limit of the hardware, verify with ND_INVALID_BUFFER_SIZE

#include "ndmemorytest.h"

void NdLargePrivateDataServer::RunTest(
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
    NdTestServerBase::CreateListener();
    NdTestServerBase::Listen(v4Src);
    NdTestServerBase::GetConnectionRequest();

    // Pre-post receive request.
    NdTestBase::PostReceive(nullptr, 0);

    //get the max supported length of private data on
    ND2_ADAPTER_INFO adaptorInfo;
    adaptorInfo.InfoVersion = ND_VERSION_2;
    ULONG infoSize = sizeof(ND2_ADAPTER_INFO);
    HRESULT hr = m_pAdapter->Query(&adaptorInfo, &infoSize);
    LogIfErrorExit(hr, ND_SUCCESS, "Querying dataptor info failed!", __LINE__);
    DWORD maxCalleeData = adaptorInfo.MaxCalleeData;


    // give it the correct private data size/buffer size, but larger than what the hardware can support
    void* data = new (std::nothrow) char[maxCalleeData + 1LL];
    NdTestServerBase::Accept(1, 1, data, maxCalleeData + 1,
        ND_INVALID_BUFFER_SIZE, "Expecting Access violation when data exceeds MaxCalleeData");
    delete[] data;

    //try with just enough data
    data = new (std::nothrow) char[maxCalleeData];
    NdTestServerBase::Accept(1, 1, data, maxCalleeData,
        ND_SUCCESS, "Expecting ND_SUCCESS when privata data size is just MaxCalleeData");

    //Get result for the pre-posted receive
    NdTestBase::WaitForCompletion(ND_SUCCESS);

    //tear down
    NdTestBase::Shutdown();
    delete[] data;
    printf("NdLargePrivateData: passed\n");
}

void NdLargePrivateDataClient::RunTest(
    _In_ const struct sockaddr_in& v4Src,
    _In_ const struct sockaddr_in& v4Dst,
    _In_ DWORD queueDepth,
    _In_ DWORD nSge
)
{
    //prep
    NdTestBase::Init(v4Src);
    NdTestBase::CreateMR();
    NdTestBase::RegisterDataBuffer(x_MaxXfer, ND_MR_FLAG_ALLOW_REMOTE_READ | ND_MR_FLAG_ALLOW_REMOTE_WRITE);

    NdTestBase::CreateCQ(nSge, ND_SUCCESS);
    NdTestBase::CreateConnector();
    NdTestBase::CreateQueuePair(queueDepth, nSge);

    //get the max supported length of private data
    ND2_ADAPTER_INFO adaptorInfo;
    adaptorInfo.InfoVersion = ND_VERSION_2;
    ULONG infoSize = sizeof(ND2_ADAPTER_INFO);
    HRESULT hr = m_pAdapter->Query(&adaptorInfo, &infoSize);
    LogIfErrorExit(hr, ND_SUCCESS, "Querying dataptor info failed!", __LINE__);
    DWORD maxCallerData = adaptorInfo.MaxCallerData;

    //attempt to connect with larger private data
    const void* data = new (std::nothrow) char[maxCallerData + 1LL];
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1, data, maxCallerData + 1,
        ND_INVALID_BUFFER_SIZE, "Expecting Access violation when data size is bigger than maxCallerData");
    delete[] data;

    //try with just enough data, should get ND_SUCCESS
    data = new (std::nothrow) char[maxCallerData];
    NdTestClientBase::Connect(v4Src, v4Dst, 1, 1, data, maxCallerData,
        ND_SUCCESS, "Expecting ND_SUCCESS when data size is just maxCallerData");
    delete[] data;

    //complete connect event
    NdTestClientBase::CompleteConnect();

    //send a packet to signal tear down
    NdTestBase::Send(nullptr, 0, 0);
    NdTestBase::WaitForCompletion();

    //tear down
    NdTestBase::Shutdown();
    printf("NdLargePrivateData: passed\n");
}
