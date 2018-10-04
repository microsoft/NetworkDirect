//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

inline void INIT_LOG(const LPCWSTR testname)
{
    wprintf(L"Beginning test: %s\n", testname);
}

inline void END_LOG(const LPCWSTR testname)
{
    wprintf(L"End of test: %s\n", testname);
}

inline void LOG_FAILURE_HRESULT_AND_EXIT(HRESULT hr, const LPCWSTR errormessage, int LINE)
{
    wprintf(errormessage, hr);
    wprintf(L"  Line: %d\n", LINE);

    exit(LINE);
}

inline void LOG_FAILURE_AND_EXIT(const LPCWSTR errormessage, int LINE)
{
    wprintf(errormessage);
    wprintf(L"  Line: %d\n", LINE);

    exit(LINE);
}

inline void LOG_FAILURE_HRESULT(HRESULT hr, const LPCWSTR errormessage, int LINE)
{
    wprintf(errormessage, hr);
    wprintf(L"  Line: %d\n", LINE);
}

inline void LOG_FAILURE(const LPCWSTR errormessage, int LINE)
{
    wprintf(errormessage);
    wprintf(L"  Line: %d\n", LINE);
}

inline void RedirectLogsToFile(const TCHAR* logFileName)
{
    // Can't use freopen_s because it doesn't allow sharing.
    // So supress the deprecated warning, because for our use
    // it isn't deprecated.
#pragma warning( disable : 4996 )
    if (_tfreopen(logFileName, _T("w"), stdout) == NULL ||
        _tfreopen(logFileName, _T("a+"), stderr) == NULL)
#pragma warning( default : 4996 )
    {
        LOG_FAILURE_AND_EXIT(L"failed to redirect logs", __LINE__);
    }
}
