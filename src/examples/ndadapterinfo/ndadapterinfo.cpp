// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndadapterinfo.cpp - NetworkDirect AdapterInfo Test
//
// This test checks that the input IP address is a valid NetworkDirect
// address, and that all addresses returned by QueryAddressList are
// valid NetworkDirect addresses.  If there are no addresses the test
// generates an error.

#include "ndcommon.h"
#include <logging.h>
#include <string>

const LPCWSTR TESTNAME = L"ndadapterinfo.exe";

void ShowUsage()
{
    printf("ndadapterinfo [options] <IPv4 Address>\n"
        "Options:\n"
        "\t-l,--logFile <logFile>   Log output to a given file\n"
        "\t-h,--help                Show this message\n");
}

static std::string TranslateAdapterInfoFlags(ULONG flags)
{
    std::string flagsString;
    ULONG allFlags = ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED |
        ND_ADAPTER_FLAG_CQ_INTERRUPT_MODERATION_SUPPORTED |
        ND_ADAPTER_FLAG_MULTI_ENGINE_SUPPORTED |
        ND_ADAPTER_FLAG_CQ_RESIZE_SUPPORTED |
        ND_ADAPTER_FLAG_LOOPBACK_CONNECTIONS_SUPPORTED;

    if (flags & ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED)
    {
        flagsString += "\n\t\tND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED";
    }

    if (flags & ND_ADAPTER_FLAG_CQ_INTERRUPT_MODERATION_SUPPORTED)
    {
        flagsString += "\n\t\tND_ADAPTER_FLAG_CQ_INTERRUPT_MODERATION_SUPPORTED";
    }

    if (flags & ND_ADAPTER_FLAG_MULTI_ENGINE_SUPPORTED)
    {
        flagsString += "\n\t\tND_ADAPTER_FLAG_MULTI_ENGINE_SUPPORTED";
    }

    if (flags & ND_ADAPTER_FLAG_CQ_RESIZE_SUPPORTED)
    {
        flagsString += "\n\t\tND_ADAPTER_FLAG_CQ_RESIZE_SUPPORTED";
    }

    if (flags & ND_ADAPTER_FLAG_LOOPBACK_CONNECTIONS_SUPPORTED)
    {
        flagsString += "\n\t\tND_ADAPTER_FLAG_LOOPBACK_CONNECTIONS_SUPPORTED";
    }

    if ((flags | allFlags) != allFlags)
    {
        ULONG additionalFlags = flags & ~allFlags;
        char tmpBuf[64];
        snprintf(tmpBuf, sizeof(tmpBuf), "\n\t\t%08x", additionalFlags);
        flagsString += tmpBuf;
    }
    return flagsString;
}

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    if (argc < 2)
    {
        ShowUsage();
        return -1;
    }

    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        printf("Failed to initialize Windows Sockets: %d\n", ret);
        exit(__LINE__);
    }

    TCHAR *logFileName = nullptr;
    for (int i = 1; i < argc; i++)
    {
        TCHAR *arg = argv[i];
        if ((wcscmp(arg, L"-l") == 0) || (wcscmp(arg, L"--logFile") == 0))
        {
            RedirectLogsToFile(argv[++i]);
        }
        else if ((wcscmp(arg, L"-h") == 0) || (wcscmp(arg, L"--help") == 0))
        {
            ShowUsage();
            exit(0);
        }
    }

    TCHAR *ipAddress = argv[argc -1];
    struct sockaddr_in v4 = { 0 };
    INT len = sizeof(v4);
    WSAStringToAddress(
        ipAddress,
        AF_INET,
        nullptr,
        reinterpret_cast<struct sockaddr*>(&v4),
        &len
    );

    if (v4.sin_addr.s_addr == 0)
    {
        printf("Bad address.\n");
        ShowUsage();
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdStartup failed with %08x", __LINE__);
    }

    hr = NdCheckAddress(reinterpret_cast<const struct sockaddr*>(&v4), sizeof(v4));
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdCheckAddress for input address returned %08x", __LINE__);
    }

    IND2Adapter *pAdapter = nullptr;
    hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr*>(&v4),
        sizeof(v4),
        reinterpret_cast<void**>(&pAdapter)
    );
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"Failed open adapter: %08x\n", __LINE__);
    }

    ND2_ADAPTER_INFO adapterInfo = { 0 };
    adapterInfo.InfoVersion = ND_VERSION_2;
    ULONG adapterInfoSize = sizeof(adapterInfo);

    hr = pAdapter->Query(&adapterInfo, &adapterInfoSize);
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"IND2Adapter::GetAdapterInfo failed: %08x", __LINE__);
    }

    printf("InfoVersion: %lu\n", adapterInfo.InfoVersion);
    printf("VendorId: %u\n", adapterInfo.VendorId);
    printf("DeviceId: %u\n", adapterInfo.DeviceId);
    printf("AdapterId: 0x%08llx\n", adapterInfo.AdapterId);
    printf("MaxRegistrationSize: %zu\n", adapterInfo.MaxRegistrationSize);
    printf("MaxWindowSize: %zu\n", adapterInfo.MaxWindowSize);
    printf("MaxInitiatorSge: %lu\n", adapterInfo.MaxInitiatorSge);
    printf("MaxReceiveSge: %lu\n", adapterInfo.MaxReceiveSge);
    printf("MaxReadSge: %lu\n", adapterInfo.MaxReadSge);
    printf("MaxTransferLength: %lu\n", adapterInfo.MaxTransferLength);
    printf("MaxInlineDataSize: %lu\n", adapterInfo.MaxInlineDataSize);
    printf("MaxInboundReadLimit: %lu\n", adapterInfo.MaxInboundReadLimit);
    printf("MaxOutboundReadLimit: %lu\n", adapterInfo.MaxOutboundReadLimit);
    printf("MaxReceiveQueueDepth: %lu\n", adapterInfo.MaxReceiveQueueDepth);
    printf("MaxInitiatorQueueDepth: %lu\n", adapterInfo.MaxInitiatorQueueDepth);
    printf("MaxSharedReceiveQueueDepth: %lu\n", adapterInfo.MaxSharedReceiveQueueDepth);
    printf("MaxCompletionQueueDepth: %lu\n", adapterInfo.MaxCompletionQueueDepth);
    printf("InlineRequestThreshold: %lu\n", adapterInfo.InlineRequestThreshold);
    printf("LargeRequestThreshold: %lu\n", adapterInfo.LargeRequestThreshold);
    printf("MaxCallerData: %lu\n", adapterInfo.MaxCallerData);
    printf("MaxCalleeData: %lu\n", adapterInfo.MaxCalleeData);
    printf("AdapterFlags: %s\n", TranslateAdapterInfoFlags(adapterInfo.AdapterFlags).c_str());

    pAdapter->Release();
    hr = NdCleanup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdCleanup failed with %08x", __LINE__);
    }

    _fcloseall();
    WSACleanup();
    return 0;
}

