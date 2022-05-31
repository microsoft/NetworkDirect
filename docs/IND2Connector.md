# IND2Connector interface
Use to connect to a remote peer.

The [IND2Adapter:CreateConnector](./IND2Adapter.md#ind2adaptercreateconnector) method returns this interface.

The IND2Connector interface inherits from [IND2Overlapped](./IND2Overlapped.md).
In addition, IND2Connector defines the following methods. 

- [__Bind__](#ind2listenerbind) - Binds the queue pair to a local address.
- [__Connect__](#ind2connectorconnect) - Connects the queue pair to a listening peer.
- [__CompleteConnect__](#ind2connectorcompleteconnect) - Completes the connection request initiated by a previous call to the Connect method.
- [__Accept__](#ind2connectoraccept) - Accepts a pending connection request and assigns a queue pair to use in the data exchange.
- [__Reject__](#ind2connectorreject) - Rejects a pending connection request.
- [__GetReadLimits__](#ind2connectorgetreadlimits)- Retrieves the inbound and outbound read limits of the peer.
- [__GetPrivateData__](#ind2connectorgetprivatedata)- Retrieves the private data that the peer sends when it accepts or rejects the connection request.
- [__GetLocalAddress__](#ind2connectorgetlocaladdress) - Retrieves the address of the local NetworkDirect adapter.
- [__GetPeerAddress__](#ind2connectorgetpeeraddress) - Retrieves the address of the peer's NetworkDirect adapter.
- [__NotifyDisconnect__](#ind2connectornotifydisconnect) - Notifies the caller that an established connection was disconnected.
- [__Disconnect__](#ind2connectordisconnect) - Disconnects the queue pair from the peer.

__Remarks:__

The remote peer gets the connecting peer's requested inbound and outbound read limits by calling the [GetReadLimits](#ind2connectorgetreadlimits) method. When the remote peer calls [Accept](#ind2connectoraccept), Accept transfers the (potentially updated) limits back to the connecting peer. The connecting peer retrieves the limits by calling the [GetReadLimits](#ind2connectorgetreadlimits) method.

The connecting peer can accept the limits and call the [CompleteConnect](#ind2connectorcompleteconnect) method to complete the connection or call the [Reject](#ind2connectorreject) method to reject the connection. When the connecting peer calls [CompleteConnect](#ind2connectorcompleteconnect), the limit values in its queue pair are updated with the new limits.

## IND2Connector::Bind
Binds the queue pair to a local address.
```
HRESULT Bind(
 [in] const struct sockaddr *pAddress,
 [in] ULONG cbAddress
);
```

__Parameters:__

- __pAddress__ [in]

  A sockaddr buffer that specifies the local address to use for the connection. Typically, this is a sockaddr_in structure for IPv4 addresses and a sockaddr_in6 structure for IPv6 addresses.
  
  If the sin_port or sin6_port member is specified as zero, the provider assigns a unique port number to the application.

- __cbAddress__ [in]

  Size, in bytes, of the pAddress buffer.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues. Unless otherwise noted, errors do not allow the connector to be used to retry the connection attempt.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_CANCELED__- The queue pair was removed.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.
- __ND_SHARING_VIOLATION__ - The local address is already in use.
- __ND_TOO_MANY_ADDRESSES__ - The client specified a local port number of zero, and the NetworkDirect provider was unable to allocate a port from the ephemeral port space (ports 49152-65535.)
- __ND_ADDRESS_ALREADY_EXISTS__ - A connection with the combination of local address, local port, remote address, and remote port already exists.

## IND2Connector::Connect
Connects the queue pair to a listening peer.
```
HRESULT Connect(
 [in] IUnknown *pQueuePair,
 [in] const struct sockaddr *pDestAddress,
 [in] ULONG cbDestAddress,
 [in] ULONG inboundReadLimit,
 [in] ULONG outboundReadLimit,
 [in, optional] const void *pPrivateData,
 [in] ULONG cbPrivateData,
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pQueuePair__ [in]

  An interface that identifies the queue pair to use for the connection.

- __pDestAddress__ [in]

  A sockaddr buffer that specifies the address of the peer to connect to. Typically, this is a sockaddr_in structure for IPv4 addresses and a sockaddr_in6 structure for IPv6 addresses.

- __cbDestAddress__ [in]

  Size, in bytes, of the pDestAddress buffer.

- __inboundReadLimit__ [in] 

  The maximum number of in-flight RDMA Read requests the caller will support from the peer on this connection. This value may be zero if you do not support Read requests from the peer.

- __outboundReadLimit__ [in] 

  The maximum number of in-flight RDMA Read requests the caller would like to initiate on this connection. This value may be zero if you are not issuing Read requests to the peer.

- __pPrivateData__ [in, optional]

Private data that is sent with the connection request. May be nullptr if PrivateDataLength is zero.
To get this data, the peer calls the [IND2Connector::GetPrivateData](#ind2connectorgetprivatedata) method.

- __cbPrivateData__ [in] 

  Length, in bytes, of the pPrivateData buffer. May be zero.

- __pOverlapped__ [in, out] 

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is used to indicate completion of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues. Unless otherwise noted, errors do not allow the connector to be used to retry the connection attempt.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_PENDING__ - The request is pending and will be completed when the listening peer accepts the connection request.
- __ND_INVALID_BUFFER_SIZE__ - The specified private data length exceeded the capabilities of the underlying NetworkDirect hardware. The limits are reported in the [ND2_ADAPTER_INFO](./IND2Adapter.md#nd2_adapter_info-structure) structure (see [IND2Adapter::Query](./IND2Adapter.md#ind2adapterquery)).
- __ND_CANCELED__- The queue pair was removed.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.
- __ND_CONNECTION_ACTIVE__ - This queue pair is already connected to another peer.
- __ND_NETWORK_UNREACHABLE__ - The remote network was not reachable. The connection attempt may be retried.
- __ND_HOST_UNREACHABLE__ - The remote system was not reachable. The connection attempt may be retried.
- __ND_CONNECTION_REFUSED__ - The remote system refused the connection request. This can be due to lack of listener, backlog limits, or the peer actively rejecting the connection request. The connection attempt may be retried.
- __ND_IO_TIMEOUT__ - The connection request timed out. The connection attempt may be retried. Timeout values are selected by NetworkDirect providers to match their respective network characteristics.
- __ND_ACCESS_VIOLATION__ - The pPrivateData buffer was not valid for the size specified in PrivateDataLength.

__Implementation Notes:__

Providers are responsible for transmitting the inbound and outbound read limits. Callers may specify inboundReadLimit and outboundReadLimit values greater than the adapter limits, in which case providers silently limit the values to their adapter capabilities. NetworkDirect providers should avoid timing out connections because the listening peer did not call [IND2Connector::Accept](#ind2connectoraccept), there is no timeliness requirement placed on client applications.

__Remarks:__

The peer to that you are trying to connect must have issued an [IND2Listener::GetConnectionRequest](IND2Listener.md#ind2listenergetconnectionrequest) request to receive your connection request. The connection is performed asynchronously. If the connection fails, the queue pair is not affected, and it remains in the same state as when the call was made.

When the request completes, you must call the [IND2Connector::CompleteConnect](#ind2connectorcompleteconnect) method to complete the connection process or the [IND2Connector::Reject](#ind2connectorreject) method to reject the connection. The application protocol defines the content and format of the private data exchange. Typically, the private data contains information for negotiating the connection with the peer.

To abandon a connection attempt, you can release the connector or call the [IND2Overlapped::CancelOverlappedRequests](./IND2Overlapped.md#ind2overlappedcanceloverlappedrequests) method. The peer abandons the connection attempt by rejecting the request. Either side of a connection can terminate the connection by calling the [IND2Connector::Disconnect](#ind2connectordisconnect) method. When a queue pair is disconnected, all requests are flushed. The completion status is ND_CANCELED for all outstanding requests on both sides of the connection.

Providers are expected to manage a separate port space for NetworkDirect connections from the host TCP port space, unless the underlying hardware must share the host TCP port space, in which case a port mapping service must be provided by the provider to map NetworkDirect ports to host TCP ports. Local port numbers should not be shared between connections for connections established through this method.


## IND2Connector::CompleteConnect
Completes the connection request initiated by a previous call to the [IND2Connector::Connect](#ind2connectorconnect) method. Call this method after the peer accepts your connection request (and your connection request completes).

```
HRESULT CompleteConnect(
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pOverlapped__ [in, out] 

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is used to indicate completion of the operation. The variable must remain valid for the duration of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues. If the request fails, either immediately or asynchronously, the IND2Connector object cannot be reused.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_CONNECTION_INVALID__ - The queue pair is not connecting.
- __ND_PENDING__ - The request is pending and will be completed when the operation completes.
- __ND_DEVICE_REMOVED__ - The underlying local NetworkDirect adapter was removed from the system.
- __ND_IO_TIMEOUT__ - Connection establishment timed out. This is not an indication of a catastrophic or permanent failure, but it ends connection establishment for this particular connector.

__Implementation Notes:__

NetworkDirect providers should avoid timing out connections when the client does not call this method in a timely manner because there is no timeliness requirement placed on client applications.

__Remarks:__

You must call this method to complete the connection. If you do not call this method, the connection may time out.

## IND2Connector::Accept
Accepts a pending connection request and assigns a queue pair to use in the data exchange.
```
HRESULT Accept(
 [in] IUnknown *pQueuePair,
 [in] ULONG inboundReadLimit,
 [in] ULONG outboundReadLimit,
 [in, optional] const VOID *pPrivateData,
 [in] ULONG cbPrivateData,
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pQueuePair__ [in] 

  An interface that defines the queue pair to connect to the peer's queue pair. The queue pair must have been created by using the same IND2Adapter instance as this instance of the Connector object.

- __inboundReadLimit__ [in] 

  The maximum number of in-flight RDMA Read requests that the caller will support from the peer on this connection. This value may be zero if you do not support Read requests from the peer.

- __outboundReadLimit__ [in] 

  The maximum number of in-flight RDMA Read requests that the caller would like to initiate on this connection. This value may be zero if you are not issuing Read requests to the peer.

- __pPrivateData__ [in, optional] 

  Private data to transmit to the peer. May be nullptr if cbPrivateData is zero.

- __cbPrivateData__ [in] 

  The size, in bytes, of the pPrivateData buffer.

- __pOverlapped__ [in, out] 

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is used to indicate completion of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues. If the request fails, either immediately or asynchronously, the IND2Connector object cannot be reused.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_PENDING__ - The request is pending and will be completed when the connection is fully established.
- __ND_CANCELED__ - The queue pair was removed. 
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.
- __ND_CONNECTION_ACTIVE__ - The specified queue pair is already connected.
- __ND_CONNECTION_ABORTED__ - The connecting peer abandoned the connection establishment.
- __ND_ACCESS_VIOLATION__ - The pPrivateData buffer is not valid for the size specified in cbPrivateData.
- __ND_INVALID_BUFFER_SIZE__ - The specified private data buffer length exceeds the capabilities of the underlying NetworkDirect hardware. The limits are reported in the [ND2_ADAPTER_INFO](./IND2Adapter.md#nd2_adapter_info-structure) structure (see [IND2Adapter::Query](./IND2Adapter.md#ind2adapterquery)).
- __ND_IO_TIMEOUT__ - The peer did not call the [IND2Connector::CompleteConnect](#ind2connectorcompleteconnect) method to complete the connection.

__Implementation Notes:__
Providers are responsible for transmitting the inbound and outbound read limits. Callers may specify inboundReadLimit and outboundReadLimit values greater than the adapter limits or greater than the values offered by the connecting peer, in which case providers silently limit the values to the lower of their adapter capabilities or the values offered by the connecting peer.

NetworkDirect providers should avoid timing out connections when the client does not call this method in a timely manner because there is no timeliness requirement placed on client applications.

__Remarks:__

Calling this method causes the peer's connection request ([IND2Connector::Connect](#ind2connectorconnect)) to complete.
A queue pair can be connected to only one other queue pair at a time.

## IND2Connector::Reject
Rejects a pending connection request.
```
HRESULT Reject(
 [in, optional] const VOID *pPrivateData,
 [in] ULONG cbPrivateData
);
```

__Parameters:__

- __pPrivateData__ [in, optional] 

  Private data to transmit to the peer. May be nullptr if cbPrivateData is zero.

- __cbPrivateData__ [in] 

  The size, in bytes, of the pPrivateData buffer.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_PENDING__ - The request is pending and will be completed when the connection is fully established.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.
- __ND_CONNECTION_ABORTED__ - The connecting peer abandoned the connection establishment.
- __ND_ACCESS_VIOLATION__ - The pPrivateData buffer is not valid for the size given in PrivateDataLength.
- __ND_INVALID_BUFFER_SIZE__ - The specified buffer length exceeds the capabilities of the underlying NetworkDirect hardware. The limits are reported in the [ND2_ADAPTER_INFO](./IND2Adapter.md#nd2_adapter_info-structure) structure (see [IND2Adapter::Query](./IND2Adapter.md#ind2adapterquery)).

__Remarks:__

The remote peer or connecting peer can call this method to reject the connection. The connecting peer would call this method after determining that the inbound and outbound Read limits that the remote peer specifies are not acceptable.

## IND2Connector::GetReadLimits
Retrieves the read limits of the peer.
```
HRESULT GetReadLimits(
 [out, optional] ULONG *pInboundReadLimit,
 [out, optional] ULONG *pOutboundReadLimit,
);
```
__Parameters:__

- __pInboundReadLimit__ [out, optional] 

  Pointer to the value of outboundReadLimit that is specified by the peer when calling [IND2Connector::Connect](#ind2connectorconnect), or [IND2Connector::Accept](#ind2connectoraccept). The value is limited by the NetworkDirect hardware capabilities on both ends of the connection, and the value can be less than or equal to the value that is specified by the remote peer. Use this value to determine whether to accept or complete a connection.

- __pOutboundReadLimit__ [out, optional] 

  Pointer to the value of inboundReadLimit that is specified by the peer when calling [IND2Connector::Connect](#ind2connectorconnect), or [IND2Connector::Accept](#ind2connectoraccept). The value is limited by the NetworkDirect hardware capabilities on both ends of the connection, and the value may be less than or equal to the value that is specified by the remote peer. Use this value to determine whether to accept or complete a connection.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_CONNECTION_INVALID__ - The connector is not connected.
- __ND_UNSUCCESSFUL__ - The operation failed.

__Remarks:__

The connecting peer calls this method after the [IND2Connector::Connect](#ind2connectorconnect) call completes and before calling the [IND2Connector::CompleteConnect](#ind2connectorcompleteconnect) method, as well as after the [IND2Listener::GetConnectionRequest](./IND2Listener.md#ind2listenergetconnectionrequest) call completes and before calling [IND2Connector::Accept](#ind2connectoraccept).


## IND2Connector::GetPrivateData
Retrieves the private data that the peer sends when it accepts or rejects the connection request.
```
HRESULT GetPrivateData(
 [out, optional] VOID *pPrivateData,
 [in, out] ULONG *pcbPrivateData
);
```

__Parameters:__

- __pPrivateData__ [out, optional]

  The buffer to receive the private data. May be nullptr if pcbPrivateData is zero.

- __pcbPrivateData__ [in, out]

  The size, in bytes, of the pPrivateData buffer. If the buffer is too small, the method fails with ND_BUFFER_OVERFLOW, and this parameter is set to the required buffer size. If the buffer is too big, the method sets this parameter to the size used on output.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_BUFFER_OVERFLOW__ - The pPrivateData buffer is not large enough to hold all the private data. Partial data was returned up to the size of the buffer. The pcbPrivateData is updated with the required buffer size.
- __ND_CONNECTION_INVALID__ - The connector is not connected.
- __ND_UNSUCCESSFUL__ - The operation failed.

__Remarks:__

The connecting peer calls this method after the [IND2Connector::Connect](#ind2connectorconnect) call completes and before calling the [IND2Connector::CompleteConnect](#ind2connectorcompleteconnect) method, as well as after the [IND2Listener::GetConnectionRequest](./IND2Listener.md#ind2listenergetconnectionrequest) call completes and before calling [IND2Connector::Accept](#ind2connectoraccept).

## IND2Connector::GetLocalAddress
Retrieves the connector's address, which includes the connector's adapter address and port number. To succeed, the connector must be connected or in the process of connecting.
```
HRESULT GetLocalAddress(
 [out, optional] struct sockaddr *pAddress,
 [in, out] ULONG *pcbAddress
);
```

__Parameters:__

- __pAddress__ [out, optional] 

  A sockaddr buffer that will receive the local address. May be nullptr if pcbAddress is zero.

- __pcbAddress__ [in, out] 

  The size, in bytes, of the pAddress buffer. If the buffer is too small, the method fails with ND_BUFFER_OVERFLOW and sets this parameter to the required buffer size. If the buffer is too big, the method sets this parameter to the size that is used on output.

__Return Value:__

  When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_BUFFER_OVERFLOW__ - The pAddress buffer is not large enough to hold the address, and the buffer was not modified. The pcbAddress parameter is updated with the required buffer size.
- __ND_CONNECTION_INVALID__ - The connector is not connected or not in the process of connecting.

## IND2Connector::GetPeerAddress
Retrieves the address of the peer's NetworkDirect queue pair. To succeed, the queue pair must be connected.

```
HRESULT GetPeerAddress(
 [out, optional] struct sockaddr *pAddress,
 [in, out] ULONG *pcbAddress
);
```

__Parameters:__

- __pAddress__ [out, optional] 

  A sockaddr buffer that will receive the remote address. May be nullptr if pcbAddress is zero.

- __pcbAddress__ [in, out] 

  The size, in bytes, of the pAddress buffer. If the buffer is too small, the method fails with ND_BUFFER_OVERFLOW and sets this parameter to the required buffer size. If the buffer is too big, the method sets this parameter to the size that is used on output.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_BUFFER_OVERFLOW__ - The pAddress buffer is not large enough to hold the address—the buffer was not modified. The pcbAddress parameter is updated with the required buffer size.
- __ND_CONNECTION_INVALID__ - The connector is not connected.

## IND2Connector::NotifyDisconnect
Notifies the caller that an established connection was disconnected.

```
HRESULT NotifyDisconnect(
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pOverlapped__ [in, out] 

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is used to indicate a disconnection request.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_PENDING__ - The request is pending.
- __ND_CONNECTION_INVALID__ - The connector was never connected.

Asynchronously, the method can return ND_CANCELED (ND_CANCELED should only be returned when a caller calls [IND2Overlapped::CancelOverlappedRequests](./IND2Overlapped.md#ind2overlappedcanceloverlappedrequests)).

__Implementation Notes:__

A queue pair should not have its outstanding requests completed with a canceled status when receiving a disconnection request. The only action a provider should take is to complete the NotifyDisconnect request, if any.

## IND2Connector::Disconnect
Disconnects the connector and associated queue pair from the peer.
```
HRESULT Disconnect(
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pOverlapped__ [in, out] 

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that is used to indicate completion of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_PENDING__ - The request is pending and will be completed when the listening peer is disconnected.
- __ND_CANCELED__ - The queue pair was removed.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.
- __ND_CONNECTION_INVALID__ - The connector is not connected.

__Remarks:__

This method implicitly flushes all outstanding requests on the connector’s associated queue pair.  You cannot reuse the queue pair after the request completes. Either side of the connection can terminate the connection; the disconnect handshake is protocol dependent.

__Implementation Notes:__

Calling the IND2QueuePair::Release method on the queue pair without first disconnecting is valid. If the holder of the last reference does not disconnect before releasing, the object should implicitly disconnect. Disconnecting should implicitly flush pending requests (the pending requests will complete in the completion queue with an ND_CANCELED status).
