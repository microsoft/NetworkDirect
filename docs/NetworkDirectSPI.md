# NetworkDirect SPI (version 2)

The NetworkDirect architecture provides application developers with a networking interface that enables zero-copy data transfers between applications, kernel-bypass I/O generation and completion processing, and one-sided data transfer operations. The NetworkDirect service provider interface (SPI) defines the interface that NetworkDirect providers implement to expose their hardware capabilities to applications. 

## NetworkDirect SPI model

NetworkDirect client applications use [Component Object Model](https://docs.microsoft.com/windows/desktop/com/component-object-model--com--portal) (COM) interfaces to communicate with providers. The NetworkDirect client applications use only the core COM programming model, not the COM runtime. COM interfaces provide flexible extensibility through the [IUnknown::QueryInterface](https://docs.microsoft.com/windows/desktop/api/unknwn/nf-unknwn-iunknown-queryinterface(q_)) method, allowing providers to return any other interface they desire, to expose hardware-specific features easily. NetworkDirect providers do not register their objects in the system because they are not true COM objects.

There is no support for marshaling in the NetworkDirect SPI architecture, and the COM interfaces that are exposed by a provider are always instantiated in-process. This model is similar to the driver model that is used by the Windows Driver Foundation [User Mode Driver Framework (UMDF)](https://docs.microsoft.com/windows-hardware/drivers/wdf/overview-of-the-umdf). 

The NetworkDirect SPI framework defines the following interfaces:


- [__IND2Provider:__](./IND2Provider.md) Represents a service provider. IND2Adapter objects are instantiated through the [IND2Provider::OpenAdapter](./IND2Provider.md#open-adatper) method.
- [__IND2Overlapped:__](IND2Overlapped.md) Base class for objects on which overlapped operations can be performed.
- [__IND2Adapter__:](./IND2Adapter.md) Interface to a service provider’s hardware adapter instance.
- [__IND2CompletionQueue__](./IND2CompletionQueue.md): Interface to a completion queue instance. Created through the [IND2Adapter::CreateCompletionQueue](./IND2Adapter.md#create-completion-queue) method.
- [__IND2MemoryRegion__](./IND2MemoryRegion.md) - Interface to a memory region, which represents a local buffer, registered with an adapter instance. Created through the [IND2Adapter::CreateMemoryRegion](./IND2Adapter.md#create-memory-region) method.
- [__IND2SharedReceiveQueue__](./IND2SharedReceiveQueue.md): Interface to a shared receive queue, used to aggregate receive buffers between queue pairs. Created through the [IND2Adapter::CreateSharedReceiveQueue](./IND2Adapter.md#create-shared-receive-queue) method.
- [__IND2QueuePair__](./IND2QueuePair.md):	Interface to a queue pair instance, which is used to perform I/O operations. Created through the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) or [IND2Adapter::CreateQueuePairWithSrq](./IND2Adapter.md#create-queue-pair-with-srq) method.
- [__IND2Connector__](./IND2Connector.md):	Interface to a connector instance that is used to manage connection establishment for IND2QueuePair objects. Created through the [IND2Adapter::CreateConnector](./IND2Adapter.md#create-connector) method.
- [__IND2Listener__](./IND2Listener.md): Interface to a listen request. Created through the [IND2Adapter::CreateListener](./IND2Adapter.md#create-listener) method.

## Provider management

NetworkDirect SPI applications support zero, one, or more service provider libraries. NetworkDirect provider management follows the established mechanisms defined for [Windows Sockets Direct](https://docs.microsoft.com/windows-hardware/drivers/network/windows-sockets-direct) providers and traditional layered service providers.

### [Registering a NetworkDirect provider](#register-provider)

To register your provider, call the [WSCInstallProvider](https://docs.microsoft.com/windows/desktop/api/ws2spi/nf-ws2spi-wscinstallprovider) function. The following fields of the [WSAPROTOCOL_INFO](https://msdn.microsoft.com/758c5553-056f-4ea5-a851-30ef641ffb14) structure identify the provider as a NetworkDirect provider.

__dwServiceFlags1__: 
- XP1_GUARANTEED_DELIVERY
- XP1_GUARANTEED_ORDER
- XP1_MESSAGE_ORIENTED
- XP1_CONNECT_DATA 

__dwProviderFlags__:
- PFL_HIDDEN - Prevents the WSAEnumProtocols function from returning the provider in the enumeration results.
- PFL_NETWORKDIRECT_PROVIDER (0x00000010) - Identifies the provider as a NetworkDirect provider.

__iVersion__: 2

The current version of the NetworkDirect SPI.

__iAddressFamily__: AF_INET or AF_INET6

Providers that support IPv4 and IPv6 addresses do not need to register their provider multiple times. A single registration as AF_INET6 is sufficient. The NetworkDirect SPI framework can support IPv4 and IPv6 addresses that are reported simultaneously.

__iSocketType__: -1 (0xFFFFFFFF)

__iProtocol__: 0

No protocols are defined by the NetworkDirect SPI architecture.

__iProtocolMaxOffset__: 0

### [Instantiating a NetworkDirect provider](#instantiate-provider)
To get the NetworkDirect providers that are registered on the computer, call the [WSCEnumProtocols](https://docs.microsoft.com/windows/desktop/api/ws2spi/nf-ws2spi-wscenumprotocols) function. For each protocol that the function returns, compare the members of the [WSAPROTOCOL_INFO](https://msdn.microsoft.com/758c5553-056f-4ea5-a851-30ef641ffb14) structure to those that are specified in the above section. If the member values match, the protocol can be a NetworkDirect provider.

To determine if the provider is a NetworkDirect service provider, you must try to instantiate the IND2Provider interface. 

The following steps show how to instantiate the provider:
1. Load the provider's DLL. To get the path to the DLL, call the [WSCGetProviderPath](https://docs.microsoft.com/windows/desktop/api/ws2spi/nf-ws2spi-wscgetproviderpath) function by using the ProviderId member from the [WSAPROTOCOL_INFO](https://msdn.microsoft.com/758c5553-056f-4ea5-a851-30ef641ffb14) structure.
2. Call the library's [DllGetClassObject](https://docs.microsoft.com/windows/desktop/api/combaseapi/nf-combaseapi-dllgetclassobject) entry point to instantiate the IND2Provider interface.

Service provider libraries are expected to implement a [DllCanUnloadNow](https://docs.microsoft.com/windows/desktop/api/combaseapi/nf-combaseapi-dllcanunloadnow) entry point to allow clients to unload service provider libraries when they are no longer in use. Providers must also be able to handle multiple calls for the IND2Provider interface.

Now that you have a provider, you need to determine if the provider supports the IP address that you want to use. To determine the list of IP addresses that the provider supports, call the [IND2Provider::QueryAddressList](./IND2Provider.md#query-address-list) method. [IND2Provider](./IND2Provider.md) interface describes details about getting an interface to the NetworkDirect adapter that you want to use.

### Getting an interface to a NetworkDirect adapter
If you know the IP address of the NetworkDirect adapter that you want to use, call the [IND2Provider::OpenAdapter](./IND2Provider.md#open-adatper) method. If not, you first need to determine the local IP address of the adapter that will give you the best access to the destination address. For example, you could call the [GetBestInterfaceEx](https://docs.microsoft.com/windows/desktop/api/iphlpapi/nf-iphlpapi-getbestinterfaceex) function to get the local address.

After getting the local IP address, call the QueryAddressList method and enumerate the IP addresses that the provider supports. If the provider supports your local IP address, call the [IND2Provider::OpenAdapter](./IND2Provider.md#open-adatper) method to get an interface to the adapter.


## Asynchronous operations
Many operations in the NetworkDirect SPI take an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure as input. These operations are expected to provide the same functionality as traditional Win32 asynchronous I/O. Coupled with exposing the adapter’s underlying file handle on which asynchronous operations are performed, this design provides flexibility for applications to handle operations as best suits them. For example, an application can register the underlying file handle with an IOCP. All notifications, for example CQ notifications, are delivered by completing a request for notification that includes an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure. As such, providers never invoke callbacks into client code. All notifications are explicitly requested by the client.

Functions that take a pointer to an OVERLAPPED structure will always receive a valid pointer, never nullptr.

Note that the ND providers internally call [SetFileCompletionNotificationModes](https://docs.microsoft.com/windows/desktop/api/winbase/nf-winbase-setfilecompletionnotificationmodes) with both FILE_SKIP_COMPLETION_PORT_ON_SUCCESS and FILE_SKIP_SET_EVENT_ON_HANDLE set. As a result, the only return value that will cause a notification to be reported is if the operations return ND_PENDING.  If the operations return ND_SUCCESS, then the request is complete and the overlapped will not be used/notified, or if you use IOCP you will not see it reported there. All error values from the operations indicate immediate failure, so there will not be an async notification since the request failed to submit. It is necessary for the application to handle two distinct code paths where an operation can either return successfully or has to be completed asynchronously.

#### Implementation note

The current design for asynchronous operations places the burden of handling synchronous calls on the clients. As such, providers will always see asynchronous calls whenever an overlapped operation is possible.

Providers are likely to have synchronous commands (such as CreateCompletionQueue), and they will likely handle the synchronous I/O internally or issue the request on a different file handle. If the provider uses two file handles, one for asynchronous and the other for synchronous calls, it may be simpler for provider libraries to handle the synchronous requests (where pOverlapped == nullptr) rather than having the client do so for them.
