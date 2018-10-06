// Copyright (c) 2015, tandasat. All rights reserved.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

//
// This module implements a lock function halts all processors but the current
// one, and corresponding a release function. The lock function is also known
// as GainExclusivity.
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
static const LONG EXCLP_POOL_TAG = 'excl';


////////////////////////////////////////////////////////////////////////////////
//
// types
//

struct ExclusivityContext
{
    union
    {
        KIRQL OldIrql;
        void *Reserved;
    };
    KDPC Dpcs[1];  // This field is used as a variadic array
};
static_assert(sizeof(ExclusivityContext) == sizeof(void *) + sizeof(KDPC),
    "Size check");


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C static KDEFERRED_ROUTINE ExclpRaiseIrqlAndWaitDpc;


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

// 1 when all processors should be released; otherwise 0.
static volatile LONG g_ExclpReleaseAllProcessors = 0;

// How many processors were locked.
static volatile LONG g_ExclpNumberOfLockedProcessors = 0;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Locks all other processors and returns exclusivity pointer. This function
// should never be called before the last exclusivity is released.
_Use_decl_annotations_ EXTERN_C
void *ExclGainExclusivity()
{
    NT_ASSERT(InterlockedAdd(&g_ExclpNumberOfLockedProcessors, 0) == 0);
    _InterlockedAnd(&g_ExclpReleaseAllProcessors, 0);

    const auto numberOfProcessors = KeQueryActiveProcessorCount(nullptr);

    // Allocates DPCs for all processors.
    auto context = reinterpret_cast<ExclusivityContext *>(ExAllocatePoolWithTag(
        NonPagedPoolNx, sizeof(void *) + (numberOfProcessors * sizeof(KDPC)),
        EXCLP_POOL_TAG));
    if (!context)
    {
        return nullptr;
    }

    // Execute a lock DPC for all processors but this.
    context->OldIrql = KeRaiseIrqlToDpcLevel();
    const auto currentCpu = KeGetCurrentProcessorNumber();
    for (auto i = 0ul; i < numberOfProcessors; i++)
    {
        if (i == currentCpu)
        {
            continue;
        }

        // Queue a lock DPC.
        KeInitializeDpc(&context->Dpcs[i], ExclpRaiseIrqlAndWaitDpc, nullptr);
        KeSetTargetProcessorDpc(&context->Dpcs[i], static_cast<CCHAR>(i));
        KeInsertQueueDpc(&context->Dpcs[i], nullptr, nullptr);
    }

    // Wait until all other processors were halted.
    const auto needToBeLocked = numberOfProcessors - 1;
    while (_InterlockedCompareExchange(&g_ExclpNumberOfLockedProcessors,
        needToBeLocked, needToBeLocked) !=
        static_cast<LONG>(needToBeLocked))
    {
        KeStallExecutionProcessor(10);
    }
    return context;
}

_Use_decl_annotations_ EXTERN_C
void ExclReleaseExclusivity(
    void *Exclusivity)
{
    if (!Exclusivity)
    {
        return;
    }

    // Tell other processors they can be unlocked with changing the value.
    _InterlockedIncrement(&g_ExclpReleaseAllProcessors);

    // Wait until all other processors were unlocked.
    while (_InterlockedCompareExchange(&g_ExclpNumberOfLockedProcessors, 0, 0))
    {
        KeStallExecutionProcessor(10);
    }

    auto context = static_cast<ExclusivityContext *>(Exclusivity);
    KeLowerIrql(context->OldIrql);
    ExFreePoolWithTag(Exclusivity, EXCLP_POOL_TAG);
}

// Locks this processor until g_ReleaseAllProcessors becomes 1.
_Use_decl_annotations_ EXTERN_C static
void ExclpRaiseIrqlAndWaitDpc(
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    // Increase the number of locked processors.
    _InterlockedIncrement(&g_ExclpNumberOfLockedProcessors);

    // Wait until g_ReleaseAllProcessors becomes 1.
    while (!_InterlockedCompareExchange(&g_ExclpReleaseAllProcessors, 1, 1))
    {
        KeStallExecutionProcessor(10);
    }

    // Decrease the number of locked processors.
    _InterlockedDecrement(&g_ExclpNumberOfLockedProcessors);
}
