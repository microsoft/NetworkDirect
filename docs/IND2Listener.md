# INDListen interface
Use to listen for connection requests from peers.
The Listen method returns this interface.

The INDListen interface inherits from INDOverlapped.
In addition, INDListen defines the following methods. 

- __Listen__ - Starts listening for incoming connection requests.
- __GetLocalAddress__ - Retrieves the address of the listen object.
- __GetConnectionRequest__ - Retrieves the handle for a pending connection request.

__Remarks:__

NetworkDirect uses an active/passive model for establishing a connection between peers; the passive side listens for connection requests from the active side.

The active side makes the following calls:
1.	Receive (can occur before or after Connect)
2.	Connect (completes after the peer calls Accept or Reject)
3.	CompleteConnect
4.	Send (begins the first Send/Receive exchange)

The passive side makes the following calls:
1.	GetConnectionRequest (receives the Connect request)
2.	Receive (posts a request to Receives the first Send request)
3.	Accept

The iWARP specification requires that the active side always initiates a Send request to the passive side before the passive side can issue a Send request to the active side. After the connection is established, the passive side cannot issue a Send request until it has received its first message from the active side.This requires that the passive side always issues at least one Receive request, and waits for it to complete before it can begin issuing Send requests (even if it issues only Send requests after that time).

Either side of a connection can terminate the connection by calling the INDConnector::Disconnect method. When a queue pair is disconnected, all requests are flushed. The completion status is ND_CANCELED for all outstanding Send and Receive requests on both sides of the connection.

NetworkDirect providers should not make any assumptions about client response times to asynchronous connection establishment events, and they should make every effort to accommodate clients that experience delays in processing connection-related I/O completions.

## INDListen::Listen
Starts listening for incoming connection requests.
```
HRESULT Listen(
 [in]      const struct sockaddr *pAddress,
 [in]      SIZE_T cbAddress,
 [in]       SIZE_T Backlog
);
```

__Parameters:__

- __pAddress__ [in] 

  A sockaddr buffer that specifies the local address on which to listen for connection requests. Typically, this is a sockaddr_in structure for IPv4 addresses and a sockaddr_in6 structure for IPv6 addresses.

  If the sin_port or sin6_port member is specified as zero, the provider assigns a unique port number to the application. If the sin_addr or sin6_addr is INADDR_ANY or IN6ADDR_ANY the listen object can be multi-homed if the NetworkDirect adapter supports multiple IP addresses, but it cannot span multiple NetworkDirect adapters.

- __cbAddress__ [in] 

  Size, in bytes, of the pAddress buffer.

- __Backlog__ [in] 

  The maximum number of pending connection requests to maintain for the listen request. Set to zero to indicate no limit. 

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_SHARING_VIOLATION__ - The specified address is already in use on this NetworkDirect adapter.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough resources to complete the request.
- __ND_TOO_MANY_ADDRESSES__ - The client specified a local port number of zero, and the NetworkDirect provider was unable to allocate a port from the ephemeral port space (ports 49152-65535.)

__Implementation Notes:__

NetworkDirect service providers may restrict the range of the backlog and silently apply limits. There is no provision for retrieving the actual backlog value.

__Remarks:__

The server side of a client-server application listens for connection requests from the client side.

## INDListen::GetLocalAddress
Retrieves the listen object’s address, which includes the adapter address and port number. To succeed, the listen object must be listening.
```
HRESULT GetLocalAddress(
 [out, optional] struct sockaddr *pAddress,
 [in, out]    SIZE_T *pcbAddress
);
```

__Parameters:__

- __pAddress__ [out, optional] 

  A sockaddr buffer that will receive the local address. May be NULL if pcbAddress is zero.

- __pcbAddress__ [in, out] 

  The size, in bytes, of the pAddress buffer. If the buffer is too small, the method fails with ND_BUFFER_OVERFLOW and sets this parameter to the required buffer size. If the buffer is too big, the method sets this parameter to the size that is used on output.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded.
- __ND_BUFFER_OVERFLOW__ - The pAddress buffer is not large enough to hold the address, and the buffer was not modified. The pcbAddress parameter is updated with the required buffer size.
- __ND_INVALID_DEVICE_STATE__ - The listen object is not listening.

## INDListen::GetConnectionRequest
Retrieves the handle for a pending connection request.

```
HRESULT GetConnectionRequest(
 [in, out] IUnknown *pConnector,
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pConnector__ [in, out] 

  An interface to the connector request that is returned by a previous call to INDAdapter::CreateConnectionRequest.

- __pOverlapped__ [in, out] 

  A pointer to an OVERLAPPED structure that is used to indicate completion of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. 
- __ND_PENDING__ - The request is pending, and it will be completed when the listening peer accepts the connection request.
- __ND_CANCELED__ - The listen request was closed.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system.

__Remarks:__

You can issue multiple GetConnectionRequest requests to prepare for future connection requests. The overlapped requests complete one at a time for each connection request that is received. You detect completion of the overlapped request like you do for any other overlapped operation (for more information, see the Remarks for INDAdapter::GetFileHandle).
