#pragma once

// C/C++ standard headers
#include <cstdint>
#include <string>

// Other external headers
// Windows headers
#include <tchar.h>

// Support UNICODE build for DbgHelp.h
#ifdef UNICODE
#define DBGHELP_TRANSLATE_TCHAR
#endif
//
// Note from MSDN:
//      All DbgHelp functions, such as this one, are single threaded.
//      Therefore, calls from more than one thread to this function will
//      likely result in unexpected behavior or memory corruption.
//      To avoid this, you must synchronize all concurrent calls
//      from more than one thread to this function.
//
// It should not matter in this program at the current time as it is a single
// thread program.
//
#include <DbgHelp.h>

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

class SymbolResolver
{
public:
    SymbolResolver();

    std::uint64_t loadModule(
        __in const std::basic_string<TCHAR>& ModulePath);

    bool unloadModule(
        __in std::uint64_t BaseAddress);

    std::uint64_t getOffset(
        __in const std::basic_string<TCHAR>& SymbolName) const;


private:
    using SymInitializeType = decltype(&SymInitialize);
    using SymCleanupType = decltype(&SymCleanup);
    using SymSetOptionsType = decltype(&SymSetOptions);
    using SymGetOptionsType = decltype(&SymGetOptions);
    using SymFromNameType = decltype(&SymFromName);
    using SymLoadModuleExType = decltype(&SymLoadModuleEx);
    using SymUnloadModule64Type = decltype(&SymUnloadModule64);
    using SymGetSearchPathType = decltype(&SymGetSearchPath);


    std::experimental::unique_resource_t<HMODULE, decltype(&::FreeLibrary)> m_DbgHelp;
    const SymInitializeType m_SymInitialize;
    const SymCleanupType m_SymCleanup;
    const SymSetOptionsType m_SymSetOptions;
    const SymGetOptionsType m_SymGetOptions;
    const SymFromNameType m_SymFromName;
    const SymLoadModuleExType m_SymLoadModuleEx;
    const SymUnloadModule64Type m_SymUnloadModule64;
    const SymGetSearchPathType m_SymGetSearchPath;
    const std::experimental::unique_resource_t<HANDLE, SymCleanupType> m_Process;
};


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

