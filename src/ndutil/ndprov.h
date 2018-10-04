//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//


#pragma once

namespace NetworkDirect
{

    typedef HRESULT
    (*DLLGETCLASSOBJECT)(
        REFCLSID rclsid,
        REFIID rrid,
        LPVOID* ppv
        );

    typedef HRESULT
    (*DLLCANUNLOADNOW)(void);


    class Provider
    {
        friend class ListHelper<Provider>;

        GUID m_Guid;
        LIST_ENTRY m_link;
        HMODULE m_hProvider;
        DLLGETCLASSOBJECT m_pfnDllGetClassObject;
        DLLCANUNLOADNOW m_pfnDllCanUnloadNow;
        WCHAR* m_Path;
        int m_Version;
        bool m_Active;

    public:
        Provider(int version);
        ~Provider(void);
        HRESULT Init(GUID& ProviderGuid);
        void MarkActive(void) { m_Active = true; }
        void MarkInactive(void) { m_Active = false; }
        bool IsActive(void) const { return m_Active; }
        int GetVersion(void) const { return m_Version; }

        //
        // GetClassObject and TryUnload require the caller to provide
        // proper serialization.
        //
        bool TryUnload(void);

        virtual HRESULT OpenAdapter(
            _In_ REFIID iid,
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ ULONG cbAddress,
            _Deref_out_ VOID** ppIAdapter
        ) PURE;

        virtual HRESULT
            QueryAddressList(
                _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList)
                SOCKET_ADDRESS_LIST* pAddressList,
                _Inout_ ULONG* pcbAddressList
            ) PURE;

        bool operator ==(const GUID& providerGuid)
        {
            return InlineIsEqualGUID(m_Guid, providerGuid) == TRUE;
        }

    protected:
        //
        // GetClassObject and TryUnload require the caller to provide
        // proper serialization.
        //
        HRESULT GetClassObject(_In_ const IID& iid, _Out_ void** ppInterface);
    };


    class NdV1Provider : public Provider
    {
    public:
        NdV1Provider();
        ~NdV1Provider();

        HRESULT OpenAdapter(
            _In_ REFIID iid,
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ ULONG cbAddress,
            _Deref_out_ VOID** ppIAdapter
        ) override;

        HRESULT QueryAddressList(
            _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList) SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ ULONG* pcbAddressList
        ) override;

    private:
        HRESULT GetProvider(INDProvider** ppIProvider);
    };


    class NdProvider : public Provider
    {
    public:
        NdProvider();
        ~NdProvider();

        HRESULT OpenAdapter(
            _In_ REFIID iid,
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ ULONG cbAddress,
            _Deref_out_ VOID** ppIAdapter
        ) override;

        HRESULT QueryAddressList(
            _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList) SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ ULONG* pcbAddressList
        ) override;
    };

} // namespace NetworkDirect
