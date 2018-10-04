# IND2Adapter interface
Use to manage a NetworkDirect adapter. The [IND2Provider::OpenAdapter](./IND2Provider.md#open-adapter) method returns this interface.


The IND2Adapter interface inherits from [IUnknown interface](https://docs.microsoft.com/windows/desktop/api/unknwn/nn-unknwn-iunknown). In addition, IND2Adapter defines the following methods:

- [__CreateOverlappedFile__](#create-overlapped-file) - Creates a file handle for processing overlapped requests.
- [__Query__](#query-method) - Retrieves the capabilities and limits of the NetworkDirect adapter.
- [__QueryAddressList__](#query-address-list) - Returns the IPv4 and IPv6 addresses that are supported by the adapter instance.
- [__CreateCompletionQueue__](#create-completion-queue) - Allocates a completion queue.
- [__CreateMemoryRegion__](#create-memory-region) - Creates an [IND2MemoryRegion](./IND2MemoryRegion.md) instance that can be used to register an application-defined buffer to be used in requests issued to the initiator and receive queues of a queue pair.
- [__CreateMemoryWindow__](#create-memory-window) - Creates a [IND2MemoryWindow](./IND2MemoryWindow.md) instance in the invalidated state.
- [__CreateSharedReceiveQueue__](#create-shared-receive-queue) - Creates a new [IND2SharedReceiveQueue](./IND2SharedReceiveQueue.md) instance.
- [__CreateQueuePair__](#create-queue-pair) - Creates a new [IND2QueuePair](./IND2QueuePair.md) instance.
- [__CreateQueuePairWithSrq__](#create-queue-pair-with-srq) - Creates a new [IND2QueuePair](./IND2QueuePair.md) instance that uses a shared receive queue.
- [__CreateSharedEndpoint__](#create-shared-endpoint) - Creates a [IND2SharedEndpoint](./IND2SharedEndpoint.md) instance that allows connections to share local addresses.
- [__CreateConnector__](#create-connector) - Creates a new [IND2Connector](./IND2Connector.md) instance.
- [__CreateListener__](#create-listener) - Creates a new [IND2Listener](./IND2Listener.md) instance that listens for incoming connection requests.


## [IND2Adapter::CreateOverlappedFile](#create-overlapped-file)
Creates a file handle that will be used to process overlapped requests on interfaces derived from [IND2Overlapped](IND2Overlapped.md).

```
HRESULT CreateOverlappedFile(
 [out] HANDLE *phOverlappedFile
);
```

__Parameters:__
- __phOverlappedFile__ [out] 

  A file handle used for performing overlapped requests on the adapter or its resources. The returned file handle must have been opened for overlapped I/O.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the overlapped target.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

Providers must ensure that a misbehaved client that calls [CloseHandle](https://msdn.microsoft.com/library/windows/desktop/ms724211(v=vs.85).aspx) on the returned handle does not leak system resources or cause adverse effects on system stability or on another application's ability to use the adapter.

The returned file handle must have the following flags set through the Win32 [SetFileCompletionNotificationModes](https://docs.microsoft.com/windows/desktop/api/winbase/nf-winbase-setfilecompletionnotificationmodes) API:
- FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
- FILE_SKIP_SET_EVENT_ON_HANDLE

When an overlapped operation completes immediately (returns ND_SUCCESS), providers are not required to modify the [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is associated with the request.

Exposing the file handle is simpler than wrapping all possible functions that a user might want to take on a file handle.

__Remarks:__

You could use the handle to perform the following tasks:
- Bind to an I/O completion port if you want to get notification of overlapped operations (for example, memory registration or completion queue notification) delivered to an I/O completion port.
- Associate the handle with the system thread pool by using the [BindIoCompletionCallback](https://docs.microsoft.com/windows/desktop/api/winbase/nf-winbase-bindiocompletioncallback) function.

## [ND2_ADAPTER_INFO structure](#adapter-info)
Describes the capabilities and limits of the NetworkDirect adapter.

```
typedef struct _ND2_ADAPTER_INFO {
    ULONG   InfoVersion;
    
    UINT16  VendorId;
    UINT16  DeviceId;
    UINT64  AdapterId;
    
    SIZE_T  MaxRegistrationSize;
    SIZE_T  MaxWindowSize;
    
    ULONG   MaxInitiatorSge;
    ULONG   MaxReceiveSge;
    ULONG   MaxReadSge;
    
    ULONG   MaxTransferLength;
    ULONG   MaxInlineDataSize;
    
    ULONG   MaxInboundReadLimit;
    ULONG   MaxOutboundReadLimit;
    
    ULONG   MaxReceiveQueueDepth;
    ULONG   MaxInitiatorQueueDepth;
    ULONG   MaxSharedReceiveQueueDepth;
    ULONG   MaxCompletionQueueDepth;
    
    ULONG   InlineRequestThreshold;
    ULONG   LargeRequestThreshold;
    
    ULONG   MaxCallerData;
    ULONG   MaxCalleeData;
    
    ULONG   AdapterFlags;
} ND2_ADAPTER_INFO;
```

__Members:__
- __InfoVersion__

  The version of this structure, currently 1.
- __VendorId__

  The vendor ID, which is typically the vendor's organizational unique identifier (OUI).

- __DeviceId__

  Vendor defined device ID of the adapter.

- __AdapterId__

  Vendor defined unique ID of the adapter (similar to a MAC address for an Ethernet network adapter).

- __MaxRegistrationSize__

  The maximum size, in bytes, of an application-defined buffer that you can register using [IND2Adapter::RegisterMemory](#register-memory). This is the maximum size of a single registration that the adapter can address. 

  Note that since each SGE of a request references a memory registration, a single request could transfer more than this limit.

- __MaxWindowSize__

  The maximum size, in bytes, of a memory window. (See [IND2Adapter::CreateMemoryWindow](#create-memory-window).)

- __MaxInitiatorSge__

  The maximum number of scatter/gather entries that a queue pair supports for _Send_ and _Write_ requests.
  
- __MaxReceiveSge__

  The maximum number of scatter/gather entries that a queue pair supports for _Receive_ requests.

- __MaxReadSge__

  The maximum number of scatter/gather entries that a queue pair supports for _Read_ requests. Must be less than or equal to the MaxInitiatorSge value.

- __MaxTransferLength__

  The maximum length, in bytes, that can be referenced by all the scatter/gather entries for a request that is issued to a queue pair.

- __MaxInlineDataSize__

  The amount of data that a QueuePair can transfer directly to the device as part of a _Send_ or _Write_ request without further DMA operations.

- __MaxInboundReadLimit__

  The maximum number of in-progress _Read_ operations with a local queue pair as target.

- __MaxOutboundReadLimit__

  The maximum number of in-progress _Read_ operations with a local queue pair as initiator.

- __MaxReceiveQueueDepth__

  The maximum number of outstanding _Receive_ requests for a queue pair.

- __MaxInitiatorQueueDepth__

  The maximum number of outstanding _Send_, _Bind_, _Invalidate_, _Read_, and _Write_ requests for a queue pair.

- __MaxSharedReceiveQueueDepth__

  Maximum number of _Receive_ requests that can be outstanding on a shared receive queue at a time. May be zero if the adapter does not support shared receive queues.

- __MaxCompletionQueueDepth__

  The maximum number of queue pair operation completions that can be queued to a completion queue.

- __InlineRequestThreshold__

  Transfer size, in bytes, below which inline _Send_ and _Write_ operations will yield better results than DMA out of preregistered buffers.

- __LargeRequestThreshold__

  The data size, in bytes, above which _Read_ and _Write_ operations will yield better results than _Send_ and _Receive_ operations.

- __MaxCallerData__

  The maximum size, in bytes, of the private data that can be sent when calling the [IND2Connector::Connect](IND2Connector.md#connect-method) method or the [IND2Connector::ConnectSharedEndpoint](IND2Connector.md#connect-shared-endpoint) method.

- __MaxCalleeData__

  The maximum size, in bytes, of the private data that can be sent when calling the [IND2Connector::Accept](IND2Connector.md#accept-method) method or the [IND2Connector::Reject](IND2Connector.md#reject-method) method.

- __AdapterFlags__

  Flags indicating adapter capabilities

  - __ND_ADAPTER_FLAG_IN_ORDER_DMA_SUPPORTED__
 
       Flag indicating whether the adapter places the data in memory in order. May allow some optimizations in data transfers, although care must be taken in platforms where DMA controllers can buffer or reorder memory writes.
     
  - __ND_ADAPTER_FLAG_CQ_INTERRUPT_MODERATION_SUPPORTED__
    
      Flag indicating support for completion queue moderation.
      
  - __ND_ADAPTER_FLAG_MULTI_ENGINE_SUPPORTED__

    Flag indicating whether the adapter supports progressing completion queues in parallel.
    
  - __ND_ADAPTER_FLAG_CQ_RESIZE_SUPPORTED__

    Flag indicating whether the provider supports programmatically controlled interrupt moderation for each completion queue.
  
  - __ND_ADAPTER_FLAG_LOOPBACK_CONNECTIONS_SUPPORTED__

    Flag indicating whether the adapter supports loopback connections.


__Implementation Notes:__

Inline data is no longer per-SGE, but rather per request. Clients are provided with the capability to size their QueuePairs as required to support their usage model.

__Remarks:__
This structure defines the standard parameters of all NetworkDirect adapters. Only _Send_ and _Write_ requests can use inline data functionality.

See also [NDK_ADAPTER_INFO structure](https://docs.microsoft.com/windows/desktop/api/ndkinfo/ns-ndkinfo-_ndk_adapter_info).

## [IND2Adapter::Query](#query-method)
Retrieves the capabilities and limits of the NetworkDirect adapter.

```
HRESULT Query(
 [in, out, optional] ND2_ADAPTER_INFO *pInfo,
 [in, out] ULONG *pcbInfo
);
```

__Parameters:__
- __pInfo__ [in, out, optional] 

  An [ND2_ADAPTER_INFO structure](#adapter-info) that will contain the NetworkDirect adapter's information.  May be nullptr if *pcbInfo is zero.  If non-nullptr, the InfoVersion member must be set on input.
- __pcbInfo__ [in, out] 

  The size, in bytes, of the pInfo buffer. If the buffer is too small to hold the information, the method fails with ND_BUFFER_OVERFLOW, and it sets this parameter to the required buffer size. If the buffer is too big on input, the method sets this parameter to the size used.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The adapter information was retrieved.
- __ND_BUFFER_OVERFLOW__ - The pInfo buffer is not large enough to hold the adapter information,and the buffer was not modified. The pcbInfo parameter is updated with the required buffer size.

__Implementation Notes:__

A query for the capabilities must return ND_SUCCESS if the input buffer size is sizeof([ND2_ADAPTER_INFO](#adapter-info)), even if there is additional vendor-specific data that isn’t returned.

__Remarks:__

To determine the required buffer size, you can set pInfo to nullptr and the value pointed to by pcbInfo to zero. Then, call this method and use the returned value pointed to by pcbInfo to allocate the pInfo buffer.

The limits are provided by the NetworkDirect adapter hardware, which are independent of actual system resource limits such as available memory. The maximum limits reported may not be realized due to system load.

Getting the limits before you create a queue pair or completion queue or before you register memory will let you know if your application can run within the limits of hardware.
- You can use the _LargeRequestThreshold_ member of [ND2_ADAPTER_INFO](#adapter-info) to determine when _Read_ and _Write_ operations will yield better results than using Send and Receive. 
- You can use the _InlineRequestThreshold_ member of [ND2_ADAPTER_INFO](#adapter-info) to determine when inline _Send_ and _Write_ operations will yield better results than using traditional DMA.

__Implementation Note:__

The LargeRequestThreshold and InlineRequestThreshold values should be dynamically determined by providers whenever possible.

## [IND2Adapter::QueryAddressList](#query-address-list)
Retrieves a list of local addresses that the adapter supports.

```
HRESULT QueryAddressList(
 [in, out, optional] SOCKET_ADDRESS_LIST *pAddressList,
 [in, out] ULONG *pcbAddressList
);
```

__Parameters:__
- __pAddressList__ [in, out, optional] 

  A list of addresses that the NetworkDirect adapters support. (For details of the list, see the [SOCKET_ADDRESS_LIST](https://docs.microsoft.com/windows/desktop/api/ws2def/ns-ws2def-_socket_address_list) structure. May be nullptr if pcbAddressList is zero.

- __pcbAddressList__ [in, out] 

  The size, in bytes, of the pAddressList buffer. If the buffer is too small to hold the addresses, the method fails with ND_BUFFER_OVERFLOW and sets this parameter to the required buffer size. If the buffer is too big, the method sets this parameter to the size used on output.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - All addresses supported by NetworkDirect adapters were returned in pAddressList.
- __ND_BUFFER_OVERFLOW__- The pAddressList buffer is not large enough to hold the addresses, and the buffer was not modified. The pBufferSize parameter is updated with the required buffer size.

__Remarks:__

To determine the required buffer size, set pAddressList to nullptr and pcbAddressList to zero. Then call this method and use the value returned in the pcbAddressList parameter to allocate the pAddressList buffer. You should perform these calls in a loop because the size could change between calls.

## [IND2Adapter::CreateCompletionQueue](#create-completion-queue)
Allocates a completion queue.
```
HRESULT CreateCompletionQueue(
 [in]  REFIID iid,
 [in]  HANDLE hOverlappedFile,
 [in]  ULONG queueDepth,
 [in]  USHORT group,
 [in]  KAFFINITY affinity, 
 [out] void **ppCompletionQueue
);
```

__Parameters:__
- __iid__ [in]

  The IID of the completion queue interface requested. IID_IND2CompletionQueue must be supported, but other IIDs may be supported as new interfaces are defined.
- __hOverlappedFile__ [in]

  Handle returned by a previous call to [IND2Adapter::CreateOverlappedFile](#create-overlapped-file), on which overlapped operations on this completion queue should be performed.

- __queueDepth__ [in] 

  The number of completion queue entries to support. The value must be greater than zero and less than the MaxCqDepth member of the [ND2_ADAPTER_INFO](#adapter-info) structure.

- __group__ [in] 

  Processor group number for the requested affinity. This value must be 0 if the operating system does not support processor groups.

- __affinity__ [in] 

  A bitmap that specifies the processor affinity on which notifications should be processed by the hardware if possible. If affinity cannot be set for completion queue notifications, this value is silently ignored. Set to 0 if no specific affinity is requested.

- __ppCompletionQueue__ [out] 

  An [IND2CompletionQueue](./IND2CompletionQueue.md) interface to the completion queue.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the completion queue.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough hardware resources to create the completion queue.
- __ND_INVALID_PARAMETER_3__ - The adapter cannot support the requested number of entries.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

Provider implementations should set the notify affinity (including interrupt affinity if applicable) to one or more of the specified processors in the following order: requested affinity, calling thread affinity, preferably the calling thread’s ideal processor, or process affinity. If none of these can be supported, the providers should attempt to set affinity to the closest processor to the requested settings, in the same order: requested affinity, calling thread affinity, process affinity.

__Remarks:__

You must create at least one completion queue. You can create one for inbound requests and another for outbound requests, or you can use the same queue for both. You specify the completion queue when you call [IND2Adapter::CreateQueuePair](#create-queue-pair) to create a queue pair.
More than one queue pair can specify the same completion queue.

## [IND2Adapter::CreateMemoryRegion](#create-memory-region)
Allocates a memory region.
```
HRESULT CreateMemoryRegion(
 [in]  REFIID iid, 
 [in]  HANDLE hOverlappedFile,
 [out] void **ppMemoryRegion
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2MemoryRegion must be supported, but other IIDs may be supported as new interfaces are defined.
- __hOverlappedFile__ [in]

  Handle returned by a previous call to [IND2Adapter::CreateOverlappedFile](#create-overlapped-file), on which overlapped operations on this memory region should be performed.
- __ppMemoryRegion__ [out] 

  An [IND2MemoryRegion](./IND2MemoryRegion.md) interface to the memory region.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the memory region.
- __ND_INSUFFICIENT_RESOURCES__ - There was not enough hardware resources to create the memory region.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Remarks__

You must create memory regions to reference memory in data transfer operations.

## [IND2Adapter::CreateMemoryWindow](#create-memory-window)
Creates an uninitialized memory window.
```
HRESULT CreateMemoryWindow(
 [in]  REFIID iid, 
 [out] void **ppMemoryWindow
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2MemoryWindow must be supported, but other IIDs may be supported as new interfaces are defined.
- __ppMemoryWindow__ [out] 

  An [IND2MemoryWindow](./IND2MemoryWindow.md) interface.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_INSUFFICIENT_RESOURCES__ - There was not enough hardware resources to create the memory window.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Remarks__

You need to create a memory window and bind it to registered memory only if the connected peer is going to perform _Read_ and _Write_ operations and you want fine-grained access control to a long-lived memory registration. The side issuing the _Read_ and _Write_ requests does not create the memory window.

The memory window becomes valid when you bind it to registered memory. For more information, see [IND2Adapter::CreateMemoryRegion](#create-memory-region) method. 

To bind the window to registered memory, call the [IND2QueuePair::Bind](./IND2QueuePair.md#bind-method) method. The window can be bound to only a single queue pair at one time. However, the window could be bound to a different queue pair after being invalidated.

To invalidate the window, call the [IND2QueuePair::Invalidate](./IND2QueuePair.md#invalidate-method) method. You should not invalidate the window unless you receive a message from the connected peer indicating that the connected peer is done accessing the window. 

## [IND2Adapter::CreateSharedReceiveQueue](#create-shared-receive-queue)
Allocates a shared receive queue.
```
HRESULT CreateSharedReceiveQueue(
 [in]  REFIID iid, 
 [in]  HANDLE hOverlappedFile,
 [in]  ULONG queueDepth,
 [in]  ULONG maxRequestSge,
 [in]  ULONG notifyThreshold,
 [in]  USHORT group,
 [in]  KAFFINITY affinity,
 [out] void **ppSharedReceiveQueue
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2SharedReceiveQueue must be supported, but other IIDs may be supported as new interfaces are defined.
- __hOverlappedFile__ [in]

  Handle returned by a previous call to [IND2Adapter::CreateOverlappedFile](#create-overlapped-file), on which overlapped operations on this shared receive queue should be performed.
- __queueDepth__ [in] 

  The maximum number of outstanding _Receive_ requests.
- __maxRequestSge__ [in] 

  The maximum number of scatter/gather entries to support for _Receive_ requests.

- __notifyThreshold__ [in] 

  The number of outstanding _Receive_ requests below which the shared receive queue will complete any Notify requests in order for the application to post more buffers. May be zero to disable notifications.
- __group__ [in] 

  Processor group number for the requested affinity. This value must be 0 if the operating system does not support processor groups.
- __affinity__ [in] 

  A bitmap that specifies the processor affinity on which notifications should be processed by the hardware if possible. If affinity cannot be set for shared receive queue notifications, this value is silently ignored. Set to 0 if no specific affinity is requested.
- __ppSharedReceiveQueue__ [out] 

  An [IND2SharedReceiveQueue](./IND2SharedReceiveQueue.md) interface to the shared receive queue.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the completion queue.
- __ND_INSUFFICIENT_RESOURCES__ - There was not enough hardware resources to create the completion queue.
- __ND_INVALID_PARAMETER_3__ - The adapter cannot support the requested number of entries.
- __ND_INVALID_PARAMETER_4__ - The adapter cannot support the requested number of scatter/gather entries.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

Provider implementations should set the notify affinity (including interrupt affinity if applicable) to one or more of the specified processors in the following order: requested affinity, calling thread affinity, preferably the calling thread’s ideal processor, or process affinity. If none of these can be supported, the providers should attempt to set affinity to the closest processor to the requested settings, in the same order: requested affinity, calling thread affinity, process affinity.

__Remarks:__

Shared receive queues allow pooling receive buffers between multiple queue pairs. To use a shared receive queue, you must create queue pairs by using [IND2Adapter::CreateQueuePairWithSrq](#create-queue-pair-with-srq) method.

## [IND2Adapter::CreateQueuePair](#create-queue-pair)
Creates a queue pair to use for data transfers.
```
HRESULT CreateQueuePair(
 [in]  REFIID iid, 
 [in]  IUnknown *pReceiveCompletionQueue,
 [in]  IUnknown *pInitiatorCompletionQueue,
 [in]  void *context,
 [in]  ULONG receiveQueueDepth,
 [in]  ULONG initiatorQueueDepth,
 [in]  ULONG maxReceiveRequestSge,
 [in]  ULONG maxInitiatorRequestSge,
 [in]  ULONG inlineDataSize,
 [out] void **ppQueuePair
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2QueuePair must be supported, but other IIDs may be supported as new interfaces are defined.
- __pReceiveCompletionQueue__ [in] 

  An [IND2CompletionQueue](./IND2CompletionQueue.md) interface. The interface is used to queue _Receive_ request results.
- __pInitiatorCompletionQueue__ [in] 

  An [IND2CompletionQueue](./IND2CompletionQueue.md) interface. The interface is used to queue _Send_, _Bind_, _Invalidate_, _Read_, and _Write_ request results.
- __context__ [in] 

  Context value to associate with the queue pair, returned in the _RequestContext_ member of the [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure when calling [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results).
- __receiveQueueDepth__ [in] 

  The maximum number of outstanding _Receive_ requests.
- __initiatorQueueDepth__ [in] 

  The maximum number of outstanding _Send_, _Bind_, _Invalidate_, _Read_, and _Write_ requests.

- __maxReceiveRequestSge__ [in] 

  The maximum number of scatter/gather entries supported for _Receive_ requests.

- __maxInitiatorRequestSge__ [in] 

  The maximum number of scatter/gather entries supported for _Send_, _Read_, and _Write_ requests.
  
- __inlineDataSize__ [in] 

  The maximum inline data size supported for _Send_ and _Write_ requests.

- __ppQueuePair__ [out] 

  An [IND2QueuePair](./IND2QueuePair.md) interface. 

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_INVALID_PARAMETER_2__ - The pReceiveCompletionQueue parameter was not valid.
- __ND_INVALID_PARAMETER_3__ - The pInitiatorCompletionQueue parameter was not valid.
- __ND_INVALID_PARAMETER_5__ - The receiveQueueDepth parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_6__ - The initiatorQueueDepth parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_7__ - The maxReceiveRequestSge parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_8__ - The maxInitiatorRequestSge parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_9__ - The inlineDataSize parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough hardware resources to create the queue pair.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

NetworkDirect service providers are free to return more resources than requested, but there are no mechanisms to advertise the excess. It is not possible to change these parameters after a queue pair is created.

__Remarks__

To get the limits that are supported by the NetworkDirect adapter, call the [IND2Adapter::Query](#query-method) method. 

You can specify the same completion queue for both pReceiveCompletionQueue and pInitiatorCompletionQueue. The initiator completion queue reports completions for all requests other than _Receive_ requests. 

An instance of the [IND2Connector](./IND2Connector.md) interface can have only a single queue pair associated with it at a time. To disassociate the queue pair from the connector, the queue pair must be disconnected.

## [IND2Adapter::CreateQueuePairWithSrq](#create-queue-pair-with-srq)
Creates a queue pair to use for data transfers that are associated with a shared receive queue.
```
HRESULT CreateQueuePairWithSrq(
 [in]  REFIID iid, 
 [in]  IUnknown *pReceiveCompletionQueue,
 [in]  IUnknown *pInitiatorCompletionQueue,
 [in]  IUnknown *pSharedReceiveQueue,
 [in]  void *context,
 [in]  ULONG initiatorQueueDepth,
 [in]  ULONG maxInitiatorRequestSge,
 [in]  ULONG inlineDataSize,
 [out] void **ppQueuePair
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2QueuePair must be supported, but other IIDs may be supported as new interfaces are defined.

- __pReceiveCompletionQueue__ [in] 

  An [IND2CompletionQueue](./IND2CompletionQueue.md) interface. The interface is used to queue _Receive_ request results.

- __pInitiatorCompletionQueue__ [in] 

  An [IND2CompletionQueue](./IND2CompletionQueue.md) interface. The interface is used to queue _Send_, _Bind_, _Invalidate_, _Read_, and _Write_ request results.

- __pSharedReceiveQueue__ [in] 

  An [IND2SharedReceiveQueue](./IND2SharedReceiveQueue.md) interface. The interface is used to issue _Receive_ requests.

- __context__ [in] 

  Context value to associate with the queue pair, returned in the _RequestContext_ member of the [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure when calling [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results).

- __initiatorQueueDepth__ [in] 

  The maximum number of outstanding _Send_, _Bind_, _Invalidate_, _Read_, and _Write_ requests.

- __maxInitiatorRequestSge__ [in] 

  The maximum number of scatter/gather entries supported for _Send_, _Read_, and _Write_ requests.

 - __inlineDataSize__ [in] 

   The maximum inline data size supported for _Send_ and _Write_ requests.

- __ppQueuePair__ [out] 

  An [IND2QueuePair](./IND2QueuePair.md) interface. 

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NOT_SUPPORTED__ - Shared receive queue functionality is not supported by the adapter.
- __ND_INVALID_PARAMETER_2__ - The pReceiveCompletionQueue parameter was not valid.
- __ND_INVALID_PARAMETER_3__ - The pInitiatorCompletionQueue parameter was not valid.
- __ND_INVALID_PARAMETER_4__ - The pSharedReceiveQueue parameter was not valid.
- __ND_INVALID_PARAMETER_6__ - The initiatorQueueDepth parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_7__ - The maxInitiatorRequestSge parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INVALID_PARAMETER_8__ - The inlineDataSize parameter exceeded the limits of the NetworkDirect adapter.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough hardware resources to create the queue pair.
- __ND_DEVICE_REMOVED__- The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

NetworkDirect providers are free to return more resources than requested, but there are no mechanisms to advertise the excess. It is not possible to change these parameters after a queue pair is created.

Providers must ensure that a _Send_ request that arrives at a queue pair that is bound to an empty shared receive queue does not cause the connection to fail.

__Remarks:__

To get the limits that are supported by the NetworkDirect adapter, call the [IND2Adapter::Query](#query-method) method. 

You can specify the same completion queue for both pReceiveCompletionQueue and pInitiatorCompletionQueue. The initiator completion queue reports completions for all requests other than Receive requests.

An instance of the [IND2Connector](./IND2Connector.md) interface can have only a single queue pair bound to it at a time. To unbind the queue pair from the connector, the queue pair must be disconnected.

When using a shared receive queue, client applications cannot depend on credit flow control. Flow control in this case is managed by the hardware.

## [IND2Adapter::CreateSharedEndpoint](#create-shared-endpoint)
Creates a new IND2SharedEndpoint instance.
```
HRESULT CreateSharedEndpoint(
 [in]  REFIID iid, 
 [out] void **ppSharedEndpoint
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2MemoryRegion must be supported, but other IIDs may be supported as new interfaces are defined.

- __ppSharedEndpoint__ [out] 

  The requested interface that you use to connect to a remote peer.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the connector.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough hardware resources to create the connector.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

## IND2Adapter::CreateConnector
Creates a new [IND2Connector](./IND2Connector.md) instance.
```
HRESULT CreateConnector(
 [in]  REFIID iid, 
 [in]  HANDLE hOverlappedFile,
 [out] void **ppConnector
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2Connector must be supported, but other IIDs may be supported as new interfaces are defined.

- __hOverlappedFile__ [in]

  Handle returned by a previous call to [IND2Adapter::CreateOverlappedFile](#create-overlapped-file), on which overlapped operations on this connector should be performed.

- __ppConnector__ [out] 

  The requested interface that you use to connect to a remote peer.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the connector.
- __ND_INSUFFICIENT_RESOURCES__ - There was not enough hardware resources to create the connector.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

# [IND2Adapter::CreateListener](#create-listener)
Creates a new [IND2Listener](./IND2Listener.md) instance.
```
HRESULT CreateListener(
 [in]  REFIID iid, 
 [in]  HANDLE hOverlappedFile,
 [out] void **ppListener
);
```

__Parameters:__
- __iid__ [in]

  The IID of the memory region interface requested. IID_IND2Listener must be supported, but other IIDs may be supported as new interfaces are defined.

- __hOverlappedFile__ [in]

  Handle returned by a previous call to [IND2Adapter::CreateOverlappedFile](#create-overlapped-file), on which overlapped operations on this listen should be performed.

- __ppListener__ [out] 

  The requested interface that you use to listen for connection requests.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.
- __ND_SUCCESS__ - The operation completed successfully.
- __ND_NO_MEMORY__ - There was not enough memory to create the connector.
- __ND_INSUFFICIENT_RESOURCES__ - There was not enough hardware resources to create the connector.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.