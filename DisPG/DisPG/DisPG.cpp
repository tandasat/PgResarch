//
// This module implements an entry point of the driver and functions that
// call appropriate functions according to the running OS version.
//
#include "stdafx.h"
#include "util.h"
#include "win8.h"
#include "winX.h"


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

struct SymbolSet
{
    const wchar_t* SymbolName;
    void** Variable;
    bool(*IsRequired)();
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C DRIVER_INITIALIZE DriverEntry;

EXTERN_C static
NTSTATUS DispgpInitialize(
    __in PUNICODE_STRING RegistryPath);

EXTERN_C static
NTSTATUS DispgInitWindowsVersion(
    __in const wchar_t* RegistryPath);

EXTERN_C static
NTSTATUS DispgpLoadSymbolAddresses(
    __in const wchar_t* RegistryPath);

EXTERN_C static
NTSTATUS DispgpLoadPointerVaule(
    __in const wchar_t* Path,
    __in const wchar_t* ValueName,
    __out void** Value);

EXTERN_C static
NTSTATUS DispgpDisablePatchGuard();

EXTERN_C static
bool DispgpIsWindows8OrGreater();

EXTERN_C static
bool DispgpIsWindowsXp();

EXTERN_C void AsmNtQuerySystemInformation_Win8_1();
EXTERN_C void AsmNtQuerySystemInformation_Win8_1End();
EXTERN_C void AsmNtQuerySystemInformation_WinVista();
EXTERN_C void AsmNtQuerySystemInformation_WinVistaEnd();
EXTERN_C void AsmNtQuerySystemInformation_WinXp();
EXTERN_C void AsmNtQuerySystemInformation_WinXpEnd();


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

static RTL_OSVERSIONINFOEXW g_WindowsVersion = {};
static ULONG64 g_KernelVersion = 0;

// always
static ULONG_PTR g_ExAcquireResourceSharedLite = 0;
// ifVistaOr7
static POOL_TRACKER_BIG_PAGES** g_PoolBigPageTable = nullptr;
// ifXp
static POOL_TRACKER_BIG_PAGES_XP** g_PoolBigPageTableXp = nullptr;
// ifNot8OrGreater
static SIZE_T* g_PoolBigPageTableSize = nullptr;
static ULONG_PTR* g_MmNonPagedPoolStart = nullptr;
// if8OrGreater
static ULONG_PTR g_KiScbQueueScanWorker = 0;
static ULONG_PTR g_HalPerformEndOfInterrupt = 0;
static ULONG_PTR g_KiCommitThreadWait = 0;
static ULONG_PTR g_KiAttemptFastRemovePriQueue = 0;
static ULONG_PTR g_KeDelayExecutionThread = 0;
static ULONG_PTR g_KeWaitForSingleObject = 0;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Entry point
ALLOC_TEXT(INIT, DriverEntry)
EXTERN_C
NTSTATUS DriverEntry(
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath)
{
    PAGED_CODE();

    DBG_BREAK();

    DBG_PRINT("[%5Iu:%5Iu] Initialize : Starting DisPG.\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()));

    auto status = DispgpInitialize(RegistryPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    // Disable PatchGuard.
    status = DispgpDisablePatchGuard();
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    DBG_PRINT("[%5Iu:%5Iu] Initialize : PatchGuard has been disarmed.\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()));

    DBG_PRINT("[%5Iu:%5Iu] Initialize : Enjoy freedom ;)\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()));
    return status;
}


// Initialize global variables
ALLOC_TEXT(INIT, DispgpInitialize)
EXTERN_C static
NTSTATUS DispgpInitialize(
    __in PUNICODE_STRING RegistryPath)
{
    PAGED_CODE();

    // UNICODE_STRING to wchar_t
    wchar_t path[200];
    auto status = RtlStringCchPrintfW(path, _countof(path), L"%wZ",
        RegistryPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = DispgInitWindowsVersion(path);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = DispgpLoadSymbolAddresses(path);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    return status;
}


// Initialize g_WindowsVersion and g_KernelVersion
ALLOC_TEXT(INIT, DispgInitWindowsVersion)
EXTERN_C static
NTSTATUS DispgInitWindowsVersion(
    __in const wchar_t* RegistryPath)
{
    PAGED_CODE();

    auto status = RtlGetVersion(
        reinterpret_cast<RTL_OSVERSIONINFOW*>(&g_WindowsVersion));
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = DispgpLoadPointerVaule(RegistryPath, L"KernelVersion",
        reinterpret_cast<void**>(&g_KernelVersion));
    if (!NT_SUCCESS(status))
    {
        DBG_PRINT("KernelVersion was not found in the registry.\n");
        return status;
    }
    return status;
}


// Initialize symbols
ALLOC_TEXT(INIT, DispgpLoadSymbolAddresses)
EXTERN_C static
NTSTATUS DispgpLoadSymbolAddresses(
    __in const wchar_t* RegistryPath)
{
    PAGED_CODE();
    auto status = STATUS_UNSUCCESSFUL;

    // Define condition functors
    const auto always = []() { return true; };
    const auto if8OrGreater = []() { return DispgpIsWindows8OrGreater(); };
    const auto ifNot8OrGreater = []() { return !DispgpIsWindows8OrGreater(); };
    const auto ifVistaOr7 = []()
    {
        return (g_WindowsVersion.dwMajorVersion == 6 && g_WindowsVersion.dwMinorVersion == 0)
            || (g_WindowsVersion.dwMajorVersion == 6 && g_WindowsVersion.dwMinorVersion == 1);
    };
    const auto ifXpOrVista = []()
    {
        return (g_WindowsVersion.dwMajorVersion == 5 && g_WindowsVersion.dwMinorVersion == 2)
            || (g_WindowsVersion.dwMajorVersion == 6 && g_WindowsVersion.dwMinorVersion == 0);
    };
    const auto ifXp = []() { return DispgpIsWindowsXp(); };

    // Define a list of required symbols, variables to save the value and
    // conditions to determine if it is needed or not
    const SymbolSet requireSymbols[] =
    {
        { L"ntoskrnl!ExAcquireResourceSharedLite",  reinterpret_cast<void**>(&g_ExAcquireResourceSharedLite),   always, },
        { L"ntoskrnl!PoolBigPageTable",             reinterpret_cast<void**>(&g_PoolBigPageTable),              ifVistaOr7, },
        { L"ntoskrnl!PoolBigPageTable",             reinterpret_cast<void**>(&g_PoolBigPageTableXp),            ifXp, },
        { L"ntoskrnl!PoolBigPageTableSize",         reinterpret_cast<void**>(&g_PoolBigPageTableSize),          ifNot8OrGreater, },
        { L"ntoskrnl!MmNonPagedPoolStart",          reinterpret_cast<void**>(&g_MmNonPagedPoolStart),           ifNot8OrGreater, },
        { L"ntoskrnl!KiScbQueueScanWorker",         reinterpret_cast<void**>(&g_KiScbQueueScanWorker),          if8OrGreater, },
        { L"ntoskrnl!HalPerformEndOfInterrupt",     reinterpret_cast<void**>(&g_HalPerformEndOfInterrupt),      if8OrGreater, },
        { L"ntoskrnl!KiCommitThreadWait",           reinterpret_cast<void**>(&g_KiCommitThreadWait),            if8OrGreater, },
        { L"ntoskrnl!KiAttemptFastRemovePriQueue",  reinterpret_cast<void**>(&g_KiAttemptFastRemovePriQueue),   if8OrGreater, },
        { L"ntoskrnl!KeDelayExecutionThread",       reinterpret_cast<void**>(&g_KeDelayExecutionThread),        if8OrGreater, },
        { L"ntoskrnl!KeWaitForSingleObject",        reinterpret_cast<void**>(&g_KeWaitForSingleObject),         if8OrGreater, },
    };

    // Load each symbol from the registry if required
    for (const auto& request : requireSymbols)
    {
        if (!request.IsRequired())
        {
            continue;
        }
        status = DispgpLoadPointerVaule(RegistryPath,
            request.SymbolName, request.Variable);
        if (!NT_SUCCESS(status))
        {
            DBG_PRINT("Symbol %ws was not found.\n", request.SymbolName);
            return status;
        }
    }

    // Check if the symbol address is correct by comparing with the real value
    UNICODE_STRING procName =
        RTL_CONSTANT_STRING(L"ExAcquireResourceSharedLite");
    const auto realAddress =
        reinterpret_cast<ULONG64>(MmGetSystemRoutineAddress(&procName));
    if (realAddress != g_ExAcquireResourceSharedLite)
    {
        DBG_PRINT("Symbol information is not fresh.\n");
        return STATUS_DATA_NOT_ACCEPTED;
    }
    return status;
}


// Loads pointer size value from the registry
ALLOC_TEXT(INIT, DispgpLoadPointerVaule)
EXTERN_C static
NTSTATUS DispgpLoadPointerVaule(
    __in const wchar_t* Path,
    __in const wchar_t* ValueName,
    __out void** Value)
{
    PAGED_CODE();
    UNICODE_STRING path = {};
    RtlInitUnicodeString(&path, Path);
    OBJECT_ATTRIBUTES oa = RTL_INIT_OBJECT_ATTRIBUTES(
        &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE);

    // Open the registry
    HANDLE regHandle = nullptr;
    NTSTATUS status = ZwOpenKey(&regHandle, KEY_READ, &oa);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    UNICODE_STRING valueName = {};
    RtlInitUnicodeString(&valueName, ValueName);

    UCHAR buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION)+sizeof(void*)] = {};
    auto* data = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(buffer);

    // Read value
    ULONG resultLength = 0;
    status = ZwQueryValueKey(regHandle, &valueName, KeyValuePartialInformation,
        buffer, sizeof(buffer), &resultLength);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    // Error if it is not a binary type or not pointer size
    if (data->Type != REG_BINARY || data->DataLength != sizeof(void*))
    {
        status = STATUS_DATA_NOT_ACCEPTED;
        goto Exit;
    }
    *Value = *reinterpret_cast<void**>(data->Data);

Exit:;
    ZwClose(regHandle);
    return status;
}


// Disables PatchGuard
ALLOC_TEXT(INIT, DispgpDisablePatchGuard)
EXTERN_C static
NTSTATUS DispgpDisablePatchGuard()
{
    PAGED_CODE();
    auto status = STATUS_UNSUCCESSFUL;

    if (DispgpIsWindows8OrGreater())
    {
        // For Win 8.1
        Win8Symbols symbols = {};
        symbols.KernelVersion = g_KernelVersion;
        symbols.HalPerformEndOfInterrupt = g_HalPerformEndOfInterrupt;
        symbols.KeDelayExecutionThread = g_KeDelayExecutionThread;
        symbols.KeWaitForSingleObject = g_KeWaitForSingleObject;
        symbols.KiAttemptFastRemovePriQueue = g_KiAttemptFastRemovePriQueue;
        symbols.KiCommitThreadWait = g_KiCommitThreadWait;
        symbols.KiScbQueueScanWorker = g_KiScbQueueScanWorker;
        status = Win8DisablePatchGuard(symbols);
    }
    else
    {
        // For Win 7, Vista and XP
        WinXSymbols symbols = {};
        symbols.ExAcquireResourceSharedLite = g_ExAcquireResourceSharedLite;
        symbols.MmNonPagedPoolStart = g_MmNonPagedPoolStart;
        symbols.PoolBigPageTable = g_PoolBigPageTable;
        symbols.PoolBigPageTableSize = g_PoolBigPageTableSize;
        symbols.PoolBigPageTableXp = g_PoolBigPageTableXp;
        status = WinXDisablePatchGuard(symbols);
    }
    return status;
}


// Return true if the platform is Win 8 or later
ALLOC_TEXT(INIT, DispgpIsWindows8OrGreater)
EXTERN_C static
bool DispgpIsWindows8OrGreater()
{
    PAGED_CODE();
    return (g_WindowsVersion.dwMajorVersion > 6
        || (g_WindowsVersion.dwMajorVersion == 6 && g_WindowsVersion.dwMinorVersion >= 2));
}


// Return true if the platform is Win XP
ALLOC_TEXT(INIT, DispgpIsWindowsXp)
EXTERN_C static
bool DispgpIsWindowsXp()
{
    PAGED_CODE();
    return (g_WindowsVersion.dwMajorVersion == 5 && g_WindowsVersion.dwMinorVersion == 2);
}

