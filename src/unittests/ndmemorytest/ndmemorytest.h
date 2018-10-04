//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

#ifndef _ND_MEMORY_TEST_
#define _ND_MEMORY_TEST_

// to get STATUS_DATA_ERROR
#include <ntstatus.h>
// define WIN32_NO_STATUS so that STATUS_* types are not defined again
#define WIN32_NO_STATUS

#include "time.h"
#include "ndtestutil.h"
#include "string.h"

//default prarameters for tests
const USHORT x_DefaultPort = 54350;
const DWORD x_MaxXfer = (4 * 1024 * 1024);
const DWORD x_HdrLen = 40;
const SIZE_T x_MaxVolume = (500 * x_MaxXfer);
const DWORD x_MaxIterations = 100000;

struct MemoryWindowDesc
{
    UINT64 base;
    UINT64 length;
    UINT32 token;
};

class NdWriteViolationServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdWriteViolationClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdMRDeregisterServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdMRDeregisterClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdInvalidReadServer : public NdTestServerBase
{
public:
    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdInvalidWriteServer : public NdTestServerBase
{
public:
    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdInvalidReadWriteClient : public NdTestClientBase
{
public:
    NdInvalidReadWriteClient(const TCHAR *testName)
    {
        wcscpy_s(m_testName, testName);
    }
    void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
private:
    TCHAR m_testName[64] = { 0 };
};

class NdOverReadServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};
class NdOverWriteServer : public NdTestServerBase {
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};


class NdOverReadWriteClient : public NdTestClientBase
{
public:
    NdOverReadWriteClient(const TCHAR *testName)
    {
        wcscpy_s(m_testName, testName);
    }
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
private:
    TCHAR m_testName[64] = { 0 };
};

class NdLargePrivateDataServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdLargePrivateDataClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdSendReceiveNoConnectionServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdSendReceiveNoConnectionClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdSendNoReceiveServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdSendNoReceiveClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};


class NdReceiveFlushQPServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdReceiveFlushQPClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdReceiveConnectorClosedServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdReceiveConnectorClosedClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdLargeQPDepthServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdLargeQPDepthClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};
class NdQPMaxAllServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdQPMaxAllClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdReadConnectionClosedServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdWriteConnectionClosedServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdReadWriteConnectionClosedClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdMRInvalidBufferServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdMRInvalidBufferClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdConnectListenerClosingServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdConnectListenerClosingClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdInvalidIPServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdInvalidIPClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};
class NdDualListenServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdDualListenClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdDualConnectionServer : public NdTestServerBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdDualConnectionClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdConnRejectCloseServer : public NdTestServerBase
{
public:
    NdConnRejectCloseServer(const TCHAR *testName)
    {
        wcscpy_s(m_testName, testName);
    }
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );

private:
    TCHAR m_testName[64] = { 0 };
};

class NdConnRejectClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};

class NdConnCloseClient : public NdTestClientBase
{
public:
    virtual void RunTest(
        _In_ const struct sockaddr_in& v4Src,
        _In_ const struct sockaddr_in& v4Dst,
        _In_ DWORD queueDepth,
        _In_ DWORD nSge
    );
};
#endif