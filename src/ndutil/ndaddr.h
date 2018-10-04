//
// Copyright(c) Microsoft Corporation.All rights reserved.
// Licensed under the MIT License.
//


#pragma once


namespace NetworkDirect
{

    class Provider;

    class Address
    {
        friend class ListHelper<Address>;

        LIST_ENTRY m_link;
        Provider* m_pProvider;
        SOCKADDR_INET m_Addr;

    public:
        Address(_In_ const struct sockaddr& addr, _In_ Provider& provider);

        bool Matches(const struct sockaddr* pMatchAddr) const;
        Provider* GetProvider() const { return m_pProvider; }

        short AF() const { return m_Addr.si_family; };
        SIZE_T CopySockaddr(_Out_writes_(len) BYTE* pBuf, _In_ SIZE_T len) const;

    };

} // namespace NetworkDirect

