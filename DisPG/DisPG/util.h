#pragma once
extern "C" {
#include <ntddk.h>
}


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

//
// CR0
//  2.5 CONTROL REGISTERS
//
typedef union
{
    ULONG_PTR All;
    struct
    {
        unsigned PE : 1;   // Protected Mode Enabled
        unsigned MP : 1;   // Monitor Coprocessor FLAG
        unsigned EM : 1;   // Emulate FLAG
        unsigned TS : 1;   // Task Switched FLAG
        unsigned ET : 1;   // Extension Type FLAG
        unsigned NE : 1;   // Numeric Error
        unsigned Reserved1 : 10;
        unsigned WP : 1;   // Write Protect
        unsigned Reserved2 : 1;
        unsigned AM : 1;   // Alignment Mask
        unsigned Reserved3 : 10;
        unsigned NW : 1;   // Not Write-Through
        unsigned CD : 1;   // Cache Disable
        unsigned PG : 1;   // Paging Enabled
    } Field;
} CR0_REG;
C_ASSERT(sizeof(CR0_REG) == sizeof(void*));


static const auto HARDWARE_PTE_WORKING_SET_BITS = 11;
typedef struct _HARDWARE_PTE
{
    ULONG64 Valid : 1;
    ULONG64 Write : 1;                // UP version
    ULONG64 Owner : 1;
    ULONG64 WriteThrough : 1;
    ULONG64 CacheDisable : 1;
    ULONG64 Accessed : 1;
    ULONG64 Dirty : 1;
    ULONG64 LargePage : 1;
    ULONG64 Global : 1;
    ULONG64 CopyOnWrite : 1;          // software field
    ULONG64 Prototype : 1;            // software field
    ULONG64 reserved0 : 1;            // software field
    ULONG64 PageFrameNumber : 28;
    ULONG64 reserved1 : 24 - (HARDWARE_PTE_WORKING_SET_BITS + 1);
    ULONG64 SoftwareWsIndex : HARDWARE_PTE_WORKING_SET_BITS;
    ULONG64 NoExecute : 1;
} HARDWARE_PTE, *PHARDWARE_PTE;
C_ASSERT(sizeof(HARDWARE_PTE) == sizeof(void*));


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C
void Ia32DisableWriteProtect();

EXTERN_C
void Ia32EnableWriteProtect();

EXTERN_C
PHARDWARE_PTE MiAddressToPxe(
    __in void* Address);

EXTERN_C
PHARDWARE_PTE MiAddressToPpe(
    __in void* Address);

EXTERN_C
PHARDWARE_PTE MiAddressToPde(
    __in void* Address);

EXTERN_C
PHARDWARE_PTE MiAddressToPte(
    __in void* Address);

EXTERN_C
ULONG KeQueryActiveProcessorCountCompatible(
    __out_opt PKAFFINITY ActiveProcessors);

EXTERN_C
NTSTATUS ForEachProcessors(
    __in NTSTATUS(*CallbackRoutine)(void*),
    __in_opt void* Context);

EXTERN_C
void* MemMem(
    __in const void* SearchBase,
    __in SIZE_T SearchSize,
    __in const void* Pattern,
    __in SIZE_T PatternSize);

EXTERN_C
void FixupAsmCode(
    __in ULONG_PTR ReturnAddress,
    __in ULONG_PTR AsmHandler,
    __in ULONG_PTR AsmHandlerEnd);

EXTERN_C
void InstallJump(
    __in ULONG_PTR PatchAddress,
    __in ULONG_PTR JumpDestination);

EXTERN_C
void PatchReal(
    __in void* PatchAddr,
    __in const void* PatchCode,
    __in SIZE_T PatchBytes);

EXTERN_C
bool IsCanonicalAddress(
    __in ULONG_PTR Address);


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

