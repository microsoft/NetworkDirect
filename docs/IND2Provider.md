# IND2Provider interface

Use to discover the NetworkDirect adapters that the provider supports.
For details on getting an instance of this interface, see [Instantiating a NetworkDirect Provider](./NetworkDirectSPI.md#instantiate-provider). Note that NetworkDirect providers are not registered with the system by using COM registration, so you do _not_ use the CoCreateInstance function to get this interface.

The IND2Provider interface inherits the methods of the [IUnknown interface](https://docs.microsoft.com/windows/desktop/api/unknwn/nn-unknwn-iunknown).
In addition, IND2Provider defines the following methods:

- [__QueryAddressList__](#ind2providerqueryaddresslist) - Retrieves a list of local addresses that the provider supports.

- [__ResolveAddress__](#ind2providerresolveaddress) - Resolves a local IPv4 or IPv6 address to a unique adapter ID.

- [__OpenAdapter__](#ind2provideropenadapter) - Retrieves an interface to a local NetworkDirect adapter.

#### Implementation Notes

__Requirements for InfiniBand:__

NetworkDirect providers use IP addresses to represent local and remote queue pairs, and it is the responsibility of the InfiniBand providers to manage mappings from these IP addresses to path records. Provides should use a distinct port space for RDMA connections, and only map to host TCP ports when absolutely necessary.

InfiniBand vendors should use the IP addressing annex to the IP specification for formatting the connection establishment messages. InfiniBand providers can use the TCP port space for connection management, but the port space should be managed separately from the host port space.

The InfiniBand 1.2 specification defines extensions to the architecture that provides matching semantics to iWARP. Specifically, the NetworkDirect SPI makes use of the following extensions:
- Base Queue Management Extensions
- Base Memory Management Extensions

__Requirements for iWARP devices:__

iWARP devices are expected to support establishing connections in RDMA mode without any streaming-mode support. RDMA read with local invalidate operations is not supported because the InfiniBand architecture does not support that capability.

__Registration:__

For details about how to register your provider, see the section [Registering a NetworkDirect service provider](./NetworkDirectSPI.md#registering-a-networkdirect-provider).

## IND2Provider::QueryAddressList
Retrieves a list of local addresses that the provider supports.

```
HRESULT QueryAddressList(
 [in, out, optional] SOCKET_ADDRESS_LIST *pAddressList,
 [in, out] ULONG *pcbAddressList
);
```

__Parameters:__

- __pAddressList__ [in, out, optional]

  A list of addresses that the NetworkDirect adapters support. (For details of the list, see [SOCKET_ADDRESS_LIST](https://docs.microsoft.com/windows/desktop/api/ws2def/ns-ws2def-_socket_address_list) structure. May be nullptr if pcbAddressList is zero.

- __pcbAddressList__ [in, out] 

  The size, in bytes, of the pAddressList buffer. If the buffer is too small to hold the addresses, the method fails with ND_BUFFER_OVERFLOW and sets this parameter to the required buffer size. If the buffer is too big, the method sets this parameter to the size that is used on output.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - All addresses supported by NetworkDirect adapters were returned in pAddressList.
- __ND_BUFFER_OVERFLOW__ - The pAddressList buffer is not large enough to hold the addresses, and the pAddressList buffer was not modified. The pcbAddressList parameter is updated with the required buffer size.

__Remarks:__
To determine the required buffer size, set pAddressList to nullptr, and set the value that is pointed to by pcbAddressList to zero. Then, call this method and use the value that is returned in the pcbAddressList parameter to allocate the pAddressList buffer. You may need to perform these calls in a loop because the size could change between calls.

### IND2Provider::ResolveAddress
Returns a provider-managed unique ID for an adapter when given a local IPv4 or IPv6 address.

```
HRESULT
ResolveAddress(
 [in]  const struct sockaddr *pAddress,
 [in]  ULONG cbAddress,
 [out] UINT64* pAdapterId
);
```

__Parameters:__
- __pAddress__ [in]

  A sockaddr structure that specifies the address of a local NetworkDirect adapter to open. Typically, this is a sockaddr_in structure for IPv4 addresses and a sockaddr_in6 structure for IPv6 addresses. The sin_port and sin6_port members are ignored for sockaddr_in and sockaddr_in6 address structures, respectively.
- __cbAddress__ [in] 

  The size, in bytes, of the pAddress buffer.
- __pAdapterId__ [out] 

  A unique identifier for the requested adapter. 

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The call succeeded and the ID of the requested adapter is returned in the pAdapterId parameter.
- __ND_NO_MEMORY__ - There was not enough memory to resolve the address.
- __ND_INVALID_ADDRESS__ - The address specified by the pAddress parameter is not supported by the NetworkDirect provider.

__Remarks:__

A single NetworkDirect adapter can support more than a single IP address. By making the relationship between the addresses and the adapters explicit, this function allows sharing common resources such as completion queues and memory regions between connections that are active on different IP addresses of the same adapter.


## IND2Provider::OpenAdapter
Retrieves an interface to a local NetworkDirect adapter.

```
HRESULT OpenAdapter(
 [in]  REFIID iid, 
 [in]  UINT64 adapterId,
 [out] void **ppAdapter
);
```

__Parameters:__
- __iid__ [in]

  The IID of the adapter interface requested. IID_IND2Adapter must be supported, but other IIDs may be supported as new interfaces are defined.
- __adapterId__ [in] 

  ID returned by a previous call to the [IND2Provider::ResolveAddress](#ind2providerresolveaddress) method.
- __ppAdapter__ [out] 
  
  The requested interface to the opened adapter. 

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The call succeeded and the handle to the opened adapter is returned in the ppAdapter parameter.
- __ND_NO_MEMORY__ - There was not enough memory to open the adapter.
- __ND_INVALID_PARAMETER__ - The adapterId specified is not valid.

__Implementation Notes:__

After getting an interface to the adapter, the application can release the IND2Provider interface. Providers must ensure that they remain loaded while resources are allocated.
