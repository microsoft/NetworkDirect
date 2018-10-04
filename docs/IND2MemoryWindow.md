# IND2MemoryWindow interface
Memory windows are used to grant a remote peer Read and/or Write access to a region of your registered memory.
The [IND2Adapter::CreateMemoryWindow](./IND2Adapter.md#ind2adaptercreatememorywindow) method returns this interface.

The IND2MemoryWindow interface inherits from [IUnknown interface](https://docs.microsoft.com/windows/desktop/api/unknwn/nn-unknwn-iunknown). In addition, IND2MemoryWindow defines the following methods.

- [__GetRemoteToken__](#ind2memorywindowgetremotetoken) -  Retrieves the remote token that allows remote access to the memory window. It can be used by the connected [QueuePair](#./IND2QueuePair.md) in Read and Write requests.

__Remarks:__

The memory window is created in an invalidated state, and it is not usable until you call the [IND2QueuePair::Bind](ind2queuepairbind) method to define the size of the window and bind it to a queue pair and a memory region.

A memory window can be bound only to a single queue pair. After being invalidated (unbound), the window can be bound to a different queue pair or to different memory region. The adapter that is used to create the memory window, memory region, and queue pair must be the same.

Multiple windows can be bound to the same registered memory. Memory windows can overlap and have different access rights; the windows are independent. The flags parameter of the [IND2QueuePair::Bind](ind2queuepairbind) method determines the operations that the remote peer can perform on the window.

Clients call the [IND2MemoryWindow::GetRemoteToken](#ind2memorywindowgetremotetoken) method to get the remote access token for a memory window after it has been bound to a queue pair.

A memory window becomes invalid when the owner of the memory window calls the [IND2QueuePair::Invalidate](#ind2queuepairinvalidate) method. Memory windows can be bound and invalidated repeatedly.

__Implementation notes:__

For InfiniBand devices, Type 2 memory windows should be used to implement IND2MemoryWindow support. Type 1 memory windows can be used, but they are less secure than Type 2 memory windows, and testing may expose this limitation. 

## IND2MemoryWindow::GetRemoteToken
Gets the remote memory token to send to remote peers to enable them to perform Read or Write requests against the window.
```
UINT32 GetRemoteToken();
```
__Return Value:__

The remote access token to send to connected peers. This value is opaque to the client. This value should be in network order to enable interoperability with other system architectures.
