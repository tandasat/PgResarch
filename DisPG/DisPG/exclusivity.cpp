//
// This module implements a lock function halts all processors but one, and
// corresponding a release function. The lock function is also known as
// GainExclusivity.
//
#include "stdafx.h"
#include "exclusivity.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//

// Tag used for memory allocation APIs
static const LONG EXCLUSIVITY_TAG = 'excl';


////////////////////////////////////////////////////////////////////////////////
//
// types
//

struct ExclusivityContext
{
    KIRQL OldIrql;
    KDPC Dpcs[1];   // This field is used as a variadic array
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C static
void ExclpRaiseIrqlAndWait(
    __in PKDPC Dpc,
    __in_opt PVOID DeferredContext,
    __in_opt PVOID SystemArgument1,
    __in_opt PVOID SystemArgument2);


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

// 1 when all processors should be released; otherwise 0
static LONG64 volatile g_ReleaseAllProcessors = 0;

// How many processors were locked
static LONG64 volatile g_NumberOfLockedProcessors = 0;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Locks all other processors and returns exclusivity pointer. This function
// should never be called before the last exclusivity is released.
_IRQL_raises_(DISPATCH_LEVEL)
EXTERN_C
void* ExclGainExclusivity()
{
    ASSERT(g_NumberOfLockedProcessors == 0);
    _InterlockedAnd64(&g_ReleaseAllProcessors, 0);

    const auto numberOfProcessors = KeNumberProcessors;

    // Allocates DPCs for all processors
    auto context = reinterpret_cast<ExclusivityContext*>(
        ExAllocatePoolWithTag(NonPagedPool,
            sizeof(KIRQL)+(numberOfProcessors * sizeof(KDPC)), EXCLUSIVITY_TAG));
    if (!context)
    {
        return nullptr;
    }

    // Execute a lock DPC for all processors but this
    context->OldIrql = KeRaiseIrqlToDpcLevel();
    const auto currentCpu = KeGetCurrentProcessorNumber();
    for (CCHAR i = 0; i < numberOfProcessors; i++)
    {
        if (static_cast<UCHAR>(i) == currentCpu)
        {
            continue;
        }

        // Queue a lock DPC
        KeInitializeDpc(&context->Dpcs[i], ExclpRaiseIrqlAndWait, nullptr);
        KeSetTargetProcessorDpc(&context->Dpcs[i], i);
        KeInsertQueueDpc(&context->Dpcs[i], nullptr, nullptr);
    }

    // Wait until all other processors are halted
    const auto needToBeLocked = numberOfProcessors - 1;
    while (_InterlockedCompareExchange64(&g_NumberOfLockedProcessors,
        needToBeLocked, needToBeLocked) != needToBeLocked)
    {
        KeStallExecutionProcessor(10);
    }
    return context;
}


EXTERN_C
void ExclReleaseExclusivity(
    __in_opt void* Exclusivity)
{
    if (!Exclusivity)
    {
        return;
    }

    // Tell other processors they can be unlocked by changing the value
    _InterlockedIncrement64(&g_ReleaseAllProcessors);

    // Wait until all other processors are unlocked
    while (_InterlockedCompareExchange64(&g_NumberOfLockedProcessors, 0, 0))
    {
        KeStallExecutionProcessor(10);
    }

    auto context = static_cast<ExclusivityContext*>(Exclusivity);
    KeLowerIrql(context->OldIrql);
    ExFreePoolWithTag(context, EXCLUSIVITY_TAG);
}


// Locks this processor until g_ReleaseAllProcessors becomes 1
EXTERN_C static
void ExclpRaiseIrqlAndWait(
    __in PKDPC Dpc,
    __in_opt PVOID DeferredContext,
    __in_opt PVOID SystemArgument1,
    __in_opt PVOID SystemArgument2)
{
    // Increase the number of locked processors
    _InterlockedIncrement64(&g_NumberOfLockedProcessors);

    // Wait until g_ReleaseAllProcessors becomes 1
    while (!_InterlockedCompareExchange64(&g_ReleaseAllProcessors, 1, 1))
    {
        KeStallExecutionProcessor(10);
    }

    // Decrease the number of locked processors
    _InterlockedDecrement64(&g_NumberOfLockedProcessors);
}

