#include "stdafx.h"
#include "SymbolAddressDeriver.h"

// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers
#include "SymbolResolver.h"


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

// Load a module to get ready for resolving symbols
SymbolAddressDeriver::SymbolAddressDeriver(
    __in SymbolResolver* SymbolResolver,
    __in const std::basic_string<TCHAR>& FilePath,
    __in std::uintptr_t BaseAddress)
    : m_SymbolResolver(SymbolResolver)
    , m_BaseAddress(BaseAddress)
    , m_Module(m_SymbolResolver->loadModule(FilePath))
{
}


// Unload a module
SymbolAddressDeriver::~SymbolAddressDeriver()
{
    m_SymbolResolver->unloadModule(m_Module);
}


// Resolve a symbol and return the address
std::uint64_t SymbolAddressDeriver::getAddress(
    const std::basic_string<TCHAR>& SymbolName) const
{
    const auto offset = m_SymbolResolver->getOffset(SymbolName);
    if (!offset)
    {
        return 0;
    }
    return m_BaseAddress + offset;
}

