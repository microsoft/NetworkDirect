# IND2MemoryRegion interface
Use to make application buffers accessible to the NetworkDirect adapter, which will use them in I/O operations.

Note that only one memory buffer can be registered to a memory region object. The user should be able to register the same memory buffer multiple times, even for different access rights. However, one can not register two different memory buffers to the same memory region.

The IND2MemoryRegion interface inherits from [IND2Overlapped](./IND2Overlapped.md).
In addition, IND2MemoryRegion defines the following methods.

- [__Register__](#ind2memoryregionregister) - Registers an application-defined buffer.

- [__Deregister__](#ind2memoryregionderegister) - Deregisters a previously registered application-defined buffer.

- [__GetLocalToken__](#ind2memoryregiongetlocaltoken) - Retrieves the local token for the memory region, used in [ND2_SGE](./IND2QueuePair.md#nd2_sge-structure) structures as part of I/O requests.
- [__GetRemoteToken__](#ind2memoryregiongetremotetoken) - Retrieves the remote token that allows remote access to the memory region. It is used by the connected QueuePair in Read and Write requests.

__Remarks:__

Memory registrations are fundamental to data transfer operations with NetworkDirect adapters. Memory registrations map application buffers to a NetworkDirect adapter and provide the NetworkDirect adapter the ability to access application buffers directly without operating system involvement.

Memory registration is typically an expensive operation and the NetworkDirect SPI allows all memory registration and deregistration operations to be processed asynchronously in order to parallelize application processing with registration. Because memory region operations can be asynchronous, there is no opportunity for post-processing by the user-mode NetworkDirect provider libraries that support NetworkDirect adapters.

## IND2MemoryRegion::Register
Registers an application-defined buffer that is used for Send, Receive, Read, and Write requests. 
```
HRESULT Register(
 [in] const void *pBuffer,
 [in] SIZE_T cbBuffer,
 [in] ULONG flags,
 [in, out] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pBuffer__ [in]

  The application-defined buffer to register.

- __cbBuffer__ [in]

  The size, in bytes, of the application-defined buffer.

- __flags__ [in]

  The following flags control the behavior of the Register request. You can specify one or more of the following flags:

  - __ND_MR_FLAG_ALLOW_LOCAL_WRITE__

    Allow the adapter Write access to the memory region.

  - __ND_MR_FLAG_ALLOW_REMOTE_READ__

    Enable Read access to the memory region by any connected peer.

  - __ND_MR_FLAG_ALLOW_REMOTE_WRITE__

    Enable write access to the memory region by any connected peer.

  - __ND_MR_FLAG_RDMA_READ_SINK__

    Enable access to the memory region to be used as a sink for RDMA read operation.

  - __ND_MR_FLAG_DO_NOT_SECURE_VM__
 
    Instructs the provider to not secure the virtual memory range by calling the [MmSecureVirtualMemory](https://docs.microsoft.com/windows-hardware/drivers/ddi/content/ntddk/nf-ntddk-mmsecurevirtualmemory) function. Applications that are using [AWE memory management](https://docs.microsoft.com/windows/desktop/memory/address-windowing-extensions) functions must specify this flag to register AWE memory. Securing memory is only required when implementing a cache of memory registrations. Memory registrations that are not going to be cached (where the lifetime of the memory region is always less than the lifetime of the buffer) do not need to be secured.

- __pOverlapped__ [in, out]

A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that must remain valid for the duration of the operation.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_PENDING__ - The NetworkDirect adapter is in process of registering the memory. Completion is reported asynchronously.
- __ND_INSUFFICIENT_RESOURCES__ - There were not enough hardware resources to register the memory.
- __ND_ACCESS_VIOLATION__ - The specified buffer or buffer size was not valid.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.
- __ND_INVALID_PARAMETER__ - The buffer exceeds the size supported by the adapter. For details, see the MaxRegistrationSize member of the [ND2_ADAPTER_INFO](./IND2Adapter.md#nd2_adapter_info-structure) structure.


__Remarks:__

The MaxRegistrationSize member of the [ND2_ADAPTER_INFO](./IND2Adapter.md#nd2_adapter_info-structure) structure contains the maximum application-defined buffer size that can be registered with the adapter. To get the adapter information, call the [IND2Adapter::Query](./IND2Adapter.md#ind2adapterquery) method.

You can allocate and register your memory before connecting, or you can start the connection process and then register the memory before calling [IND2Connector::Accept](./IND2Connector.md#ind2connectoraccept) or [IND2Connector::CompleteConnect](./IND2Connector.md#ind2connectorcompleteconnect).

The registered memory's scope is the adapter object. It can be used on any objects within the adapter, for example, multiple queue pairs can both use the registered memory.

## IND2MemoryRegion::Deregister
Removes the memory registration for an application-defined buffer.
```
HRESULT Deregister (
 [in] OVERLAPPED *pOverlapped
);
```

__Parameters:__

- __pOverlapped__ [in]

  A pointer to an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that must remain valid for the duration of the operation.


__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_PENDING__ - The memory region is in the process of being freed. Completion will be reported asynchronously.
- __ND_DEVICE_BUSY__ - The memory region still has memory windows bound to it.

__Remarks:__

It is very important that Deregister is called before the memory region is released, otherwise the registered memory buffer will stay locked and count against your process.

## IND2MemoryRegion::GetLocalToken
Gets the local memory token to allow the region to be referenced in [ND2_SGE](./IND2QueuePair.md#nd2_sge-structure) structures that are passed to Send, Receive, Read, and Write requests.

```
UINT32 GetLocalToken();
```

__Return Value:__

The local token to use in [ND2_SGE](./IND2QueuePair.md#nd2_sge-structure) structures.

## IND2MemoryRegion::GetRemoteToken
Gets the remote memory token to send to remote peers to enable them to perform Read or Write requests against the region.

```
UINT32 GetRemoteToken();
```

__Return Value:__

The remote access token to send to connected peers. This value is opaque to the client. This value should be in network order to enable interoperability with other system architectures.
