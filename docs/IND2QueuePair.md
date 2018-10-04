# INDQueuePair interface
Use to exchange data with a remote peer.
The INDAdapter::CreateQueuePair method returns this interface.

The INDQueuePair interface inherits the methods of the IUnknown interface.
In addition, INDQueuePair defines the following methods. 

- __Flush__ - Cancels all outstanding requests in the inbound and outbound completion queues.
- __Send__ - Sends data to a remote peer.
- __Receive__ - Receives data from a remote peer.
- __Bind__ - Binds a memory window to a buffer that is within the registered memory.
- __Invalidate__ - Invalidates a local memory window.
- __Read__ - Initiates an RDMA Read request.
- __Write__ - Initiates an RDMA Write request.

__Remarks:__

If you do not retrieve the outstanding requests from the completion queue before releasing your last reference to this queue pair, you may get back requests from the completion queue that were issued on a now-closed queue pair.

## INDQueuePair::Flush
Cancels all outstanding requests in the Receive and Initiator queues.
```
HRESULT Flush();
```

__Return Value__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Note that all pending requests may not have completed when this call returns.

__Remarks:__

The status for flushed requests is set to ND_CANCELED. 

Note that the INDConnector::Disconnect method implicitly flushes all outstanding requests. The Flush method gives the client the opportunity to stop I/O processing before disconnecting (if needed). 

If the completion queues are used by more than one queue pair, only this queue pair's requests are canceled.

## ND_SGE structure
Represents local memory segments that are used in Send, Receive, Read, and Write requests.

```
typedef struct _ND_SGE {
 void *Buffer;
 DWORD BufferLength;
 UINT32 MemoryRegionToken;
} ND_SGE;
```

__Members:__

- __Buffer__

  The address of the buffer. The buffer is located within the memory region indicated by MemoryRegionToken. When using Send or Write requests with the ND_OP_FLAG_INLINE flag, the memory can be defined inside or outside a memory region.

- __BufferLength__

  The length, in bytes, of the buffer.

- __MemoryRegionToken__

  Opaque token value that is returned by a call to INDMemoryRegion::GetLocalToken. The token value controls hardware access to registered memory because the provider associated the token to the memory region when memory was registered.

## INDQueuePair::Send
Sends data to a remote peer.

```
HRESULT Send(
 [in] VOID *requestContext,
 [in] const ND_SGE pSge[],
 [in] DWORD nSge,
 [in] DWORD flags
);
```

__Parameters:__

- __requestContext__ [in] 

  A context value to associate with the request, returned in the RequestContext member of the ND_RESULT structure that is returned when the request completes.

- __pSge__ [in] 

  A pointer to an array of ND_SGE structures that describe the buffers that contain the data to send to the peer. May be NULL if nSge is zero, resulting in a zero-byte Send request and a corresponding zero-byte Receive request on the remote peer.  The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in] 

  The number of entries in the pSge array. May be zero.

- __flags__ [in] 

  The following flags control the behavior of the Send request. You can specify one or more of the following flags:

  - __ND_OP_FLAG_SILENT_SUCCESS__ (0x00000001)

    The successful completion of the request does not generate a completion on the outbound completion queue. Requests that fail will generate a completion.

  - __ND_OP_FLAG_READ_FENCE__ (0x00000002)

    All prior Read requests must be complete before the hardware begins processing the Send request.

  - __ND_OP_FLAG_SEND_AND_SOLICIT_EVENT__ (0x00000004)

    You can use this flag if you issue multiple, related Send requests. Set this flag only on the last, related Send request. The peer will receive all the Send requests as normal—the only difference is that when they receive the last Send request (the request with the ND_OP_FLAG_SEND_AND_SOLICIT_EVENT flag set), the completion queue for the peer generates a notification. The notification is generated after the Receive request completes.

    This flag has no meaning to the receiver (peer) unless it has previously called the INDCompletionQueue::Notify method with the notification type set to ND_CQ_NOITFY_SOLICITED. Note that errors always satisfy a Notify request.

  - __ND_OP_FLAG_INLINE__ (0x00000020)

    Indicates that the memory referenced by the scatter/gather list should be transferred inline, and the MemoryRegionToken in the ND_SGE entries may be invalid. Inline requests do not need to limit the number of entries in the scatter/gather list to the maxInitiatorRequestSge that is specified when the queue pair was created. The amount of memory transferred inline must be within the inline data limits of the queue pair.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue that is associated with the queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The send request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of initiator requests allowed on the queue pair. The initiatorQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list of the request exceeded the number allowed on the queue pair. The maxInitiatorRequestSge parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_INVALID_PARAMETER_4__ - The request is too large to be sent inline.

__Remarks:__

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated.

## INDQueuePair::Receive
Receives data from a peer.
```
HRESULT Receive(
 [in]    VOID *requestContext,
 [in] ND_SGE pSge[],
 [in]    DWORD nSge
);
```

__Parameters:__

- __requestContext__ [in] 

  A context value to associate with the request, returned in the RequestContext member of the ND_RESULT structure that is returned when the request completes.

- __pSge__ [in] 

  A pointer to an array of ND_SGE structures that describe the buffers to which data sent by the peer is written. May be NULL if nSge is zero, resulting in a zero-byte Receive request.  The SGE array can be stack-based, and it is only used in the context of this call. 

- __nSge__ [in] 

  The number of entries in the pSge array. May be zero.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of Receive requests allowed on this queue pair. The receiveQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list exceeded the number allowed on the queue pair. The maxReceiveRequestSge parameter of the INDAdapter::CreateQueuePair method specifies the limit.

__Remarks:__

You can call this method at any time, the queue pair does not have to be connected.

You must post a Receive request before the peer posts a Send request. The buffers that you specify must be large enough to receive the sent data. If not, the Send request fails and the connection is terminated; the completion status for the Receive request is ND_BUFFER_OVERFLOW. If the Receive request fails, the queue pair can still process existing or future requests.

The protocol determines how many Receive requests you must post and the buffer size that is required for each post.

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.

## INDQueuePair::Bind
Binds a memory window to a buffer that is within the registered memory.
```
HRESULT Bind(
 [in]  VOID *requestContext,
 [in]  IUnknown *pMemoryRegion,
 [in]  IUnknown *pMemoryWindow,
 [in]  const VOID *pBuffer,
 [in]  SIZE_T cbBuffer,
 [in]  DWORD flags
);
```

__Parameters:__

- __requestContext__ [in] 

  A context value to associate with the request, returned in the RequestContext member of the ND_RESULT structure that is returned when the request completes.

- __pMemoryRegion__ [in] 

  The memory region to which to bind the memory window. The INDAdapter::CreateMemoryRegion method returns this interface.

- __pMemoryWindow__ [in] 

  The memory window to bind to the registered memory. The INDAdapter::CreateMemoryWindow method returns this interface.

- __pBuffer__ [in] 

  A buffer within the registered memory to which the memory window is bound. This buffer must be entirely within the bounds of the specified registered memory.

- __cbBuffer__ [in] 

  The size, in bytes, of the memory window buffer.

- __flags__ [in] 

  The following flags control the operations that the remote peer can perform against the memory. You can specify one or more of the following flags (you must specify at least ND_OP_FLAG_ALLOW_READ or ND_OP_FLAG_ALLOW_WRITE):

  - __ND_OP_FLAG_SILENT_SUCCESS__ (0x00000001)

    If the request completes successfully, do not generate an entry in the completion queue. Requests that fail will generate an entry in the completion queue.

  - __ND_OP_FLAG_READ_FENCE__ (0x00000002)

    All prior Read requests must be complete before the hardware processes new requests.

  - __ND_OP_FLAG_ALLOW_READ__ (0x00000008)

    A remote peer can perform Read operations against the memory window.

  - __ND_OP_FLAG_ALLOW_WRITE__ (0x00000010)

    A remote peer can perform Write operations against the memory window.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of outbound requests allowed on the queue pair. The initiatorQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_ACCESS_VIOLATION__ - The memory region does not allow the type of access requested for the memory window. ND_OP_FLAG_ALLOW_WRITE requires a memory region registered with ND_MR_FLAG_ALLOW_LOCAL_WRITE.

__Remarks:__

The remote peer can use the window for Read and Write operations after you send the window descriptor to the remote peer. Typically, you send the descriptor as part of a Send request.

The memory window can be bound to only one queue pair at a time. To unbind the window, you can call the INDQueuePair::Invalidate method.

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.

The token for accessing the remote memory can be retrieved through the INDMemoryWindow::GetRemoteToken method as soon as this method returns, before the Bind operation completion is reported through the INDCompletionQueue::GetResults method.

## INDQueuePair::Invalidate
Invalidates a local memory window. Any in-progress data transfers that reference the memory window will fail.
```
HRESULT Invalidate(
 [in] VOID *requestContext,
 [in] IUnknown *pMemoryWindow,
 [in] DWORD flags
);
```

__Parameters:__
- __requestContext__ [in] 

  A client-defined opaque value, returned in the RequestContext member of an ND_RESULT structure that is filled in by INDCompletionQueue::GetResults method.

- __pMemoryWindow__ [in] 

  An interface of the memory window to invalidate. Invalidating the window lets the window be bound again to a different region of registered memory.

- flags [in] 

  The following flags control the behavior of the operation. You can specify one or both of the flags:


  - __ND_OP_FLAG_SILENT_SUCCESS__ (0x00000001)

    The successful completion of the request does not generate a completion in the outbound completion queue. However, requests that fail will generate a completion in the completion queue.

  - __ND_OP_FLAG_READ_FENCE__ (0x00000002)

    All prior Read requests must be complete before the hardware begins processing the request.


__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with this queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of outbound requests allowed on the queue pair. The initiatorQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.

__Remarks:__

A queue pair can call this method after receiving notification from a peer that it is done with the memory window. 

Invalidating a memory window prevents further access to the window. Further references to the memory window in an RDMA Read or Write would result in a fatal error on the queue pair and the connection will be terminated.

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.

## INDQueuePair::Read 
Initiates an RDMA Read request.
```
HRESULT Read(
 [in]    VOID *requestContext,
 [in] ND_SGE pSge[],
 [in]    DWORD nSge,
 [in]    UINT64 remoteAddress,
 [in]    UINT32 remoteToken,
 [in]    DWORD flags
);
```

__Parameters:__

- __requestContext__ [in] 

A client-defined opaque value, returned in the RequestContext member of an ND_RESULT structure that is filled in by INDCompletionQueue::GetResults method.

- __pSge__ [in] 

  An array of ND_SGE structures that describe the local buffers to which data that is read from the remote memory is written. The length of all the scatter/gather entry buffers in the list determines how much data is read from the remote memory. The length of all the buffers must not extend past the boundary of the remote memory.
May be NULL if nSge is zero. You can use a zero byte RDMA Read request to detect if the target hardware is working. The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in]

  The number of entries in the scatter/gather list. May be zero.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, from which to read.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer granting read access to the remote memory specified by remoteAddress.

- __flags__ [in] 

  The following flags control the behavior of the Read request. You can specify one or both of the flags:

  - __ND_OP_FLAG_SILENT_SUCCESS__ (0x00000001)

    The successful completion of the request does not generate a completion in the outbound completion queue. However, requests that fail will generate a completion in the completion queue.

  - __ND_OP_FLAG_READ_FENCE__ (0x00000002)

    All prior Read requests must be complete before the hardware begins processing this request.


__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with this queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request exceeded the number of outbound requests allowed on this queue pair. The initiatorQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list exceeded the number allowed on this queue pair. The maxInitiatorRequestSge parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_REMOTE_ERROR__ - The request tried to read beyond the size of the remote memory.

__Remarks:__

You can read the data in a single scatter/gather entry or in multiple entries. For example, you could use multiple entries to read message headers in one entry and the payload in the other.

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated and any further requests are completed with ND_CANCELED status.

## INDQueuePair::Write
Initiates a one-sided write operation.

```
HRESULT Write(
 [in] VOID *requestContext,
 [in] const ND_SGE pSge[],
 [in] DWORD nSge,
 [in] UINT64 remoteAddress,
 [in] UINT32 remoteToken,
 [in] DWORD flags
);
```

__Parameters:__

- __requestContext__ [in] 

  A client-defined opaque value, returned in the RequestContext member of an ND_RESULT structure that is filled in by INDCompletionQueue::GetResults method.

- __pSge__ [in] 

  An array of ND_SGE structures that describe the local buffers to write to the remote memory. May be NULL if nSge is zero, resulting in a zero-byte write. The SGE array can be stack-based, and it is only used in the context of this call.

- __nSge__ [in] 

  The number of entries in the scatter/gather list. May be zero.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, to which to write.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer that is granting Write access to the remote memory specified by remoteAddress.

- __remoteAddress__ [in] 

  The remote address on the remote peer, in host order, to which to write.

- __remoteToken__ [in] 

  Opaque token value provided by the remote peer that is granting Write access to the remote memory specified by remoteAddress.

- __flags__ [in] 

  The following flags control the behavior of the Send operation. You can specify one or both of the following flags:
  - __ND_OP_FLAG_SILENT_SUCCESS__ 0x00000001

    The successful completion of the Send request does not generate a completion on the associated completion queue. Work requests that fail processing always generate a work completion.

  - __ND_OP_FLAG_READ_FENCE__ (0x00000002)

    All prior Read requests must be complete before the hardware begins processing the Send request.

  - __ND_OP_FLAG_INLINE__ (0x00000020)

    Indicates that the memory referenced by the scatter/gather list should be transferred inline, and the MemoryRegionToken in the ND_SGE entries may be invalid. Inline requests do not need to limit the number of entries in the scatter/gather list to the maxInitiatorRequestSge parameter specified when the queue pair was created. The amount of memory transferred inline must be within the inline data limits of the queue pair.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. Completion status will be returned through the outbound completion queue associated with the queue pair.
- __ND_CONNECTION_INVALID__ - The queue pair is not connected.
- __ND_BUFFER_OVERFLOW__ - The request referenced more data than is supported by the underlying hardware.
- __ND_NO_MORE_ENTRIES__ - The request would have exceeded the number of outbound requests allowed on the queue pair. The initiatorQueueDepth parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_DATA_OVERRUN__ - The number of scatter/gather entries in the scatter/gather list of the request exceeded the number allowed on the queue pair. The maxInitiatorRequestSge parameter of the INDAdapter::CreateQueuePair method specifies the limit.
- __ND_INVALID_PARAMETER_6__ - The request is too large to be written inline.

__Remarks:__

If the method fails, the queue pair can still process existing or future requests. However, if the INDCompletionQueue::GetResults method returns an ND_RESULT with an error status, the connection is terminated and further requests are completed with ND_CANCELED status.
