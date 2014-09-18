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


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//


extern "C" {

// Interlocked
__int64 _InterlockedIncrement64(__int64 volatile* lpAddend);
__int64 _InterlockedDecrement64(__int64 volatile* lpAddend);
__int64 _InterlockedAnd64(__int64 volatile* value, __int64 mask);
__int64 _InterlockedCompareExchange64(__int64 volatile * Destination, __int64 Exchange, __int64 Comparand);
#pragma intrinsic(_InterlockedIncrement64)
#pragma intrinsic(_InterlockedDecrement64)
#pragma intrinsic(_InterlockedAnd64)
#pragma intrinsic(_InterlockedCompareExchange64)

// Interruption
void _disable(void);
void _enable(void);
#pragma intrinsic(_disable)
#pragma intrinsic(_enable)

// CR
ULONG_PTR __readcr0(void);
ULONG_PTR __readcr3(void);
ULONG_PTR __readcr4(void);
void __writecr0(ULONG_PTR Data);
void __writecr3(ULONG_PTR Data);
void __writecr4(ULONG_PTR Data);
#pragma intrinsic(__readcr0)
#pragma intrinsic(__readcr3)
#pragma intrinsic(__readcr4)
#pragma intrinsic(__writecr0)
#pragma intrinsic(__writecr3)
#pragma intrinsic(__writecr4)

// MSR
unsigned __int64 __readmsr(int EcxRegister);
void __writemsr(unsigned long Register, unsigned __int64 Value);
#pragma intrinsic(__readmsr)
#pragma intrinsic(__writemsr)

// Misc
void __nop();
#pragma intrinsic(__nop)

void* _AddressOfReturnAddress();
#pragma intrinsic(_AddressOfReturnAddress)

void __invlpg(void* Address);
#pragma intrinsic(__invlpg)

}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

