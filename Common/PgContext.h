//
// This module defines PatchGuard context structures for each platform and is
// shared with multiple projects.
//
#pragma once
#include <basetsd.h>

// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers


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

struct PgContextBase
{
    UCHAR CmpAppendDllSection[192];
    ULONG32 unknown;
    ULONG32 ContextSizeInQWord;
    ULONG64 ExAcquireResourceSharedLite;
    // ...
};
C_ASSERT(sizeof(PgContextBase) == 0xd0);


struct PgContext_7
{
    UCHAR CmpAppendDllSection[192];
    ULONG32 unknown;
    ULONG32 ContextSizeInQWord;
    ULONG64 ExAcquireResourceSharedLite;
    ULONG64 ExAllocatePoolWithTag;
    ULONG64 ExFreePool;
    ULONG64 ExMapHandleToPointer;
    ULONG64 ExQueueWorkItem;
    ULONG64 ExReleaseResourceLite;
    ULONG64 ExUnlockHandleTableEntry;
    ULONG64 ExfAcquirePushLockExclusive;
    ULONG64 ExfReleasePushLockExclusive;
    ULONG64 KeAcquireInStackQueuedSpinLockAtDpcLevel;
    ULONG64 ExAcquireSpinLockShared;
    ULONG64 KeBugCheckEx;
    ULONG64 KeDelayExecutionThread;
    ULONG64 KeEnterCriticalRegionThread;
    ULONG64 KeLeaveCriticalRegion;
    ULONG64 KeEnterGuardedRegion;
    ULONG64 KeLeaveGuardedRegion;
    ULONG64 KeReleaseInStackQueuedSpinLockFromDpcLevel;
    ULONG64 ExReleaseSpinLockShared;
    ULONG64 KeRevertToUserAffinityThread;
    ULONG64 KeProcessorGroupAffinity;
    ULONG64 KeSetSystemGroupAffinityThread;
    ULONG64 KeSetTimer;
    ULONG64 LdrResFindResource;
    ULONG64 MmDbgCopyMemory;
    ULONG64 ObfDereferenceObject;
    ULONG64 ObReferenceObjectByName;
    ULONG64 RtlAssert;
    ULONG64 RtlImageDirectoryEntryToData;
    ULONG64 RtlImageNtHeader;
    ULONG64 RtlLookupFunctionTable;
    ULONG64 RtlSectionTableFromVirtualAddress;
    ULONG64 DbgPrint;
    ULONG64 DbgPrintEx;
    ULONG64 KiProcessListHead;
    ULONG64 KiProcessListLock;
    ULONG64 unknown1;
    ULONG64 PsActiveProcessHead;
    ULONG64 PsInvertedFunctionTable;
    ULONG64 PsLoadedModuleList;
    ULONG64 PsLoadedModuleResource;
    ULONG64 PsLoadedModuleSpinLock;
    ULONG64 PspActiveProcessLock;
    ULONG64 PspCidTable;
    ULONG64 SwapContext;
    ULONG64 EnlightenedSwapContext;
    ULONG64 unknown2;
    ULONG64 unknown3;
    ULONG64 unknown4;
    ULONG64 workerRoutine;
    ULONG64 workerQueueContext;
    ULONG64 unknown5;
    ULONG64 Prcb;
    ULONG64 PageBase;
    ULONG64 DcpRoutineToBeScheduled;
    ULONG32 unknown6;
    ULONG32 unknown7;
    ULONG32 offsetToPg_SelfValidation;
    ULONG32 offsetToRtlLookupFunctionEntryEx;
    ULONG32 offsetToFsRtlUninitializeSmallMcb;
    ULONG32 unknown8;
    ULONG64 field_298;
    ULONG64 field_2A0;
    ULONG64 field_2A8;
    ULONG64 field_2B0;
    ULONG64 field_2B8;
    ULONG64 field_2C0;
    ULONG64 field_2C8;
    ULONG64 field_2D0;
    ULONG64 field_2D8;
    ULONG64 field_2E0;
    ULONG64 field_2E8;
    ULONG64 field_2F0;
    ULONG64 field_2F8;
    ULONG64 field_300;
    ULONG64 isErroFound;
    ULONG64 bugChkParam1;
    ULONG64 bugChkParam2;
    ULONG64 bugChkParam4Type;
    ULONG64 bugChkParam3;
    ULONG64 field_330;
};
C_ASSERT(sizeof(PgContext_7) == 0x338);


struct PgContext_8_1
{
    UCHAR CmpAppendDllSection[192];
    ULONG32 unknown;
    ULONG32 ContextSizeInQWord;
    ULONG64 ExAcquireResourceSharedLite;
    ULONG64 ExAcquireResourceExclusiveLite;
    ULONG64 ExAllocatePoolWithTag;
    ULONG64 ExFreePool;
    ULONG64 ExMapHandleToPoULONG32er;
    ULONG64 ExQueueWorkItem;
    ULONG64 ExReleaseResourceLite;
    ULONG64 ExUnlockHandleTableEntry;
    ULONG64 ExfAcquirePushLockExclusive;
    ULONG64 ExfReleasePushLockExclusive;
    ULONG64 ExfAcquirePushLockShared;
    ULONG64 ExfReleasePushLockShared;
    ULONG64 KeAcquireInStackQueuedSpinLockAtDpcLevel;
    ULONG64 ExAcquireSpinLockSharedAtDpcLevel;
    ULONG64 KeBugCheckEx;
    ULONG64 KeDelayExecutionThread;
    ULONG64 KeEnterCriticalRegionThread;
    ULONG64 KeLeaveCriticalRegion;
    ULONG64 KeEnterGuardedRegion;
    ULONG64 KeLeaveGuardedRegion;
    ULONG64 KeReleaseInStackQueuedSpinLockFromDpcLevel;
    ULONG64 ExReleaseSpinLockSharedFromDpcLevel;
    ULONG64 KeRevertToUserAffinityThread;
    ULONG64 KeProcessorGroupAffinity;
    ULONG64 KeSetSystemGroupAffinityThread;
    ULONG64 KeSetCoalescableTimer;
    ULONG64 ObfDereferenceObject;
    ULONG64 ObReferenceObjectByName;
    ULONG64 RtlImageDirectoryEntryToData;
    ULONG64 RtlImageNtHeader;
    ULONG64 RtlLookupFunctionTable;
    ULONG64 RtlPcToFileHeader;
    ULONG64 RtlSectionTableFromVirtualAddress;
    ULONG64 DbgPrULONG32;
    ULONG64 MmAllocateIndependentPages;
    ULONG64 MmFreeIndependentPages;
    ULONG64 MmSetPageProtection;
    ULONG64 unknown1;
    ULONG64 unknown2;
    ULONG64 unknown3;
    ULONG64 unknown4;
    ULONG64 RtlLookupFunctionEntry;
    ULONG64 KeAcquireSpinLockRaiseToDpc;
    ULONG64 KeReleaseSpinLock;
    ULONG64 MmGetSessionById;
    ULONG64 MmGetNextSession;
    ULONG64 MmQuitNextSession;
    ULONG64 MmAttachSession;
    ULONG64 MmDetachSession;
    ULONG64 MmGetSessionIdEx;
    ULONG64 MmIsSessionAddress;
    ULONG64 KeInsertQueueApc;
    ULONG64 KeWaitForSingleObject;
    ULONG64 PsCreateSystemThread;
    ULONG64 ExReferenceCallBackBlock;
    ULONG64 ExGetCallBackBlockRoutine;
    ULONG64 ExDereferenceCallBackBlock;
    ULONG64 KiScbQueueScanWorker;
    ULONG64 PspEnumerateCallback;
    ULONG64 CmpEnumerateCallback;
    ULONG64 DbgEnumerateCallback;
    ULONG64 ExpEnumerateCallback;
    ULONG64 ExpGetNextCallback;
    ULONG64 PopPoCoalescinCallback_;
    ULONG64 KiSchedulerApcTerminate;
    ULONG64 KiSchedulerApc;
    ULONG64 PopPoCoalescinCallback;
    ULONG64 Pg_SelfEncryptWaitAndDecrypt;
    ULONG64 KiWaitAlways;
    ULONG64 KiEntropyTimingRoutine;
    ULONG64 KiProcessListHead;
    ULONG64 KiProcessListLock;
    ULONG64 ObpTypeObjectType;
    ULONG64 IoDriverObjectType;
    ULONG64 PsActiveProcessHead;
    ULONG64 PsInvertedFunctionTable;
    ULONG64 PsLoadedModuleList;
    ULONG64 PsLoadedModuleResource;
    ULONG64 PsLoadedModuleSpinLock;
    ULONG64 PspActiveProcessLock;
    ULONG64 PspCidTable;
    ULONG64 ExpUuidLock;
    ULONG64 AlpcpPortListLock;
    ULONG64 KeServiceDescriptorTable;
    ULONG64 KeServiceDescriptorTableShadow;
    ULONG64 VfThunksExtended;
    ULONG64 PsWin32CallBack;
    ULONG64 TriageImagePageSize_0x28;
    ULONG64 KiTableInformation;
    ULONG64 HandleTableListHead;
    ULONG64 HandleTableListLock;
    ULONG64 ObpKernelHandleTable;
    ULONG64 HyperSpace;
    ULONG64 KiWaitNever;
    ULONG64 KxUnexpectedULONG32errupt0;
    ULONG64 pgContextEndFieldToBeCached;
    ULONG64 unknown13;
    ULONG64 workerQueueItem;
    ULONG64 ExNode0_0x198;
    ULONG64 workerRoutine;
    ULONG64 workerQueueContext;
    ULONG64 unknown15;
    ULONG64 Prcb;
    ULONG64 PageBase;
    ULONG64 secondParamOfEndOfUninitialize;
    ULONG64 dcpRoutineToBeScheduled;
    ULONG32 numberOfChunksToBeValidated;
    ULONG32 field_41C;
    ULONG32 offsetToPg_SelfValidationInBytes;
    ULONG32 field_424;
    ULONG32 offsetToFsUninitializeSmallMcbInBytes;
    ULONG32 field_42C;
    ULONG64 spinLock;
    ULONG32 offsetToValidationInfoInBytes;
    ULONG32 field_43C;
    ULONG32 unknown22;
    ULONG32 hashShift;
    ULONG64 hashSeed;
    ULONG64 unknown24;
    ULONG32 comparedSizeForHash;
    ULONG32 field_45C;
    ULONG32 unknown26;
    ULONG32 field_464;
    ULONG64 schedulerType;
    ULONG64 unknown28;
    ULONG64 unknown29;
    ULONG64 unknown30;
    ULONG64 unknown31;
    ULONG64 unknown32;
    ULONG64 unknown33;
    ULONG64 unknown34;
    ULONG64 unknown35;
    ULONG64 unknown36;
    ULONG64 _guard_check_icall_fptr;
    ULONG64 hal_guard_check_icall_fptr;
    ULONG64 _guard_check_icall_fptr_108;
    ULONG64 isErroFound;
    ULONG64 bugChkParam1;
    ULONG64 bugChkParam2;
    ULONG64 bugChkParam4Type;
    ULONG64 bugChkParam3;
    ULONG32 unknown42;
    ULONG32 shouldMmAllocateIndependentPagesBeUsed;
    ULONG32 lockType;
    ULONG32 field_504;
    ULONG64 pagevrf;
    ULONG64 pagespec;
    ULONG64 init;
    ULONG64 pagekd;
    ULONG64 unknown44;
    ULONG64 TriageImagePageSize_0x48;
    ULONG64 unknown45;
    ULONG32 checkWin32kIfNotNegativeOne;
    ULONG32 field_544;
    ULONG64 win32kBase;
    ULONG64 sessionPoULONG32er;
    ULONG32 onTheFlyEnDecryptionFlag;
    ULONG32 dispatcherHeaderShouldBeMaked;
    ULONG32 usedToDetermineErrorAsFlag;
    ULONG32 field_564;
    ULONG64 threadForAPC;
    ULONG64 unknown51;
    ULONG64 unknown52;
    ULONG64 unknown53;
    ULONG64 unknown54;
    ULONG64 unknown55;
    ULONG64 unknown56;
    ULONG64 unknown57;
    ULONG64 unknown58;
    ULONG64 apc;
    ULONG64 KiDispatchCallout;
    ULONG64 EmpCheckErrataList_PopPoCoalescinCallback;
    ULONG64 shouldKeWaitForSingleObjectBeUsed;
    ULONG64 shouldKiScbQueueWorkerBeUsed;
    ULONG64 unknown62;
    ULONG64 unknown63;
    ULONG64 PgOriginalHash;
    ULONG64 unknown65;
    ULONG64 shouldPg_SelfEncryptWaitAndDecryptBeUsed;
    ULONG32 offsetToPteRestoreInfoArrayInNumbers;
    ULONG32 numberOfPteToBeRestored;
    ULONG64 hal_HaliHaltSystem;
    ULONG64 unknown68;
    ULONG64 KeBugCheckEx_;
    ULONG64 unknown69;
    ULONG64 KeBugCheck2;
    ULONG64 unknown70;
    ULONG64 KiBugCheckDebugBreak;
    ULONG64 unknown71;
    ULONG64 KiDebugTrapOrFault;
    ULONG64 unknown72;
    ULONG64 DbgBreakPoULONG32WithStatus;
    ULONG64 unknown73;
    ULONG64 RtlCaptureContext;
    ULONG64 unknown74;
    ULONG64 KeQueryCurrentStackInformation;
    ULONG64 unknown75;
    ULONG64 KeQueryCurrentStackInformation_chunk;
    ULONG64 unknown76;
    ULONG64 KiSaveProcessorControlState;
    ULONG64 unknown77;
    ULONG64 HalPrivateDispatchTable_0x48;
    ULONG64 unknown78;
    ULONG64 unknown79;
    ULONG64 unknown80;
    ULONG64 unknown81;
    ULONG64 unknown82;
    ULONG64 unknown83;
    ULONG64 unknown84;
    ULONG64 unknown85;
    ULONG64 unknown86;
    ULONG64 unknown87;
    ULONG64 unknown88;
    ULONG64 unknown89;
    ULONG64 unknown90;
    ULONG64 unknown91;
    ULONG64 unknown92;
    ULONG64 unknown93;
    ULONG64 unknown94;
    ULONG64 unknown95;
    ULONG64 unknown96;
    ULONG64 unknown97;
    ULONG64 unknown98;
    ULONG64 unknown99;
    ULONG64 unknown100;
    ULONG64 unknown101;
    ULONG64 unknown102;
    ULONG64 unknown103;
    ULONG64 unknown104;
    ULONG64 unknown105;
    ULONG64 unknown106;
    ULONG64 unknown107;
    ULONG64 unknown108;
};
C_ASSERT(sizeof(PgContext_8_1) == 0x7a8);


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

