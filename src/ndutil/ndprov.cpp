//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//


#include "precomp.h"
#include "ndaddr.h"
#include "ndprov.h"


namespace NetworkDirect
{

    Provider::Provider(int version) :
        m_Guid(GUID_NULL),
        m_hProvider(nullptr),
        m_pfnDllGetClassObject(nullptr),
        m_pfnDllCanUnloadNow(nullptr),
        m_Path(nullptr),
        m_Version(version),
        m_Active(true)
    {
        m_link.Flink = &m_link;
        m_link.Blink = &m_link;
    }


    Provider::~Provider()
    {
        if (m_hProvider != nullptr)
        {
            ::FreeLibrary(m_hProvider);
        }

        if (m_Path)
        {
            ::HeapFree(ghHeap, 0, m_Path);
        }
    }


    HRESULT Provider::Init(GUID& ProviderGuid)
    {
        INT pathLen;
        INT ret, err;
        WCHAR* pPath;

        // Get the path length for the provider DLL.
        pPath = static_cast<WCHAR*>(
            ::HeapAlloc(ghHeap, 0, sizeof(WCHAR) * MAX_PATH)
            );
        if (pPath == nullptr)
        {
            return ND_NO_MEMORY;
        }

        pathLen = MAX_PATH;
        ret = ::WSCGetProviderPath(&ProviderGuid, pPath, &pathLen, &err);
        if (ret != 0)
        {
            ::HeapFree(ghHeap, 0, pPath);
            return HRESULT_FROM_WIN32(err);
        }

        pathLen = ::ExpandEnvironmentStringsW(pPath, nullptr, 0);
        if (pathLen == 0)
        {
            ::HeapFree(ghHeap, 0, pPath);
            return HRESULT_FROM_WIN32(::GetLastError());
        }

        m_Path = static_cast<WCHAR*>(
            ::HeapAlloc(ghHeap, 0, sizeof(WCHAR) * pathLen)
            );
        if (m_Path == nullptr)
        {
            ::HeapFree(ghHeap, 0, pPath);
            return ND_NO_MEMORY;
        }

        ret = ::ExpandEnvironmentStringsW(pPath, m_Path, pathLen);

        // We don't need the un-expanded path anymore.
        ::HeapFree(ghHeap, 0, pPath);

        if (ret != pathLen)
        {
            return ND_UNSUCCESSFUL;
        }

        m_Guid = ProviderGuid;
        return S_OK;
    }


    //
    // Serialization with TryUnload must be provided by the caller.  Multiple
    // callers may call this function concurrently.
    //
    HRESULT Provider::GetClassObject(
        _In_ const IID& iid,
        _Out_ void** ppInterface
    )
    {
        if (m_hProvider == nullptr)
        {
            HMODULE hProvider;
            hProvider = ::LoadLibraryExW(m_Path, nullptr, 0);
            if (hProvider == nullptr)
            {
                return HRESULT_FROM_WIN32(::GetLastError());
            }

            m_pfnDllGetClassObject = reinterpret_cast<DLLGETCLASSOBJECT>(
                ::GetProcAddress(hProvider, "DllGetClassObject")
                );
            if (!m_pfnDllGetClassObject)
            {
                ::FreeLibrary(hProvider);
                return HRESULT_FROM_WIN32(::GetLastError());
            }

            m_pfnDllCanUnloadNow = reinterpret_cast<DLLCANUNLOADNOW>(
                ::GetProcAddress(hProvider, "DllCanUnloadNow")
                );
            if (!m_pfnDllCanUnloadNow)
            {
                ::FreeLibrary(hProvider);
                return HRESULT_FROM_WIN32(::GetLastError());
            }

            HMODULE hCurrentProvider = static_cast<HMODULE>(
                ::InterlockedCompareExchangePointer(
                    reinterpret_cast<void**>(&m_hProvider), hProvider, nullptr)
                );
            if (hCurrentProvider)
            {
                ASSERT(hCurrentProvider == hProvider);
                ::FreeLibrary(hProvider);
            }
        }

        HRESULT hr = m_pfnDllGetClassObject(
            m_Guid,
            iid,
            ppInterface
        );

        return hr;
    }


    //
    // Strict serialization must be provided by the caller.
    //
    bool Provider::TryUnload(void)
    {
        if (m_hProvider == nullptr)
        {
            return true;
        }

        ASSERT(m_pfnDllCanUnloadNow != nullptr);

        HRESULT hr = m_pfnDllCanUnloadNow();
        if (hr != S_OK)
        {
            return false;
        }

        ::FreeLibrary(m_hProvider);
        m_hProvider = nullptr;

        return true;
    }


    NdV1Provider::NdV1Provider() :
        Provider(ND_VERSION_1)
    {
    }


    //
    // Serialization with TryUnload must be provided by the caller.  Multiple
    // callers may call this function concurrently.
    //
    HRESULT NdV1Provider::GetProvider(INDProvider** ppIProvider)
    {
        IClassFactory* pClassFactory;
        HRESULT hr = GetClassObject(
            IID_IClassFactory,
            reinterpret_cast<void**>(&pClassFactory)
        );
        if (FAILED(hr))
        {
            return hr;
        }

        hr = pClassFactory->CreateInstance(
            nullptr,
            IID_INDProvider,
            reinterpret_cast<void**>(ppIProvider)
        );

        // Now that we asked for the provider, we don't need the class factory.
        pClassFactory->Release();
        return hr;
    }


    HRESULT
        NdV1Provider::OpenAdapter(
            _In_ REFIID iid,
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ ULONG cbAddress,
            _Deref_out_ VOID** ppIAdapter
        )
    {
        if (iid != IID_INDAdapter)
        {
            return E_NOINTERFACE;
        }

        INDProvider* pIProvider;
        HRESULT hr = GetProvider(&pIProvider);
        if (FAILED(hr))
        {
            TryUnload();
            return ND_INVALID_ADDRESS;
        }

        hr = pIProvider->OpenAdapter(
            pAddress,
            cbAddress,
            reinterpret_cast<INDAdapter**>(ppIAdapter)
        );

        pIProvider->Release();
        TryUnload();

        return hr;
    }


    HRESULT
        NdV1Provider::QueryAddressList(
            _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList) SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ ULONG* pcbAddressList
        )
    {
        INDProvider *pIProvider;
        HRESULT hr = GetProvider(&pIProvider);
        if (FAILED(hr))
        {
            TryUnload();
            return ND_DEVICE_NOT_READY;
        }

        SIZE_T cbAddressList = *pcbAddressList;
        hr = pIProvider->QueryAddressList(pAddressList, &cbAddressList);
        *pcbAddressList = static_cast<ULONG>(cbAddressList);

        pIProvider->Release();
        TryUnload();

        return hr;
    }


    NdProvider::NdProvider() :
        Provider(ND_VERSION_2)
    {
    }


    //
    // Serialization with TryUnload must be provided by the caller.  Multiple
    // callers may call this function concurrently.
    //
    HRESULT NdProvider::OpenAdapter(
        _In_ REFIID iid,
        _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
        _In_ ULONG cbAddress,
        _Deref_out_ VOID** ppIAdapter
    )
    {
        IND2Provider* pIProvider;
        HRESULT hr = GetClassObject(
            IID_IND2Provider,
            reinterpret_cast<void**>(&pIProvider)
        );
        if (FAILED(hr))
        {
            TryUnload();
            return ND_INVALID_ADDRESS;
        }

        UINT64 id;
        hr = pIProvider->ResolveAddress(pAddress, cbAddress, &id);
        if (FAILED(hr))
        {
            pIProvider->Release();
            TryUnload();
            return ND_INVALID_ADDRESS;
        }

        hr = pIProvider->OpenAdapter(iid, id, ppIAdapter);

        pIProvider->Release();
        TryUnload();

        return hr;
    }


    HRESULT
        NdProvider::QueryAddressList(
            _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList) SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ ULONG* pcbAddressList
        )
    {
        IND2Provider *pIProvider;
        HRESULT hr = GetClassObject(
            IID_IND2Provider,
            reinterpret_cast<void**>(&pIProvider)
        );
        if (FAILED(hr))
        {
            TryUnload();
            return ND_DEVICE_NOT_READY;
        }

        hr = pIProvider->QueryAddressList(pAddressList, pcbAddressList);

        pIProvider->Release();
        TryUnload();

        return hr;
    }


} // namespace NetworkDirect
