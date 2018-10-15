# IND2QueuePair interface
Use to exchange data with a remote peer.
The [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method returns this interface.

The IND2QueuePair interface inherits the methods of the [IUnknown](https://docs.microsoft.com/windows/desktop/api/unknwn/nn-unknwn-iunknown) interface. In addition, IND2QueuePair defines the following methods.

- [__Flush__](#flush-method) - Cancels all outstanding requests in the inbound and outbound completion queues.
- [__Send__](#send-method) - Sends data to a remote peer.
- [__Receive__](#receive-method) - Receives data from a remote peer.
- [__Read__](#read-method) - Initiates an RDMA Read request.
- [__Write__](#write-method) - Initiates an RDMA Write request.

__Remarks:__

If you do not retrieve the outstanding requests from the completion queue before releasing your last reference to this queue pair, you may get back requests from the completion queue that were issued on a now-closed queue pair.

## [IND2QueuePair::Flush](#flush-method)
Cancels all outstanding requests in the Receive and Initiator queues.
```
HRESULT Flush();
```

__Return Value__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Note that all pending requests may not have completed when this call returns.

__Remarks:__

The status for flushed requests is set to ND_CANCELED. 

Note that the [IND2Connector::Disconnect](./IND2Connector.md#disconnect-method) method implicitly flushes all outstanding requests. The Flush method gives the client the opportunity to stop I/O processing before disconnecting (if needed). 

If the completion queues are used by more than one queue pair, only this queue pair's requests are canceled.

## [ND2_SGE structure](#nd2-sge)
Represents local memory segments that are used in Send, Receive, Read, and Write requests.

```
typedef struct _ND2_SGE {
 void *Buffer;
 ULONG BufferLength;
 UINT32 MemoryRegionToken;
} ND2_SGE;
```

__Members:__

- __Buffer__

  The address of the buffer. The buffer is located within the memory region indicated by MemoryRegionToken. When using Send or Write requests with the ND_OP_FLAG_INLINE flag, the memory can be defined inside or outside a memory region.

- __BufferLength__

  The length, in bytes, of the buffer.

- __MemoryRegionToken__

  Opaque token value that is returned by a call to [IND2MemoryRegion::GetLocalToken](./IND2MemoryRegion.md#get-local-token). The token value controls hardware access to registered memory because the provider associated the token to the memory region when memory was registered.

## [IND2QueuePair::Send](#send-method)
Sends data to a remote peer.

```
HRESULT Send(
 [in] VOID *requestContext,
 [in] const ND2_SGE sge[],
 [in] ULONG nSge,
 [in] ULONG flags
);
```

__Parameters:__

- __requestContext__ [in] 

  A context value to associate with the request, returned in the RequestContext member of the [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure that is returned when the request completes.

- __sge__ [in] 

  An array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures that describe the buffers that contain the data to send to the peer. May be nullptr if nSge is zero, resulting in a zero-byte Send request and a corresponding zero-byte Receive request on the remote peer.  The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in] 

  The number of entries in the sge array. May be zero.

- __flags__ [in] 

  The following flags control the behavior of the Send request. You can specify one or more of the following flags:

  - __ND_OP_FLAG_SILENT_SUCCESS__

    The successful completion of the request does not generate a completion on the outbound completion queue. Requests that fail will generate a completion.

  - __ND_OP_FLAG_READ_FENCE__

    All prior Read requests must be complete before the hardware begins processing the Send request.

  - __ND_OP_FLAG_SEND_AND_SOLICIT_EVENT__

    You can use this flag if you issue multiple, related Send requests. Set this flag only on the last, related Send request. The peer will receive all the Send requests as normal—the only difference is that when they receive the last Send request (the request with the ND_OP_FLAG_SEND_AND_SOLICIT_EVENT flag set), the completion queue for the peer generates a notification. The notification is generated after the Receive request completes.

    This flag has no meaning to the receiver (peer) unless it has previously called the [IND2CompletionQueue::Notify](./IND2CompletionQueue.md#notify-method) method with the notification type set to ND_CQ_NOITFY_SOLICITED. Note that errors always satisfy a Notify request.

  - __ND_OP_FLAG_INLINE__

    Indicates that the memory referenced by the scatter/gather list should be transferred inline, and the MemoryRegionToken in the [ND2_SGE](./IND2QueuePair.md#nd2-sge) entries may be invalid. Inline requests do not need to limit the number of entries in the scatter/gather list to the maxInitiatorRequestSge that is specified when the queue pair was created. The amount of memory transferred inline must be within the inline data limits of the queue pair. Please see [IND2Adapter::Query](./IND2Adapter.md#query-method) method and MaxInlineDataSize field of [ND2_ADAPTER_INFO](./IND2Adapter.md#adapter-info).

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue that is associated with the queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The send request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of initiator requests allowed on the queue pair. The initiatorQueueDepth parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list of the request exceeded the number allowed on the queue pair. The maxInitiatorRequestSge parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_INVALID_PARAMETER_4__ - Invalid flags.

__Remarks:__

If the method fails, the queue pair can still process existing or future requests. However, if the [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method returns an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) with an error status, the connection is terminated.

## [IND2QueuePair::Receive](#receive-method)
Receives data from a peer.
```
HRESULT Receive(
 [in] VOID *requestContext,
 [in] ND2_SGE sge[],
 [in] ULONG nSge
);
```

__Parameters:__

- __requestContext__ [in]

  A context value to associate with the request, returned in the RequestContext member of the [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure that is returned when the request completes.

- __sge__ [in]

  A pointer to an array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures that describe the buffers to which data sent by the peer is written. May be nullptr if nSge is zero, resulting in a zero-byte Receive request.  The SGE array can be stack-based, and it is only used in the context of this call. 

- __nSge__ [in] 

  The number of entries in the sge array. May be zero.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of Receive requests allowed on this queue pair. The receiveQueueDepth parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list exceeded the number allowed on the queue pair. The maxReceiveRequestSge parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.

__Remarks:__

You can call this method at any time, the queue pair does not have to be connected.

You must post a Receive request before the peer posts a Send request. The buffers that you specify must be large enough to receive the sent data. If not, the Send request fails and the connection is terminated; the completion status for the Receive request is ND_BUFFER_OVERFLOW. If the Receive request fails, the queue pair can still process existing or future requests.

The protocol determines how many Receive requests you must post and the buffer size that is required for each post.

If the method fails, the queue pair can still process existing or future requests. However, if the [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method returns an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.


## [IND2QueuePair::Read](#read-method)
Initiates an RDMA Read request.
```
HRESULT Read(
 [in] VOID *requestContext,
 [in] ND2_SGE sge[],
 [in] ULONG nSge,
 [in] UINT64 remoteAddress,
 [in] UINT32 remoteToken,
 [in] ULONG flags
);
```

__Parameters:__

- __requestContext__ [in]

  A client-defined opaque value, returned in the RequestContext member of an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure that is filled in by [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method.

- __sge__ [in]

  An array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures that describe the local buffers to which data that is read from the remote memory is written. The length of all the scatter/gather entry buffers in the list determines how much data is read from the remote memory. The length of all the buffers must not extend past the boundary of the remote memory.

  May be nullptr if nSge is zero. You can use a zero byte RDMA Read request to detect if the target hardware is working. The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in]

  The number of entries in the scatter/gather list. May be zero.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, from which to read.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer granting read access to the remote memory specified by remoteAddress. Please see [IND2MemoryRegion::GetRemoteToken](./IND2MemoryRegion.md#get-remote-token).

- __flags__ [in] 

  The following flags control the behavior of the Read request. You can specify one or both of the flags:

  - __ND_OP_FLAG_SILENT_SUCCESS__

    The successful completion of the request does not generate a completion in the outbound completion queue. However, requests that fail will generate a completion in the completion queue.

  - __ND_OP_FLAG_READ_FENCE__

    All prior Read requests must be complete before the hardware begins processing this request.


__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with this queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request exceeded the number of outbound requests allowed on this queue pair. The initiatorQueueDepth parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list exceeded the number allowed on this queue pair. The maxInitiatorRequestSge parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_REMOTE_ERROR__ - The request tried to read beyond the size of the remote memory.

__Remarks:__

You can read the data in a single scatter/gather entry or in multiple entries. For example, you could use multiple entries to read message headers in one entry and the payload in the other.

If the method fails, the queue pair can still process existing or future requests. However, if the [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method returns an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.

## [IND2QueuePair::Write](#write-method)
Initiates a one-sided write operation.

```
HRESULT Write(
 [in] VOID *requestContext,
 [in] const ND2_SGE sge[],
 [in] ULONG nSge,
 [in] UINT64 remoteAddress,
 [in] UINT32 remoteToken,
 [in] ULONG flags
);
```

__Parameters:__

- __requestContext__ [in]

  A client-defined opaque value, returned in the RequestContext member of an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) structure that is filled in by  [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method.

- __sge__ [in]

  An array of [ND2_SGE](./IND2QueuePair.md#nd2-sge) structures that describe the local buffers to write to the remote memory. May be nullptr if nSge is zero, resulting in a zero-byte write. The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in]

  The number of entries in the scatter/gather list. May be zero.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, to which to write.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer that is granting Write access to the remote memory specified by remoteAddress.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, to which to write.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer that is granting Write access to the remote memory specified by remoteAddress. Please see [IND2MemoryRegion::GetRemoteToken](./IND2MemoryRegion.md#get-remote-token).

- __flags__ [in] 

  The following flags control the behavior of the Write operation. You can specify one or both of the following flags:
  - __ND_OP_FLAG_SILENT_SUCCESS__

    The successful completion of the Write request does not generate a completion on the associated completion queue. Work requests that fail processing always generate a work completion.

  - __ND_OP_FLAG_READ_FENCE__

    All prior Read requests must be complete before the hardware begins processing the Write request.

  - __ND_OP_FLAG_INLINE__

    Indicates that the memory referenced by the scatter/gather list should be transferred inline, and the MemoryRegionToken in the [ND2_SGE](./IND2QueuePair.md#nd2-sge) entries may be invalid. Inline requests do not need to limit the number of entries in the scatter/gather list to the maxInitiatorRequestSge parameter specified when the queue pair was created. The amount of memory transferred inline must be within the inline data limits of the queue pair.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of outbound requests allowed on the queue pair. The initiatorQueueDepth parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list of the request exceeded the number allowed on the queue pair. The maxInitiatorRequestSge parameter of the [IND2Adapter::CreateQueuePair](./IND2Adapter.md#create-queue-pair) method specifies the limit.
- __ND_INVALID_PARAMETER_6__ - Invalid flags.

__Remarks:__

If the method fails, the queue pair can still process existing or future requests. However, if the [IND2CompletionQueue::GetResults](./IND2CompletionQueue.md#get-results) method returns an [ND2_RESULT](./IND2CompletionQueue.md#nd2-result) with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.
