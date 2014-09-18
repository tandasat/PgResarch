#pragma once

// C/C++ standard headers
#include <cstdint>
#include <string>

// Other external headers
// Windows headers
#include <tchar.h>

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

// forward declaration
class SymbolResolver;


class SymbolAddressDeriver
{
public:
    SymbolAddressDeriver(
        __in SymbolResolver* SymbolResolver,
        __in const std::basic_string<TCHAR>& FilePath,
        __in std::uintptr_t BaseAddress);

    ~SymbolAddressDeriver();

    std::uint64_t getAddress(
        __in const std::basic_string<TCHAR>& SymbolName) const;

private:
    SymbolAddressDeriver& operator=(const SymbolAddressDeriver&) = delete;


    SymbolResolver* m_SymbolResolver;   // does not control its life span
    const std::uintptr_t m_BaseAddress;
    const std::uint64_t m_Module;
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

