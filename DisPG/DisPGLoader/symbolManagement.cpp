#include "stdafx.h"
#include "symbolManagement.h"

// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers
#include "util.h"
#include "SymbolResolver.h"
#include "SymbolAddressDeriver.h"


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

namespace {

using DriverInfo = std::pair<std::uintptr_t, std::basic_string<TCHAR>>;
using DriverInfoList = std::vector<DriverInfo>;

} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

namespace {

DriverInfoList GetDriverList();

std::vector<std::basic_string<TCHAR>> GetRequireSymbolNames();

} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Resolve all necessary symbols and register the addresses to the registry.
// This functions gets the list of kernel modules and checks if the module is
// needed to resolve a symbol one by one.
bool RegisterSymbolInformation(
    __in const std::basic_string<TCHAR>& RegistryPath)
{
    // Get a full path of system32
    std::array<TCHAR, MAX_PATH> sysDir_;
    ::GetSystemDirectory(sysDir_.data(), static_cast<UINT>(sysDir_.size()));
    std::basic_string<TCHAR> sysDir(sysDir_.data());
    sysDir += TEXT("\\");

    // Get a name list of required symbols
    const auto requireSymbols = GetRequireSymbolNames();

    SymbolResolver resolver;

    // Do follow for each driver files loaded in the kernel.
    for (const auto& driverInfo : GetDriverList())
    {
        // Get a base name of the driver
        const auto driverBaseName = driverInfo.second.substr(
            0, driverInfo.second.find(TEXT('.')));

        // Check if this driver is in the required list
        for (const auto& requireSymbol : requireSymbols)
        {
            // Get a base name of the required symbol name
            const auto requireBaseName = requireSymbol.substr(
                0, requireSymbol.find(TEXT('!')));

            // ignore if it is a different module
            if (requireBaseName != driverBaseName)
            {
                continue;
            }

            // Get an address of the symbol
            SymbolAddressDeriver deriver(&resolver,
                sysDir + driverInfo.second, driverInfo.first);
            const auto address = deriver.getAddress(requireSymbol);
            if (!address)
            {
                std::basic_stringstream<TCHAR> ss;
                ss << requireSymbol << TEXT(" could not be solved.");
                const auto str = ss.str();
                PrintErrorMessage(str.c_str());
                return false;
            }

            // Save the address to the registry
            if (!RegWrite64Value(RegistryPath, requireSymbol, address))
            {
                PrintErrorMessage(TEXT("RegSetPtr failed."));
                return false;
            }
            _tprintf(_T("%016llX : %s\n"), address, requireSymbol.c_str());
        }
    }
    return true;
}


namespace {


// Get a list of file names of drivers that are currently loaded in the kernel.
DriverInfoList GetDriverList()
{
    // Determine the current number of drivers
    DWORD needed = 0;
    std::array<void*, 1000> baseAddresses;
    if (!::EnumDeviceDrivers(baseAddresses.data(),
        static_cast<DWORD>(baseAddresses.size() * sizeof(void*)), &needed))
    {
        ThrowRuntimeError(TEXT("EnumDeviceDrivers failed."));
    }

    // Collect their base names
    DriverInfoList list;
    const auto numberOfDrivers = needed / sizeof(baseAddresses.at(0));
    for (std::uint32_t i = 0; i < numberOfDrivers; ++i)
    {
        std::array<TCHAR, MAX_PATH> name;
        if (!::GetDeviceDriverBaseName(baseAddresses.at(i),
            name.data(), static_cast<DWORD>(name.size())))
        {
            ThrowRuntimeError(TEXT("GetDeviceDriverBaseName failed."));
        }
        std::transform(name.begin(), name.end(), name.begin(), _totlower);
        list.emplace_back(
            reinterpret_cast<std::uintptr_t>(baseAddresses.at(i)),
            name.data());
    }
    return list;
}


// Returns a list of required symbols
std::vector<std::basic_string<TCHAR>> GetRequireSymbolNames()
{
    std::vector<std::basic_string<TCHAR>> list;

    // All platforms
    list.emplace_back(TEXT("ntoskrnl!ExAcquireResourceSharedLite"));

    if (IsWindows8Point1OrGreater())
    {
        // 8.1
        list.emplace_back(TEXT("ntoskrnl!KiScbQueueScanWorker"));
        list.emplace_back(TEXT("ntoskrnl!HalPerformEndOfInterrupt"));
        list.emplace_back(TEXT("ntoskrnl!KiCommitThreadWait"));
        list.emplace_back(TEXT("ntoskrnl!KiAttemptFastRemovePriQueue"));
        list.emplace_back(TEXT("ntoskrnl!KeDelayExecutionThread"));
        list.emplace_back(TEXT("ntoskrnl!KeWaitForSingleObject"));
    }
    else
    {
        // 7, Vista, XP
        list.emplace_back(TEXT("ntoskrnl!PoolBigPageTable"));
        list.emplace_back(TEXT("ntoskrnl!PoolBigPageTableSize"));
        list.emplace_back(TEXT("ntoskrnl!MmNonPagedPoolStart"));
    }
    return list;
}


} // End of namespace {unnamed}

