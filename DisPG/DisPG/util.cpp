//
// This module implements auxiliary functions. These functions do not have
// prefixes on their names.
//
#include "stdafx.h"
#include "util.h"
#include "intrinsics.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//

// Tag used for memory allocation APIs
static const LONG UTIL_TAG = 'util';

// Change it to 0xCC when you want to install break points for patched code
static const UCHAR UTIL_HOOK_ENTRY_CODE = 0x90;


////////////////////////////////////////////////////////////////////////////////
//
// types
//


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

// Disable the write protection
EXTERN_C
void Ia32DisableWriteProtect()
{
    CR0_REG Cr0 = {};
    Cr0.All = __readcr0();
    Cr0.Field.WP = FALSE;
    __writecr0(Cr0.All);
}


// Enable the write protection
EXTERN_C
void Ia32EnableWriteProtect()
{
    CR0_REG Cr0 = {};
    Cr0.All = __readcr0();
    Cr0.Field.WP = TRUE;
    __writecr0(Cr0.All);
}


// Return an address of PXE
EXTERN_C
PHARDWARE_PTE MiAddressToPxe(
    __in void* Address)
{
    ULONG64 Offset = reinterpret_cast<ULONG64>(Address) >> (PXI_SHIFT - 3);
    Offset &= (PXI_MASK << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PXE_BASE + Offset);
}


// Return an address of PPE
EXTERN_C
PHARDWARE_PTE MiAddressToPpe(
    __in void* Address)
{
    ULONG64 Offset = reinterpret_cast<ULONG64>(Address) >> (PPI_SHIFT - 3);
    Offset &= (0x3FFFF << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PPE_BASE + Offset);
}


// Return an address of PDE
EXTERN_C
PHARDWARE_PTE MiAddressToPde(
    __in void* Address)
{
    ULONG64 Offset = reinterpret_cast<ULONG64>(Address) >> (PDI_SHIFT - 3);
    Offset &= (0x7FFFFFF << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PDE_BASE + Offset);
}


// Return an address of PTE
EXTERN_C
PHARDWARE_PTE MiAddressToPte(
    __in void* Address)
{
    ULONG64 Offset = reinterpret_cast<ULONG64>(Address) >> (PTI_SHIFT - 3);
    Offset &= (0xFFFFFFFFFULL << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PTE_BASE + Offset);
}


// Return a number of processors
EXTERN_C
ULONG KeQueryActiveProcessorCountCompatible(
    __out_opt PKAFFINITY ActiveProcessors)
{
#if (NTDDI_VERSION >= NTDDI_VISTA)
    return KeQueryActiveProcessorCount(ActiveProcessors);
#else
    ULONG numberOfProcessors = 0;
    KAFFINITY affinity = KeQueryActiveProcessors();

    if (ActiveProcessors)
    {
        *ActiveProcessors = affinity;
    }

    for (; affinity; affinity >>= 1)
    {
        if (affinity & 1)
        {
            numberOfProcessors++;
        }
    }
    return numberOfProcessors;
#endif
}


// Execute a given callback routine on all processors in DPC_LEVEL. Returns
// STATUS_SUCCESS when all callback returned STATUS_SUCCESS as well. When
// one of callbacks returns anything but STATUS_SUCCESS, this function stops
// to call remaining callbacks and returns the value.
EXTERN_C
NTSTATUS ForEachProcessors(
    __in NTSTATUS(*CallbackRoutine)(void*),
    __in_opt void* Context)
{
    const auto numberOfProcessors =
        KeQueryActiveProcessorCountCompatible(nullptr);
    for (ULONG processorNumber = 0; processorNumber < numberOfProcessors;
        processorNumber++)
    {
        // Switch the current processor
        KeSetSystemAffinityThread(static_cast<KAFFINITY>(1 << processorNumber));
        const auto affinity = std::experimental::scope_guard(
            KeRevertToUserAffinityThread);

        const auto oldIrql = std::experimental::unique_resource(
            KeRaiseIrqlToDpcLevel(), KeLowerIrql);

        // Execute callback
        const auto status = CallbackRoutine(Context);
        if (!NT_SUCCESS(status))
        {
            return status;
        }
    }
    return STATUS_SUCCESS;
}


// Random memory version strstr()
EXTERN_C
void* MemMem(
    __in const void* SearchBase,
    __in SIZE_T SearchSize,
    __in const void* Pattern,
    __in SIZE_T PatternSize)
{
    ASSERT(SearchBase);
    ASSERT(SearchSize);
    ASSERT(Pattern);
    ASSERT(PatternSize);

    if (PatternSize > SearchSize)
    {
        return nullptr;
    }
    auto searchBase = static_cast<const char*>(SearchBase);
    for (size_t i = 0; i <= SearchSize - PatternSize; i++)
    {
        if (!memcmp(Pattern, &searchBase[i], PatternSize))
        {
            return const_cast<char*>(&searchBase[i]);
        }
    }
    return nullptr;
}


// Replaces placeholder (0xffffffffffffffff) in AsmHandler with a given
// ReturnAddress. AsmHandler does not has to be writable. Race condition between
// multiple processors should be taken care of by a programmer it exists; this
// function does not care about it.
EXTERN_C
void FixupAsmCode(
    __in ULONG_PTR ReturnAddress,
    __in ULONG_PTR AsmHandler,
    __in ULONG_PTR AsmHandlerEnd)
{
    ASSERT(AsmHandlerEnd > AsmHandler);
    SIZE_T asmHandlerSize = AsmHandlerEnd - AsmHandler;

    ULONG_PTR pattern = 0xffffffffffffffff;
    auto returnAddr = MemMem(reinterpret_cast<void*>(AsmHandler),
        asmHandlerSize, &pattern, sizeof(pattern));
    ASSERT(returnAddr);
    PatchReal(returnAddr, &ReturnAddress, sizeof(ReturnAddress));
}


// Install a hook on PatchAddress by overwriting original code with JMP code
// that transfer execution to JumpDestination without modifying any registers.
// PatchAddress does not has to be writable. Race condition between multiple
// processors should be taken care of by a programmer it exists; this function
// does not care about it.
EXTERN_C
void InstallJump(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR JumpDestination)
{
    // Code used to overwrite PatchAddress
    UCHAR patchCode[] =
    {
        UTIL_HOOK_ENTRY_CODE,                                       // nop or int 3
        0x50,                                                       // push rax
        0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // mov rax, 0ffffffffffffffffh
        0x48, 0x87, 0x04, 0x24,                                     // xchg rax, [rsp]
        0xC3,                                                       // ret
    };
    C_ASSERT(sizeof(patchCode) == 17);

    // Replace placeholder (0xffffffffffffffff) located at offset 4 of patchCode
    // with JumpDestination
    *reinterpret_cast<ULONG_PTR*>(patchCode + 4) = JumpDestination;

    // And install patch
    PatchReal(reinterpret_cast<void*>(PatchAddress), patchCode,
        sizeof(patchCode));
}


// Overwrites PatchAddr with PatchCode with disabling write protection and
// raising IRQL in order to avoid race condition on this processor.
EXTERN_C
void PatchReal(
    __in void* PatchAddr,
    __in const void* PatchCode,
    __in SIZE_T PatchBytes)
{
    auto oldIrql = std::experimental::unique_resource(
        KeRaiseIrqlToDpcLevel(), KeLowerIrql);
    Ia32DisableWriteProtect();
    auto protection = std::experimental::scope_guard(Ia32EnableWriteProtect);
    memcpy(PatchAddr, PatchCode, PatchBytes);
    __writecr3(__readcr3());
}


// Returns true if the address is canonical address
EXTERN_C
bool IsCanonicalAddress(
    __in ULONG_PTR Address)
{
    return (
        (Address & 0xFFFF000000000000) == 0xFFFF000000000000 ||
        (Address & 0xFFFF000000000000) == 0);
}

