//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

#include "ndmemorytest.h"
#include "logging.h"

enum TestName
{
    ND_CONN_CLOSE_TEST,
    ND_CONN_LISTEN_CLOSING_TEST,
    ND_CONN_REJECT_TEST,
    ND_DUAL_CONECTION_TEST,
    ND_DUAL_LISTEN_TEST,
    ND_INVALID_IP_TEST,
    ND_INVALID_READ_TEST,
    ND_INVALID_WRITE_TEST,
    ND_LARGE_PRIVATEDATA_TEST,
    ND_LARGE_QP_TEST,
    ND_MR_DEREGISTER_TEST,
    ND_MR_INVALID_BUFFER_TEST,
    ND_OVER_READ_TEST,
    ND_OVER_WRITE_TEST,
    ND_QP_MAX_TEST,
    ND_RECEIVE_FLUSHQP_TEST,
    ND_SEND_NO_RECEIVE_TEST,
    ND_RECEIVE_CONN_CLOSED_TEST,
    ND_WRITE_VIOLATION_TEST,
    ND_TEST_ERROR
};

void ShowUsage()
{
    printf("ndtest [options] <ip>[:<port>]\n"
        "Options:\n"
        "\t-s            - Start as server (listen on IP/Port)\n"
        "\t-c            - Start as client (connect to server IP/Port)\n"
        "\t-b            - Blocking I/O (wait for CQ notification)\n"
        "\t-p            - Polling I/O (poll on the CQ) (default)\n"
        "\t-n <nSge>     - Number of scatter/gather entries per transfer (default: 1)\n"
        "\t-q <pipeline> - Pipeline limit of <pipeline> requests\n"
        "\t-l <logFile>  - Log output to a file named <logFile>\n"
        "\t-t <testName> - Run <testName>, <testName> can be one of the following:\n"
        "\t                 - NdConnClose\n"
        "\t                 - NdConnListenClosing\n"
        "\t                 - NdConnReject\n"
        "\t                 - NdDualConnection\n"
        "\t                 - NdDualListen\n"
        "\t                 - NdInvalidIP\n"
        "\t                 - NdInvalidRead\n"
        "\t                 - NdInvalidWrite\n"
        "\t                 - NdLargePrivateData\n"
        "\t                 - NdLargeQPDepth\n"
        "\t                 - NdMRDeregister\n"
        "\t                 - NdMRInvalidBuffer\n"
        "\t                 - NdOverRead\n"
        "\t                 - NdOverWrite\n"
        "\t                 - NdQpMax\n"
        "\t                 - NdReceiveFlushQP\n"
        "\t                 - NdSendNoReceive\n"
        "\t                 - NdReceiveConnClosed\n"
        "\t                 - NdWriteViolation\n"
        "<ip>            - IPv4 Address\n"
        "<port>          - Port number, (default: %hu)\n",
        x_DefaultPort
    );
}

TestName ParseTestName(const TCHAR* testName)
{
    if (_tcsicmp(testName, _T("NdConnClose")) == 0)
    {
        return ND_CONN_CLOSE_TEST;
    }

    if (_tcsicmp(testName, _T("NdConnListenClosing")) == 0)
    {
        return ND_CONN_LISTEN_CLOSING_TEST;
    }

    if (_tcsicmp(testName, _T("NdConnReject")) == 0)
    {
        return ND_CONN_REJECT_TEST;
    }

    if (_tcsicmp(testName, _T("NdDualConnection")) == 0)
    {
        return ND_DUAL_CONECTION_TEST;
    }

    if (_tcsicmp(testName, _T("NdDualListen")) == 0)
    {
        return ND_DUAL_LISTEN_TEST;
    }

    if (_tcsicmp(testName, _T("NdInvalidIP")) == 0)
    {
        return ND_INVALID_IP_TEST;
    }

    if (_tcsicmp(testName, _T("NdInvalidRead")) == 0)
    {
        return ND_INVALID_READ_TEST;
    }

    if (_tcsicmp(testName, _T("NdInvalidWrite")) == 0)
    {
        return ND_INVALID_WRITE_TEST;
    }

    if (_tcsicmp(testName, _T("NdLargePrivateData")) == 0)
    {
        return ND_LARGE_PRIVATEDATA_TEST;
    }

    if (_tcsicmp(testName, _T("NdLargeQPDepth")) == 0)
    {
        return ND_LARGE_QP_TEST;
    }

    if (_tcsicmp(testName, _T("NdMRDeregister")) == 0)
    {
        return ND_MR_DEREGISTER_TEST;
    }

    if (_tcsicmp(testName, _T("NdMRInvalidBuffer")) == 0)
    {
        return ND_MR_INVALID_BUFFER_TEST;
    }

    if (_tcsicmp(testName, _T("NdOverRead")) == 0)
    {
        return ND_OVER_READ_TEST;
    }

    if (_tcsicmp(testName, _T("NdOverWrite")) == 0)
    {
        return ND_OVER_WRITE_TEST;
    }

    if (_tcsicmp(testName, _T("NdQpMax")) == 0)
    {
        return ND_QP_MAX_TEST;
    }

    if (_tcsicmp(testName, _T("NdReceiveFlushQP")) == 0)
    {
        return ND_RECEIVE_FLUSHQP_TEST;
    }

    if (_tcsicmp(testName, _T("NdSendNoReceive")) == 0)
    {
        return ND_SEND_NO_RECEIVE_TEST;
    }

    if (_tcsicmp(testName, _T("NdReceiveConnClosed")) == 0)
    {
        return ND_RECEIVE_CONN_CLOSED_TEST;
    }

    if (_tcsicmp(testName, _T("NdWriteViolation")) == 0)
    {
        return ND_WRITE_VIOLATION_TEST;
    }

    return ND_TEST_ERROR;
}

int __cdecl  _tmain(int argc, TCHAR* argv[])
{
    bool bServer = false, bClient = false;
    bool bBlocking = false, bPolling = false;
    DWORD nSge = 2, nPipeline = 64;

    struct sockaddr_in v4Server = { 0 };
    TestName testName = ND_INVALID_READ_TEST;
    const TCHAR *testNameStr = nullptr;

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
        if (_wcsicmp(arg, L"-s") == 0)
        {
            bServer = true;
        }
        else if (_wcsicmp(arg, L"-c") == 0)
        {
            bClient = true;
        }
        else if (_wcsicmp(arg, L"-p") == 0)
        {
            bPolling = true;
        }
        else if (_wcsicmp(arg, L"-b") == 0)
        {
            bBlocking = true;
        }
        else if (_wcsicmp(arg, L"-n") == 0)
        {
            if (i == argc - 2)
            {
                ShowUsage();
                exit(-1);
            }
            nSge = _ttol(argv[++i]);
        }
        else if (_wcsicmp(arg, L"-q") == 0)
        {
            if (i == argc - 2)
            {
                ShowUsage();
                exit(-1);
            }
            nPipeline = _ttol(argv[++i]);
        }
        else if (_wcsicmp(arg, L"-l") == 0)
        {
            RedirectLogsToFile(argv[++i]);
        }
        else if (_wcsicmp(arg, L"-t") == 0)
        {
            testNameStr = argv[++i];
            testName = ParseTestName(testNameStr);
        }
        else if ((_wcsicmp(arg, L"-h") == 0) || (_wcsicmp(arg, L"--help") == 0))
        {
            ShowUsage();
            exit(0);
        }
    }

    // ip address is last parameter
    int len = sizeof(v4Server);
    WSAStringToAddress(argv[argc - 1], AF_INET, nullptr,
        reinterpret_cast<struct sockaddr*>(&v4Server), &len);

    if ((bClient && bServer) || (!bClient && !bServer))
    {
        printf("Exactly one of client (c or "
            "server (s) must be specified.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (v4Server.sin_addr.s_addr == 0)
    {
        printf("Bad address.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    if (v4Server.sin_port == 0)
    {
        v4Server.sin_port = htons(x_DefaultPort);
    }

    if (testNameStr == nullptr)
    {
        printf("Test name not specified.\n\n");
        ShowUsage();
        exit(__LINE__);
    }

    OVERLAPPED Ov;
    Ov.hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (Ov.hEvent == nullptr)
    {
        printf("Failed to allocate event for overlapped operations.\n");
        exit(__LINE__);
    }

    HRESULT hr = NdStartup();
    if (FAILED(hr))
    {
        printf("NdStartup failed with %08x\n", hr);
        exit(__LINE__);
    }
    NdTestServerBase* server;
    NdTestClientBase* client;
    switch (testName)
    {
    case ND_CONN_CLOSE_TEST:
        server = new(std::nothrow) NdConnRejectCloseServer(testNameStr);
        client = new(std::nothrow) NdConnCloseClient;
        break;

    case ND_CONN_LISTEN_CLOSING_TEST:
        server = new(std::nothrow) NdConnectListenerClosingServer;
        client = new(std::nothrow) NdConnectListenerClosingClient;
        break;

    case ND_CONN_REJECT_TEST:
        server = new(std::nothrow) NdConnRejectCloseServer(testNameStr);
        client = new(std::nothrow) NdConnRejectClient;
        break;

    case ND_DUAL_CONECTION_TEST:
        server = new(std::nothrow) NdDualConnectionServer;
        client = new(std::nothrow) NdDualConnectionClient;
        break;

    case ND_DUAL_LISTEN_TEST:
        server = new(std::nothrow) NdDualListenServer;
        client = new(std::nothrow) NdDualListenClient;
        break;

    case ND_INVALID_IP_TEST:
        server = new(std::nothrow) NdInvalidIPServer;
        client = new(std::nothrow) NdInvalidIPClient;
        break;

    case ND_INVALID_READ_TEST:
        server = new(std::nothrow) NdInvalidReadServer;
        client = new(std::nothrow) NdInvalidReadWriteClient(testNameStr);
        break;

    case ND_INVALID_WRITE_TEST:
        server = new(std::nothrow) NdInvalidWriteServer;
        client = new(std::nothrow) NdInvalidReadWriteClient(testNameStr);
        break;

    case ND_LARGE_PRIVATEDATA_TEST:
        server = new(std::nothrow) NdLargePrivateDataServer;
        client = new(std::nothrow) NdLargePrivateDataClient;
        break;

    case ND_LARGE_QP_TEST:
        server = new(std::nothrow) NdLargeQPDepthServer;
        client = new(std::nothrow) NdLargeQPDepthClient;
        break;

    case ND_MR_DEREGISTER_TEST:
        server = new(std::nothrow) NdMRDeregisterServer;
        client = new(std::nothrow) NdMRDeregisterClient;
        break;

    case ND_MR_INVALID_BUFFER_TEST:
        server = new(std::nothrow) NdMRInvalidBufferServer;
        client = new(std::nothrow) NdMRInvalidBufferClient;
        break;

    case ND_OVER_READ_TEST:
        server = new(std::nothrow) NdOverReadServer;
        client = new(std::nothrow) NdOverReadWriteClient(testNameStr);
        break;

    case ND_OVER_WRITE_TEST:
        server = new(std::nothrow) NdOverWriteServer;
        client = new(std::nothrow) NdOverReadWriteClient(testNameStr);
        break;

    case ND_QP_MAX_TEST:
        server = new(std::nothrow) NdQPMaxAllServer;
        client = new(std::nothrow) NdQPMaxAllClient;
        break;

    case ND_RECEIVE_FLUSHQP_TEST:
        server = new(std::nothrow) NdReceiveFlushQPServer;
        client = new(std::nothrow) NdReceiveFlushQPClient;
        break;

    case ND_SEND_NO_RECEIVE_TEST:
        server = new(std::nothrow) NdSendNoReceiveServer;
        client = new(std::nothrow) NdSendNoReceiveClient;
        break;

    case ND_RECEIVE_CONN_CLOSED_TEST:
        server = new(std::nothrow) NdReceiveConnectorClosedServer;
        client = new(std::nothrow) NdReceiveConnectorClosedClient;
        break;

    case ND_WRITE_VIOLATION_TEST:
        server = new(std::nothrow) NdWriteViolationServer;
        client = new(std::nothrow) NdWriteViolationClient;
        break;

    default:
        printf("Bad Test ID.\n\n");
        exit(__LINE__);
    }

    if (server == nullptr || client == nullptr)
    {
        printf("Memory allocation failed.\n");
        exit(__LINE__);
    }

    if (bServer)
    {
        //run server
        server->RunTest(v4Server, nPipeline, nSge);
    }
    else
    {
        struct sockaddr_in v4Src;

        SIZE_T len = sizeof(v4Src);
        hr = NdResolveAddress(
            (const struct sockaddr*)&v4Server,
            sizeof(v4Server),
            (struct sockaddr*)&v4Src,
            &len);
        if (FAILED(hr))
        {
            printf("NdResolveAddress failed with %08x\n", hr);
            exit(__LINE__);
        }
        if (testName == ND_INVALID_IP_TEST) {
            v4Server.sin_addr.s_addr = 0;
        }

        //run client
        client->RunTest(v4Src, v4Server, nPipeline, nSge);

    }

    delete client;
    delete server;

    hr = NdCleanup();
    if (FAILED(hr))
    {
        printf("NdCleanup failed with %08x\n", hr);
        exit(__LINE__);
    }

    CloseHandle(Ov.hEvent);
    _fcloseall();
    WSACleanup();
    return 0;
}
