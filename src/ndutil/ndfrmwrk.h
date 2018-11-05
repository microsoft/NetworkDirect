//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//


#pragma once

namespace NetworkDirect
{

    enum ND_NOTIFY_TYPE
    {
        ND_NOTIFY_PROVIDER_CHANGE,
        ND_NOTIFY_ADDR_CHANGE,
        ND_NOTIFY_MAX
    };


    class Framework
    {
        // IOCP for provider and address changes.
        HANDLE m_hIocp;
        HANDLE m_hProviderChange;
        // Socket for address list change and address resolution.
        SOCKET m_Socket;
        OVERLAPPED m_Ov[ND_NOTIFY_MAX];

        // Lock protecting the provider and address lists.
        CRITICAL_SECTION m_lock;

        List<Provider> m_ProviderList;
        List<Address> m_NdAddrList;
        List<Address> m_NdV1AddrList;

        volatile LONG m_nRef;


    public:
        Framework(void);
        ~Framework(void);

        HRESULT Init(void);

        ULONG AddRef(void);
        ULONG Release(void);

        HRESULT QueryAddressList(
            _In_ DWORD flags,
            _Out_opt_bytecap_post_bytecount_(*pcbAddressList, *pcbAddressList) SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ SIZE_T* pcbAddressList
        );

        HRESULT ResolveAddress(
            _In_bytecount_(cbRemoteAddress) const struct sockaddr* pRemoteAddress,
            _In_ SIZE_T cbRemoteAddress,
            _Out_bytecap_(*pcbLocalAddress) struct sockaddr* pLocalAddress,
            _Inout_ SIZE_T* pcbLocalAddress
        );

        HRESULT CheckAddress(
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ SIZE_T cbAddress
        );

        HRESULT OpenAdapter(
            _In_ REFIID iid,
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ SIZE_T cbAddress,
            _Deref_out_ VOID** ppIAdapter
        );

        void FlushProvidersForUser();

    private:
        static HRESULT ValidateAddress(
            _In_bytecount_(cbAddress) const struct sockaddr* pAddress,
            _In_ SIZE_T cbAddress
        );

        static void CountAddresses(
            _In_ const List<Address>& list,
            _Inout_ SIZE_T* pnV4,
            _Inout_ SIZE_T* pnV6
        );

        static void
            BuildAddressList(
                _In_ Provider& prov,
                _In_ const SOCKET_ADDRESS_LIST& addrList,
                _Inout_ List<Address>* pList
            );

        static void CopyAddressList(
            _In_ const List<Address>& list,
            _Inout_ SOCKET_ADDRESS_LIST* pAddressList,
            _Inout_ BYTE** ppBuf,
            _Inout_ SIZE_T* pcbBuf
        );

        void ProcessUpdates(void);

        void ProcessProviderChange(void);
        void ProcessAddressChange(void);
        void FlushProviders(void);
    };

} // namespace NetworkDirect
