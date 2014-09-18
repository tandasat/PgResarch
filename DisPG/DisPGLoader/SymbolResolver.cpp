#include "stdafx.h"
#include "SymbolResolver.h"

// C/C++ standard headers
#include <string>

// Other external headers
// Windows headers
#include <tchar.h>

// Original headers
#include "util.h"


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


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// 'to_string' macro to support to switch between ANSI and UNICODE builds.
#define SYMBOLRESOLVER_STRINGIFY(x)   #x
#define SYMBOLRESOLVER_TOSTRING(x)    SYMBOLRESOLVER_STRINGIFY(x)

SymbolResolver::SymbolResolver()
    : m_DbgHelp(std::experimental::unique_resource(::LoadLibrary(TEXT("DbgHelp.dll")), ::FreeLibrary))
    , m_SymInitialize(reinterpret_cast<SymInitializeType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymInitialize))))
    , m_SymCleanup(reinterpret_cast<SymCleanupType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymCleanup))))
    , m_SymSetOptions(reinterpret_cast<SymSetOptionsType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymSetOptions))))
    , m_SymGetOptions(reinterpret_cast<SymGetOptionsType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymGetOptions))))
    , m_SymFromName(reinterpret_cast<SymFromNameType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymFromName))))
    , m_SymLoadModuleEx(reinterpret_cast<SymLoadModuleExType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymLoadModuleEx))))
    , m_SymUnloadModule64(reinterpret_cast<SymUnloadModule64Type>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymUnloadModule64))))
    , m_SymGetSearchPath(reinterpret_cast<SymGetSearchPathType>(::GetProcAddress(m_DbgHelp, SYMBOLRESOLVER_TOSTRING(SymGetSearchPath))))
    , m_Process(std::experimental::unique_resource(::GetCurrentProcess(), m_SymCleanup))
{
// Delete these macros
#undef SYMBOLRESOLVER_STRINGIFY
#undef SYMBOLRESOLVER_TOSTRING

    if (!m_DbgHelp ||
        !m_SymInitialize ||
        !m_SymCleanup ||
        !m_SymSetOptions ||
        !m_SymGetOptions ||
        !m_SymFromName ||
        !m_SymLoadModuleEx ||
        !m_SymUnloadModule64 ||
        !m_SymGetSearchPath)
    {
        ThrowRuntimeError(TEXT("One of DbgHelp APIs was not initialized."));
    }

    const auto originalOptions = m_SymGetOptions();
    m_SymSetOptions(originalOptions | SYMOPT_CASE_INSENSITIVE |
        //SYMOPT_DEBUG |
        SYMOPT_UNDNAME | SYMOPT_AUTO_PUBLICS | SYMOPT_DEFERRED_LOADS);

    // Use a predefined symbols search path if _NT_SYMBOL_PATH is not registered
    const auto ret = ::GetEnvironmentVariable(
        TEXT("_NT_SYMBOL_PATH"), nullptr, 0);
    TCHAR* path = nullptr;
    if (ret == 0 && ::GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    {
        path = TEXT(
            ".;srv*.\\Symbols*http://msdl.microsoft.com/download/symbols");
    }

    if (!m_SymInitialize(m_Process, path, FALSE))
    {
        ThrowRuntimeError(TEXT("SymInitialize failed."));
    }
}


// Load a module and return its base address
std::uint64_t SymbolResolver::loadModule(
    __in const std::basic_string<TCHAR>& ModulePath)
{
    return m_SymLoadModuleEx(m_Process, nullptr, ModulePath.c_str(),
        nullptr, 0, 0, nullptr, 0);
}


// Unload a module
bool SymbolResolver::unloadModule(
    __in std::uint64_t BaseAddress)
{
    return !!m_SymUnloadModule64(m_Process, BaseAddress);
}


// Return an offset of the symbols from the base address of the module
std::uint64_t SymbolResolver::getOffset(
    __in const std::basic_string<TCHAR>& SymbolName) const
{
    ULONG64 buffer[(sizeof(SYMBOL_INFO)+
        MAX_SYM_NAME * sizeof(TCHAR)+
        sizeof(ULONG64)-1) / sizeof(ULONG64)] = {};
    auto symbol = reinterpret_cast<PSYMBOL_INFO>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    if (!m_SymFromName(m_Process, SymbolName.c_str(), symbol))
    {
        return 0;
    }
    return symbol->Address - symbol->ModBase;
}

