#pragma once


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

typedef struct _POOL_TRACKER_BIG_PAGES
{
    PVOID Va;
    ULONG Key;
    ULONG PoolType;
    SIZE_T Size;    // InBytes
} POOL_TRACKER_BIG_PAGES, *PPOOL_TRACKER_BIG_PAGES;
C_ASSERT(sizeof(POOL_TRACKER_BIG_PAGES) == 0x18);


typedef struct _POOL_TRACKER_BIG_PAGES_XP
{
    PVOID Va;
    ULONG Key;
    ULONG Size;     //  InPages
    void* QuotaObject;
} POOL_TRACKER_BIG_PAGES_XP, *PPOOL_TRACKER_BIG_PAGES_XP;
C_ASSERT(sizeof(POOL_TRACKER_BIG_PAGES_XP) == 0x18);


struct WinXSymbols
{
    ULONG_PTR ExAcquireResourceSharedLite;
    SIZE_T* PoolBigPageTableSize;
    ULONG_PTR* MmNonPagedPoolStart;
    POOL_TRACKER_BIG_PAGES** PoolBigPageTable;
    POOL_TRACKER_BIG_PAGES_XP** PoolBigPageTableXp;
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

EXTERN_C
NTSTATUS WinXDisablePatchGuard(
    __in const WinXSymbols& Symbols);


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

