;/*++
;
;Copyright (c) Microsoft Corporation.  All rights reserved.
;
;ndstatus.h - NetworkDirect Status Codes
;
;Status codes with a facility of System map to NTSTATUS codes
;of similar names.
;
;--*/
;
;#ifndef _NDSTATUS_
;#define _NDSTATUS_
;
;#pragma once
;
;

MessageIdTypedef=HRESULT

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
              )

MessageId=0x0000 Facility=System Severity=Success SymbolicName=ND_SUCCESS
Language=English
.

MessageId=0x0102 Facility=System Severity=Success SymbolicName=ND_TIMEOUT
Language=English
.

MessageId=0x0103 Facility=System Severity=Success SymbolicName=ND_PENDING
Language=English
.

MessageId=0x0005 Facility=System Severity=Warning SymbolicName=ND_BUFFER_OVERFLOW
Language=English
.

MessageId=0x0011 Facility=System Severity=Warning SymbolicName=ND_DEVICE_BUSY
Language=English
.

MessageId=0x001A Facility=System Severity=Warning SymbolicName=ND_NO_MORE_ENTRIES
Language=English
.

MessageId=0x0001 Facility=System Severity=Error SymbolicName=ND_UNSUCCESSFUL
Language=English
.

MessageId=0x0005 Facility=System Severity=Error SymbolicName=ND_ACCESS_VIOLATION
Language=English
.

MessageId=0x0008 Facility=System Severity=Error SymbolicName=ND_INVALID_HANDLE
Language=English
.

MessageId=0x0010 Facility=System Severity=Error SymbolicName=ND_INVALID_DEVICE_REQUEST
Language=English
.

MessageId=0x000D Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER
Language=English
.

MessageId=0x0017 Facility=System Severity=Error SymbolicName=ND_NO_MEMORY
Language=English
.

MessageId=0x0030 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_MIX
Language=English
.

MessageId=0x003C Facility=System Severity=Error SymbolicName=ND_DATA_OVERRUN
Language=English
.

MessageId=0x0043 Facility=System Severity=Error SymbolicName=ND_SHARING_VIOLATION
Language=English
.

MessageId=0x009A Facility=System Severity=Error SymbolicName=ND_INSUFFICIENT_RESOURCES
Language=English
.

MessageId=0x00A3 Facility=System Severity=Error SymbolicName=ND_DEVICE_NOT_READY
Language=English
.

MessageId=0x00B5 Facility=System Severity=Error SymbolicName=ND_IO_TIMEOUT
Language=English
.

MessageId=0x00BB Facility=System Severity=Error SymbolicName=ND_NOT_SUPPORTED
Language=English
.

MessageId=0x00E5 Facility=System Severity=Error SymbolicName=ND_INTERNAL_ERROR
Language=English
.

MessageId=0x00EF Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_1
Language=English
.

MessageId=0x00F0 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_2
Language=English
.

MessageId=0x00F1 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_3
Language=English
.

MessageId=0x00F2 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_4
Language=English
.

MessageId=0x00F3 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_5
Language=English
.

MessageId=0x00F4 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_6
Language=English
.

MessageId=0x00F5 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_7
Language=English
.

MessageId=0x00F6 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_8
Language=English
.

MessageId=0x00F7 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_9
Language=English
.

MessageId=0x00F8 Facility=System Severity=Error SymbolicName=ND_INVALID_PARAMETER_10
Language=English
.

MessageId=0x0120 Facility=System Severity=Error SymbolicName=ND_CANCELED
Language=English
.

MessageId=0x013D Facility=System Severity=Error SymbolicName=ND_REMOTE_ERROR
Language=English
.

MessageId=0x0141 Facility=System Severity=Error SymbolicName=ND_INVALID_ADDRESS
Language=English
.

MessageId=0x0184 Facility=System Severity=Error SymbolicName=ND_INVALID_DEVICE_STATE
Language=English
.

MessageId=0x0206 Facility=System Severity=Error SymbolicName=ND_INVALID_BUFFER_SIZE
Language=English
.

MessageId=0x0209 Facility=System Severity=Error SymbolicName=ND_TOO_MANY_ADDRESSES
Language=English
.

MessageId=0x020A Facility=System Severity=Error SymbolicName=ND_ADDRESS_ALREADY_EXISTS
Language=English
.

MessageId=0x0236 Facility=System Severity=Error SymbolicName=ND_CONNECTION_REFUSED
Language=English
.

MessageId=0x023A Facility=System Severity=Error SymbolicName=ND_CONNECTION_INVALID
Language=English
.

MessageId=0x023B Facility=System Severity=Error SymbolicName=ND_CONNECTION_ACTIVE
Language=English
.

MessageId=0x023C Facility=System Severity=Error SymbolicName=ND_NETWORK_UNREACHABLE
Language=English
.

MessageId=0x023D Facility=System Severity=Error SymbolicName=ND_HOST_UNREACHABLE
Language=English
.

MessageId=0x0241 Facility=System Severity=Error SymbolicName=ND_CONNECTION_ABORTED
Language=English
.

MessageId=0x02B6 Facility=System Severity=Error SymbolicName=ND_DEVICE_REMOVED
Language=English
.

;#endif // _NDSTATUS_
