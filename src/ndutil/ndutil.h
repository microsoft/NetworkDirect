//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//

#pragma once

namespace NetworkDirect
{
#ifndef ASSERT
#define ASSERT Assert
#if DBG
#define ASSERT_BENIGN( exp )    (void)(!(exp)?OutputDebugStringA("Assertion Failed:" #exp "\n"),FALSE:TRUE)
#else
#define ASSERT_BENIGN( exp )
#endif  // DBG
#endif  // ASSERT


    //---------------------------------------------------------
    // Lock wrapper.
    //
    class Lock
    {
        CRITICAL_SECTION* m_pLock;

    public:
        Lock(CRITICAL_SECTION* pLock) { m_pLock = pLock; EnterCriticalSection(m_pLock); }

        _Releases_lock_(*this->m_pLock)
            ~Lock() { LeaveCriticalSection(m_pLock); }
    };
    //---------------------------------------------------------

    extern HANDLE ghHeap;
} // namespace NetworkDirect
