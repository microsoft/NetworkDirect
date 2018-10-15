# IND2SharedReceiveQueue interface
Use to pool Receive requests among multiple queue pairs. 
The [IND2Adapter::CreateSharedReceiveQueue](./IND2Adapter.md#create-shared-receive-queue) method returns this interface.

The IND2SharedReceiveQueue interface inherits from [IND2Overlapped](./IND2Overlapped.md).
In addition, IND2SharedReceiveQueue defines the following methods. 


- [__GetNotifyAffinity__](#srq-get-notify-affinity) - Returns the assigned affinity for processing Notify requests.

- [__Modify__](#srq-modify) - Modifies the number of Receive requests that a shared receive queue supports, and/or the threshold below which the shared receive queue will complete pending Notify requests.
- [__Notify__](#srq-notify) - Requests notification for errors or for the number of pending Receive requests that are falling below the notification threshold.
- [__Receive__](#srq-receive) - Posts a Receive request for data from a remote peer connected to any of the queue pairs that are associated with the shared receive queue.

__Remarks:__

Shared receive queue support is optional. Support for shared receive queues is identified by the MaxSharedReceiveQueueDepth member of the [ND2_ADAPTER_INFO](./IND2Adapter.md#adapter-info) structure. To get the limit supported by the NetworkDirect adapter, call the [IND2Adapter::Query](./IND2Adapter.md#query-method) method.

## [IND2SharedReceiveQueue::GetNotifyAffinity](#srq-get-notify-affinity)
Returns the assigned affinity for processing Notify requests.
```
HRESULT GetNotifyAffinity(
 [out] USHORT* pGroup,
 [out] KAFFINITY* pAffinity
);
```
__Parameters:__

- __pGroup__ [out]

  The processor group number. On operating systems that do not support processor groups, this value is returned as zero.
- __pAffinity__ [out]

  A bitmap that specifies the processor affinity within the specified group.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_INSUFFICIENT_RESOURCES__ - The requested number of entries could not be allocated.
- __ND_INVALID_PARAMETER__ - The pGroup or pAffinity parameter was invalid.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.
- __ND_NOT_SUPPORTED__ - The underlying NetworkDirect adapter does not support setting affinity for Notify requests.

__Remarks:__
Provider implementations should set the notify affinity (including interrupt affinity if applicable) to one or more of the processors in the following order - requested affinity, calling thread affinity, preferably the calling thread's ideal processor, or process affinity. If none of these can be supported, the providers should attempt to set affinity to the closest processor to the requested settings, in the same order: requested affinity, calling thread affinity, process affinity.

Providers are encouraged to return affinity information for Notify request completion processing even if they do not support specifying affinity, rather than returning ND_NOT_SUPPORTED. This allows client code to optimize its notification strategy and CPU usage, especially if all notifications go to the same processor.

## [IND2SharedReceiveQueue::Modify](#srq-modify)
Modifies the number of Receive requests that a shared receive queue supports, and/or the threshold below which the shared receive queue will complete pending Notify requests.
```
HRESULT Modify(
 [in] ULONG queueDepth,
 [in] ULONG notifyThreshold
);
```
__Parameters:__

- __queueDepth__ [in]

  The new number of outstanding Receive requests that the shared receive queue should support. If zero, queueDepth does not modify the number of entries the shared receive queue can support.
- __notifyThreshold__ [in]

  The new number of outstanding Receive requests below which outstanding Notify requests are completed. Used to indicate to the application that it needs to post more Receive requests to prevent starving senders. If zero, notifyThreshold does not modify the threshold.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_INSUFFICIENT_RESOURCES__ - The requested number of entries could not be allocated.
- __ND_INVALID_PARAMETER__ - The requested number of entries exceeds the capabilities of the hardware.
- __ND_BUFFER_OVERFLOW__ - The requested number of entries is less than the number of Receive requests currently outstanding on the shared receive queue.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Implementation Notes:__

Provider implementations may allocate more entries than requested, although there are no mechanisms for advertising these extra entries to clients.

__Remarks:__

To get the limits that are supported by a NetworkDirect adapter, call the [IND2Adapter::Query](./IND2Adapter.md#query-method) method. 

You can increase or decrease the size of the queue. If the number you specify is less than the number of outstanding Receive requests in the queue, the method returns ND_BUFFER_OVERFLOW (the queue remains unchanged).

## [IND2SharedReceiveQueue::Notify](#srq-notify)
Requests notification for errors or when the number of outstanding Receive requests falls below the notifyThreshold specified in [IND2Adapter::CreateSharedReceiveQueue](./IND2Adapter.md#create-shared-receive-queue) or [IND2SharedReceiveQueue::Modify](#srq-modify).

```
HRESULT Notify(
 [in] OVERLAPPED *pOverlapped
);
```

__Parameters:__
- __pOverlapped__ [in]

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that must remain valid for the duration of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_PENDING__ - The notification request was successfully queued to the shared receive queue.
- __ND_FAILURE__ - The shared receive queue experienced a catastrophic error and is unusable. All associated queue pairs are also unusable. No future completions will be reported. This is generally indicative of a hardware failure.
- __ND_CANCELED__ - The request was canceled.
- __ND_INSUFFICIENT_RESOURCES__ - The request for notification could not be queued.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Remarks:__

You can use this method to get notification when the number of outstanding Receive requests falls below the specified threshold, to notify the client that more Receive requests should be issued.

Multiple requests for notification can be outstanding for the same shared receive queue. All such requests will be completed by the next notification event on the specified shared receive queue. This is similar to a notification event that releases all callers that are waiting, rather than a synchronization event that releases, at most, one. This allows multiple threads to process notifications in parallel. 

By default, shared receive queues are not set to generate notifications. This method arms the shared receive queue for a threshold notification.

## [IND2SharedReceiveQueue::Receive](#srq-receive)
Receives data from a peer.
```
HRESULT Receive(
 [in] VOID *requestContext,
 [in] const ND2_SGE sge[],
 [in] ULONG nSge
);
```

__Parameters:__
- __requestContext__ [in]

  A context value to associate with the request, returned in the RequestContext member of the [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure that is returned when the request completes.
- __sge__ [in]

  An array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures that describe the buffers that will receive the data that the peer sends. May be nullptr if nSge is zero.
- __nSge__ [in]

  The number of entries in the sge array. May be zero.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of Receive requests allowed on this shared receive queue. The queueDepth parameter of the [IND2Adapter::CreateSharedReceiveQueue](./IND2Adapter.md#create-shared-receive-queue) method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list exceeded the number allowed on the queue pair. The maxRequestSge parameter of the [IND2Adapter::CreateSharedReceiveQueue](./IND2Adapter.md#create-shared-receive-queue) method specifies the limit.

__Remarks:__

You can call this method before any associated queue pairs are connected. 

You must post a Receive request before the peer posts a Send request. The buffers that you specify must be large enough to receive the sent data. If not, the Send request fails and the connection is terminated; the completion status for the Receive request is ND_BUFFER_OVERFLOW. If the Receive fails, the shared receive queue can still process existing or future requests.

The protocol determines how many Receive requests you must post and the buffer size that is required for each post.

The array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures can be allocated temporarily on the stack.
