# IND2Overlapped interface
This interface is the base interface that is used for interfaces on which overlapped operations can be performed.

The IND2Overlapped interface inherits the methods of the [IUnknown interface](https://docs.microsoft.com/windows/desktop/api/unknwn/nn-unknwn-iunknown).
In addition, IND2Overlapped defines the following methods:

- [__CancelOverlappedRequests__](#cancel-overlapped-requests) - Cancels all in-progress overlapped requests.
- [__GetOverlappedResult__](#get-overlapped-result) - Retrieves the result of an overlapped request.

__Remarks:__

Many operations in the NetworkDirect SPI take an [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure as input. These operations provide the same functionality as Win32 asynchronous I/O operations. For added flexibility, this class also exposes the adapter's underlying file handle on which asynchronous operations are performed.

Providers never invoke callbacks into client code. All notifications are explicitly requested by the client.

## [IND2Overlapped::CancelOverlappedRequests](#cancel-overlapped-requests)
Cancels all overlapped requests that are in-progress.

```
HRESULT CancelOverlappedRequests();
```

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation succeeded. All requests for asynchronous notifications have been completed with ND_CANCELED. Note that completions for such notification requests may not have been delivered or processed.
- __ND_UNSUCCESSFUL__ - All outstanding operations could not be canceled.

## [IND2Overlapped::GetOverlappedResult](#get-overlapped-result)
Retrieves the result of an overlapped request. 

```
HRESULT GetOverlappedResult(
 [in]  OVERLAPPED *pOverlapped,
 [in]  BOOL wait
);
```

__Parameters:__
- __pOverlapped__ [in] 

  The [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure that was specified when the overlapped operation started.
- __wait__ [in] 

  If this parameter is _true_, the function does not return until the operation completes. If this parameter is _false_, and the operation is still pending, the function returns ND_PENDING. Applications that set this parameter to _true_ must have specified a valid event handle in the [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) structure when initiating the I/O request.

__Return Value:__

When you implement this method, you should return the following return values. If you return others, try to use well-known values to aid in debugging issues.

- __ND_SUCCESS__ - The operation completed successfully.
- __ND_PENDING__ - The operation is still outstanding and has not yet completed.
- __ND_CANCELED__ - The NetworkDirect adapter was closed, or a new request for asynchronous notification was requested.
- __ND_DEVICE_REMOVED__ - The underlying NetworkDirect adapter was removed from the system. Only cleanup operations on the NetworkDirect adapter will succeed.

__Remarks:__

Clients are required to call this method to determine the final status of an operation, even if they detected completion through the Win32 [GetOverlappedResult](https://docs.microsoft.com/windows/desktop/api/ioapiset/nf-ioapiset-getoverlappedresult) function, [GetQueuedCompletionStatus](https://msdn.microsoft.com/library/windows/desktop/aa364986(v=vs.85).aspx), or other native Win32 APIs. This requirement allows providers to perform post-processing as necessary.

The behavior of this method is identical to that of the Win32 [GetOverlappedResult](https://docs.microsoft.com/windows/desktop/api/ioapiset/nf-ioapiset-getoverlappedresult) function. You can call this function; however, you will not get the NetworkDirect return value. Specifically, instead of returning the status through GetLastError mechanisms, [IND2Overlapped::GetOverlappedResult](#get-overlapped-result) simply returns the actual result of the operation. When calling the Win32 [GetOverlappedResult](https://docs.microsoft.com/windows/desktop/api/ioapiset/nf-ioapiset-getoverlappedresult) function, the value of the parameter specified by _lpNumberOfBytesTransferred_ is specific to the provider implementationand does not convey any meaning to the client.

Multiple instances of this function w.r.t. the same IND2Overlapped interface can be called concurrently as long as the _pOverlapped_ parameters point to different [OVERLAPPED](https://docs.microsoft.com/windows/desktop/api/minwinbase/ns-minwinbase-_overlapped) objects. To construct a parallel system using this feature, one may have multiple threads waiting for IOCP notifications by calling [GetQueuedCompletionStatus](https://msdn.microsoft.com/library/windows/desktop/aa364986(v=vs.85).aspx), and then process the notification by calling GetOverlappedResult.

When completing a request that is in error, providers cannot change the status value to a success value in their [IND2Overlapped::GetOverlappedResult](#get-overlapped-result) method. They can, however, change a success value to an error, and they can return ND_PENDING if the request was only partially complete and the next phase initiated successfully. In the latter case, clients are required to again wait for the request completes.

The NetworkDirect status values are defined to map 1 to 1 to their corresponding NTSTATUS values. Providers are encouraged to use the WDK function [NtDeviceIoControlFile](https://docs.microsoft.com/en-us/windows/desktop/api/winternl/nf-winternl-ntdeviceiocontrolfile) to issue IOCTL requests to preserve the status value that is returned by their kernel driver. Note that this function is marked as deprecated in the Microsoft Platform SDK, but it is valid for device drivers to use given their tight coupling with the operating system.