#include "stdafx.h"

// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers
#include "../../Common/PgContext.h"


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


//----------------------------------------------------------------------------
//
// Base extension class.
// Extensions derive from the provided ExtExtension class.
//
// The standard class name is "Extension".  It can be
// overridden by providing an alternate definition of
// EXT_CLASS before including engextcpp.hpp.
//
//----------------------------------------------------------------------------
class EXT_CLASS : public ExtExtension
{
public:
    EXT_CLASS();
    virtual HRESULT Initialize();
    EXT_COMMAND_METHOD(analyzepg);
    EXT_COMMAND_METHOD(dumppg);

private:
    void Dump(
        __in ULONG64 PgContextAddress,
        __in_opt ULONG64 ReasonInfoAddress,
        __in ULONG64 FailureDependentInfo,
        __in ULONG64 TypeOfCorruption,
        __in bool NeedBugCheckBanner);

    template <typename PgContextType>
    void DumpInternal(
        __in ULONG64 PgContextAddress,
        __in_opt ULONG64 ReasonInfoAddress,
        __in ULONG64 FailureDependentInfo,
        __in ULONG64 TypeOfCorruption);

    void DumpPgContext(
        __in ULONG64 PgContextAddress,
        __in_opt ULONG64 ReasonInfoAddress,
        __in const PgContext_7& PgContext);

    void DumpPgContext(
        __in ULONG64 PgContextAddress,
        __in_opt ULONG64 ReasonInfoAddress,
        __in const PgContext_8_1& PgContext);

    void DumpForType106(
        __in ULONG64 FailureDependentInfo);

    enum class TargetOsVersion
    {
        Unsupported,
        WinXP,
        WinVista,
        Win7,
        Win8_1,
    };

    TargetOsVersion m_OsVersion;
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

namespace {

const char* GetTypeString(
    __in ULONG64 ErrorWasFound,
    __in ULONG64 BugChkParam4Type);

} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

// EXT_DECLARE_GLOBALS must be used to instantiate
// the framework's assumed globals.
EXT_DECLARE_GLOBALS();


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

EXT_CLASS::EXT_CLASS()
    : m_OsVersion(TargetOsVersion::Unsupported)
{
}


HRESULT EXT_CLASS::Initialize()
{
    // Initialize ExtensionApis to use dprintf and so on
    // when this extension is loaded.
    PDEBUG_CLIENT debugClient = nullptr;
    auto result = DebugCreate(__uuidof(IDebugClient),
        reinterpret_cast<void**>(&debugClient));
    if (!SUCCEEDED(result))
    {
        return result;
    }
    auto debugClientdDeleter = std::experimental::scope_guard(
        [debugClient]() { debugClient->Release(); });

    PDEBUG_CONTROL debugControl = nullptr;
    result = debugClient->QueryInterface(__uuidof(IDebugControl),
        reinterpret_cast<void**>(&debugControl));
    if (!SUCCEEDED(result))
    {
        return result;
    }
    auto debugControlDeleter = std::experimental::scope_guard(
        [debugControl]() { debugControl->Release(); });

    ExtensionApis.nSize = sizeof(ExtensionApis);
    result = debugControl->GetWindbgExtensionApis64(&ExtensionApis);
    if (!SUCCEEDED(result))
    {
        return result;
    }

    // Check if the target machine is a supported version.
    ULONG platformId = 0;
    ULONG major = 0;
    ULONG minor = 0;
    ULONG servicePackNumber = 0;
    std::array<char, 1024> buildStr;
    result = debugControl->GetSystemVersion(
        &platformId, &major, &minor, nullptr, 0, nullptr, &servicePackNumber,
        buildStr.data(), static_cast<ULONG>(buildStr.size()), nullptr);
    if (!SUCCEEDED(result))
    {
        return result;
    }

    const std::string target = buildStr.data();

    // Check if the target platform is supported
    struct VersionMap
    {
        std::string build;
        TargetOsVersion version;
    };
    const VersionMap supportedVersions[] =
    {
        { "Built by: 9600.17085", TargetOsVersion::Win8_1, },
        { "Built by: 9600.17041", TargetOsVersion::Win8_1, },
        { "Built by: 9600.16452", TargetOsVersion::Win8_1, },
        { "Built by: 7601.18409", TargetOsVersion::Win7, },
        { "Built by: 7601.18247", TargetOsVersion::Win7, },
    };
    for (const auto& supported : supportedVersions)
    {
        if (target.substr(0, supported.build.length()) == supported.build)
        {
            m_OsVersion = supported.version;
        }
    }
    if (m_OsVersion == TargetOsVersion::Unsupported)
    {
        dprintf("Unsupported version detected: %s\n", target.c_str());
        return S_FALSE;
    }

    // Display guide messages
    dprintf("Use ");
    debugControl->ControlledOutput(
        DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
        "<exec cmd=\"!analyzepg\">!analyzepg</exec>");
    dprintf(" to get detailed PatchGuard Bugcheck information.\n");
    dprintf("Use !dumppg <address> to display PatchGuard information"
            " located at the specific address.\n");
    return result;
}


// Exported command !analyzepg (no options)
EXT_COMMAND(analyzepg,
    "Analyzes current bugcheck 0x109: CRITICAL_STRUCTURE_CORRUPTION",
    "")
{
    ULONG bugCheckCode = 0;
    ULONG64 bugCheckParameter[4] = {};
    auto result = this->m_Control->ReadBugCheckData(
        &bugCheckCode,
        &bugCheckParameter[0],  // An address of the PatchGuard context
        &bugCheckParameter[1],  // An address of a validation structure
        &bugCheckParameter[2],  // Failure type dependent information
        &bugCheckParameter[3]); // Type of corruption
    if (!SUCCEEDED(result) || bugCheckCode != 0x109)
    {
        Err("No CRITICAL_STRUCTURE_CORRUPTION bugcheck information was"
            " derived.\n");
        return;
    }

    // Decode a PatchGuard context address from the 1st bug check
    // parameter. This magic value is come from ntoskrnl.exe
    ULONG64 pgContextAddr = bugCheckParameter[0] - 0xA3A03F5891C8B4E8;

    // Decode an address containing a validation structure that
    // found an error if it is available.
    ULONG64 reasonInfoAddr = 0;
    if (bugCheckParameter[1])
    {
        // This magic value is come from ntoskrnl.exe
        reasonInfoAddr = bugCheckParameter[1] - 0xB3B74BDEE4453415;
    }

    Dump(pgContextAddr, reasonInfoAddr, bugCheckParameter[2], bugCheckParameter[3], true);
}


// Exported command !dumppg <address>
EXT_COMMAND(dumppg,
    "Displays the PatchGuard context",
    "{;e64;address;An address of a PatchGuard context.}")
{
    Dump(GetUnnamedArgU64(0), 0, 0, 0, false);
}


// Implementation of displaying the PatchGuard context
void EXT_CLASS::Dump(
    __in ULONG64 PgContextAddress,
    __in_opt ULONG64 ReasonInfoAddress,
    __in ULONG64 FailureDependentInfo,
    __in ULONG64 TypeOfCorruption,
    __in bool NeedBugCheckBanner)
{
    // Displayed as a banner if NeedBugCheckBanner is true
    static const char BUG_CHECK_BANNER[] = R"RAW(
*******************************************************************************
*                                                                             *
*                        PatchGuard Bugcheck Analysis                         *
*                                                                             *
*******************************************************************************

CRITICAL_STRUCTURE_CORRUPTION (109)
)RAW";

    // Display a banner if needed.
    if (NeedBugCheckBanner)
    {
        Out(BUG_CHECK_BANNER);
    }

    // Handle version dependency
    switch (m_OsVersion)
    {
    case TargetOsVersion::WinXP:
        break;
    case TargetOsVersion::WinVista:
        break;
    case TargetOsVersion::Win7:
        DumpInternal<PgContext_7>(PgContextAddress, ReasonInfoAddress,
            FailureDependentInfo, TypeOfCorruption);
        break;
    case TargetOsVersion::Win8_1:
        DumpInternal<PgContext_8_1>(PgContextAddress, ReasonInfoAddress,
            FailureDependentInfo, TypeOfCorruption);
        break;
    }

    // In the case of type 0x106, the address of PatchGuard context is not
    // given (it does not exist).
    if (!PgContextAddress)
    {
        Out("\n");
        return;
    }

    // Display extra guide messages to help users to explore more details
    Out("Use:\n");
    std::stringstream ss;

    ss << "dps " << std::setfill('0') << std::setw(16) << std::hex
        << PgContextAddress << "+c8";
    auto cmd = ss.str();
    ss.str("");
    Out("    ");
    DmlCmdExec(cmd.c_str(), cmd.c_str());
    Out(" to display the structure of PatchGuard\n");

    Out("\n");
}


// Display a banner and PatchGuard context according with a target OS version
template <typename PgContextType>
void EXT_CLASS::DumpInternal(
    __in ULONG64 PgContextAddress,
    __in_opt ULONG64 ReasonInfoAddress,
    __in ULONG64 FailureDependentInfo,
    __in ULONG64 TypeOfCorruption)
{
    // In the case of type 0x106, neither the address of PatchGuard context nor
    // the address of the validation structure are given (do not exist).
    if (PgContextAddress == 0
     && ReasonInfoAddress == 0
     && TypeOfCorruption == 0x106)  // CcBcbProfiler
    {
        DumpForType106(FailureDependentInfo);
        return;
    }

    // Read memory contains the PatchGuard context.
    ULONG readBytes = 0;
    PgContextType pgContext = {};
    auto result = m_Data->ReadVirtual(PgContextAddress,
        &pgContext, sizeof(pgContext), &readBytes);
    if (!SUCCEEDED(result))
    {
        Err("The given address 0x%016I64x is not readable.\n", PgContextAddress);
        return;
    }
    DumpPgContext(PgContextAddress, ReasonInfoAddress, pgContext);
}


// Display the PatchGuard context for Win7
void EXT_CLASS::DumpPgContext(
    __in ULONG64 PgContextAddress,
    __in_opt ULONG64 ReasonInfoAddress,
    __in const PgContext_7& PgContext)
{
    static const char DUMP_FORMAT[] = R"RAW(
    PatchGuard Context      : %y, An address of PatchGuard context
    Validation Data         : %y, An address of validation data that caused the error
    Type of Corruption      : Available %I64d   : %I64x : %s
    Failure type dependent information      : %y
    Allocated memory base                   : %y
    Pg_SelfValidation                       : %y
    FsRtlUninitializeSmallMcb               : %y
    FsRtlMdlReadCompleteDevEx               : %y
    ContextSizeInBytes                      : %08x (Can be broken)
    DPC Routine                             : %y
    WorkerRoutine                           : %y

)RAW";
    const auto typeString = GetTypeString(PgContext.isErroFound,
                                PgContext.bugChkParam4Type);
    Out(DUMP_FORMAT,
        PgContextAddress,
        ReasonInfoAddress,
        PgContext.isErroFound, PgContext.bugChkParam4Type, typeString,
        PgContext.bugChkParam3,
        PgContext.PageBase,
        PgContextAddress + PgContext.offsetToPg_SelfValidation,
        PgContextAddress + PgContext.offsetToFsRtlUninitializeSmallMcb,
        PgContextAddress + PgContext.offsetToFsRtlUninitializeSmallMcb - 0x349c,
        PgContext.ContextSizeInQWord * sizeof(ULONG64) + 0xC8,
        PgContext.DcpRoutineToBeScheduled,
        PgContext.workerRoutine);
}


// Display the PatchGuard context for Win8.1
void EXT_CLASS::DumpPgContext(
    __in ULONG64 PgContextAddress,
    __in_opt ULONG64 ReasonInfoAddress,
    __in const PgContext_8_1& PgContext)
{
    static const char DUMP_FORMAT[] = R"RAW(
    PatchGuard Context      : %y, An address of PatchGuard context
    Validation Data         : %y, An address of validation data that caused the error
    Type of Corruption      : Available %I64d   : %I64x : %s
    Failure type dependent information      : %y
    Allocated memory base                   : %y
    Pg_SelfValidation                       : %y
    FsRtlUninitializeSmallMcb               : %y
    FsRtlMdlReadCompleteDevEx               : %y
    Pg_SelfEncryptWaitAndDecrypt            : %y
    OnTheFlyEncryption                      : %08x
    ContextSizeInBytes                      : %08x (Can be broken)
    shouldMmAllocateIndependentPagesBeUsed  : %08x
    schedulerType                           : %y
    DPC Routine                             : %y
    WorkerRoutine                           : %y
    shouldKeWaitForSingleObjectBeUsed       : %y
    shouldKiScbQueueWorkerBeUsed            : %y
    shouldPg_SelfEncryptWaitAndDecryptBeUsed: %y

)RAW";
    const auto typeString = GetTypeString(PgContext.isErroFound,
                                PgContext.bugChkParam4Type);
    Out(DUMP_FORMAT,
        PgContextAddress,
        ReasonInfoAddress,
        PgContext.isErroFound, PgContext.bugChkParam4Type, typeString,
        PgContext.bugChkParam3,
        PgContext.PageBase,
        PgContextAddress + PgContext.offsetToPg_SelfValidationInBytes,
        PgContextAddress + PgContext.offsetToFsUninitializeSmallMcbInBytes,
        PgContextAddress + PgContext.offsetToFsUninitializeSmallMcbInBytes - 0xa000,
        PgContext.Pg_SelfEncryptWaitAndDecrypt,
        PgContext.onTheFlyEnDecryptionFlag,
        PgContext.ContextSizeInQWord * sizeof(ULONG64) + 0xC8,
        PgContext.shouldMmAllocateIndependentPagesBeUsed,
        PgContext.schedulerType,
        PgContext.dcpRoutineToBeScheduled,
        PgContext.workerRoutine,
        PgContext.shouldKeWaitForSingleObjectBeUsed,
        PgContext.shouldKiScbQueueWorkerBeUsed,
        PgContext.shouldPg_SelfEncryptWaitAndDecryptBeUsed);
}


// Display info for a corruption type 0x106 on Win8.1
void EXT_CLASS::DumpForType106(
    __in ULONG64 FailureDependentInfo)
{
    static const char DUMP_FORMAT[] = R"RAW(
    PatchGuard Context      : %y, An address of PatchGuard context
    Validation Data         : %y, An address of validation data that caused the error
    Type of Corruption      : Available %I64d   : %I64x : %s
    Failure type dependent information      : %y

)RAW";
    static const auto TYPE_OF_CORRUPTION = 0x106ull;
    const auto typeString = GetTypeString(true, TYPE_OF_CORRUPTION);
    Out(DUMP_FORMAT,
        0ull,
        0ull,
        1ull, TYPE_OF_CORRUPTION, typeString,
        FailureDependentInfo);
}


namespace {


// Returns a description text of the corruption type number
const char* GetTypeString(
    __in ULONG64 ErrorWasFound,
    __in ULONG64 BugChkParam4Type)
{
    if (!ErrorWasFound)
    {
        return "N/A";
    }
    switch (BugChkParam4Type)
    {
    case 0: return "A generic data region";
    case 1: return "Modification of a function or.pdata";
    case 2: return "A processor IDT";
    case 3: return "A processor GDT";
    case 4: return "Type 1 process list corruption";
    case 5: return "Type 2 process list corruption";
    case 6: return "Debug routine modification";
    case 7: return "Critical MSR modification";
    case 0x101: return "Type 1 context modification";
    case 0x102: return "Not investigated :(";
    case 0x103: return "MmAttachSession failure";
    case 0x104: return "KeInsertQueueApc failure";
    case 0x105: return "RtlImageNtHeader failure";
    case 0x106: return "CcBcbProfiler detected modification";
    case 0x107: return "KiTableInformation corruption";
    case 0x108: return "Not investigated :(";
    case 0x109: return "Type 2 context modification";
    case 0x10E: return "Inconsistency between before and after sleeping";
    default: return "Unknown :(";
    }
}


} // End of namespace {unnamed}

