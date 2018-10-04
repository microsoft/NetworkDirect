# INDMemoryWindow interface
Memory windows are used to grant a remote peer Read and/or Write access to a region of your registered memory.
The INDAdapter::CreateMemoryWindow method returns this interface.

The INDMemoryWindow interface inherits from IUnknown.
In addition, INDMemoryWindow defines the following methods.

- __GetRemoteToken__ - Retrieves the remote token that allows remote access to the memory window. It is used by the connected QueuePair in Read and Write requests.

__Remarks:__

The memory window is created in an invalidated state, and it is not usable until you call the INDQueuePair::Bind method to define the size of the window and bind it to a queue pair and a memory region.

A memory window can be bound only to a single queue pair. After being invalidated (unbound), the window can be bound to a different queue pair or to different memory region. The adapter that is used to create the memory window, memory region, and queue pair must be the same.

Multiple windows can be bound to the same registered memory. Memory windows can overlap and have different access rights; the windows are independent.

The Flags parameter of the Bind method determines the operations that the remote peer can perform on the window.
Clients call the INDMemoryWindow::GetRemoteToken method to get the remote access token for a memory window after it has been bound to a queue pair.

A memory window becomes invalid when the owner of the memory window calls the INDQueuePair::Invalidate method.
Memory windows can be bound and invalidated repeatedly. 

__Implementation Notes:__

For InfiniBand devices, Type 2 memory windows should be used to implement INDMemoryWindow support. Type 1 memory windows can be used, but they are less secure than Type 2 memory windows, and testing may expose this limitation. 

## INDMemoryWindow::GetRemoteToken
Gets the remote memory token to send to remote peers to enable them to perform Read or Write requests against the window.

```
UINT32 GetRemoteToken();
```

__Return Value:__

The remote access token to send to connected peers. This value is opaque to the client. This value should be in network order to enable interoperability with other system architectures.
