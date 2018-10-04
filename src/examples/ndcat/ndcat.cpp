// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//
// ndcat.cpp - NetworkDirect catalog test
//
// This test checks that the input IP address is a valid NetworkDirect
// address, and that all addresses returned by QueryAddressList are
// valid NetworkDirect addresses.  If there are no addresses the test
// generates an error.

#include "ndcommon.h"
#include <logging.h>

void ShowUsage()
{
    printf("ndcat [options] <IPv4 Address>\n"
        "Options:\n"
        "\t-l,--logFile <logFile>   Log output to a given file\n"
        "\t-h,--help                Show this message\n");
}

int __cdecl _tmain(int argc, TCHAR* argv[])
{
    WSADATA wsaData;
    int ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0)
    {
        printf("Failed to initialize Windows Sockets: %d\n", ret);
        exit(__LINE__);
    }

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

    TCHAR *ipAddress = argv[argc - 1];
    struct sockaddr_in v4 = { 0 };
    int addrLen = sizeof(v4);
    WSAStringToAddress(
        ipAddress,
        AF_INET,
        nullptr,
        reinterpret_cast<struct sockaddr*>(&v4),
        &addrLen
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

    SIZE_T len = 0;
    hr = NdQueryAddressList(0, nullptr, &len);
    if (hr != ND_BUFFER_OVERFLOW)
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdQueryAddressList returned %08x", __LINE__);
    }

    if (len == 0)
    {
        LOG_FAILURE_AND_EXIT(L"NdQueryAddressList returned zero-length list", __LINE__);
    }

    SOCKET_ADDRESS_LIST* pList = static_cast<SOCKET_ADDRESS_LIST *>(HeapAlloc(GetProcessHeap(), 0, len));
    if (pList == nullptr)
    {
        LOG_FAILURE_AND_EXIT(L"Failed to allocate address list", __LINE__);
    }

    printf("Retrieving the full Addresslist");
    hr = NdQueryAddressList(0, pList, &len);
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdQueryAddressList returned %08x", __LINE__);
    }

    // pList is already checked for nullptr
#pragma warning(suppress: 6011)
    if (pList->iAddressCount == 0)
    {
        LOG_FAILURE_AND_EXIT(L"NdQueryAddressList returned zero-length list", __LINE__);
    }

    for (int i = 0; i < pList->iAddressCount; i++)
    {
        hr = NdCheckAddress(pList->Address[i].lpSockaddr, pList->Address[i].iSockaddrLength);
        if (FAILED(hr))
        {
            LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdCheckAddress returned %08x", __LINE__);
        }
    }
    printf("\nValidation of full Addresslist passed");
    hr = NdCleanup();
    if (FAILED(hr))
    {
        LOG_FAILURE_HRESULT_AND_EXIT(hr, L"NdCleanup failed with %08x", __LINE__);
    }
    HeapFree(GetProcessHeap(), 0, pList);

    _fcloseall();
    WSACleanup();
    return 0;
}

