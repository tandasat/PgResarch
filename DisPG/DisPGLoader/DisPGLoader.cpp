#include "stdafx.h"

// C/C++ standard headers
// Other external headers
// Windows headers
#include <ImageHlp.h>
#pragma comment(lib, "Imagehlp.lib")

// Original headers
#include "util.h"
#include "serviceManagement.h"
#include "symbolManagement.h"


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

namespace {

bool AppMain(
    __in const std::vector<std::basic_string<TCHAR>>& Args);

bool IsSupportedEnvironment(
    __out std::uint64_t& KernelVersion);

std::basic_string<TCHAR> CreateFullPathFromName(
    __in const std::basic_string<TCHAR>& FileName);

std::basic_string<TCHAR> CreateServiceName(
    __in const std::basic_string<TCHAR>& DriverFullPath);

} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

// Entry point.
int _tmain(int argc, _TCHAR* argv[])
{
    int result = EXIT_FAILURE;
    try
    {
        std::vector<std::basic_string<TCHAR>> args;
        for (int i = 0; i < argc; ++i)
        {
            args.emplace_back(argv[i]);
        }
        if (AppMain(args))
        {
            result = EXIT_SUCCESS;
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unhandled exception occurred.\n";
    }
    std::system("pause");
    return result;
}


namespace {


bool AppMain(
    __in const std::vector<std::basic_string<TCHAR>>& Args)
{
    // Check if running platform is supported
    std::uint64_t kernelVersion = 0;
    if (!IsSupportedEnvironment(kernelVersion))
    {
        return false;
    }

    // Construct a full path of a given driver file name. The driver file name
    // is specified by the first command line option or 'DisPG.sys' as default.
    const auto driverFullPath = CreateFullPathFromName(
        ((Args.size() == 1) ? TEXT("DisPG.sys") : Args.at(1)));

    const auto serviceName = CreateServiceName(driverFullPath);

    // Check if the service exists
    if (IsServiceInstalled(serviceName))
    {
        // Uninstall the service when it has already been installed
        if (!UnloadDriver(serviceName))
        {
            PrintErrorMessage(TEXT("UnloadDriver failed."));
            return false;
        }
        std::cout << "Unloading the driver succeeded." << std::endl;
    }
    else
    {
        // Install the service when it has not been installed yet

        // Register necessary symbol information to the registry. Use the same
        // registry path as one used for the service as it makes reading value
        // from the driver easier.
        std::basic_string<TCHAR> registry =
            TEXT("SYSTEM\\CurrentControlSet\\Services\\");
        if (!RegisterSymbolInformation(registry + serviceName))
        {
            PrintErrorMessage(TEXT("RegisterSymbolInformation failed."));
            return false;
        }

        // Also, register the kernel file version to the registry.
        if (!RegWrite64Value(registry + serviceName, 
                TEXT("KernelVersion"), kernelVersion))
        {
            PrintErrorMessage(TEXT("RegWrite64Value failed."));
            return false;
        }

        // Then, load the driver
        if (!LoadDriver(serviceName, driverFullPath))
        {
            PrintErrorMessage(TEXT("LoadDriver failed."));
            return false;
        }
        std::cout << "Loading the driver succeeded." << std::endl;
    }
    return true;
}


// Return true if the running platform is supported.
bool IsSupportedEnvironment(
    __out std::uint64_t& KernelVersion)
{
    //// All platforms before Windows 8 are supported and do not require
    //// their detailed kernel versions
    //if (!IsWindows8OrGreater())
    //{
    //    return true;
    //}

    // Get a full path of system32
    std::array<TCHAR, MAX_PATH> sysDir;
    ::GetSystemDirectory(sysDir.data(), static_cast<UINT>(sysDir.size()));
    std::basic_string<TCHAR> ntos = sysDir.data();
    ntos += TEXT("\\ntoskrnl.exe");

    // Calculate check sum value of ntoskrnl.exe
    DWORD headerSum = 0;
    DWORD checkSum = 0;
    const auto result = ::MapFileAndCheckSum(
        ntos.c_str(), &headerSum, &checkSum);
    if (result != CHECKSUM_SUCCESS)
    {
        PrintErrorMessage(TEXT("MapFileAndCheckSum failed."));
        return false;
    }

    // Check if the check sum is in the following supported files list
    struct KernelHashMap
    {
        DWORD hash;
        std::uint64_t version;  // ntoskrnl.exe file version or 0 
                                // when it is not needed by the driver
    };
    static const KernelHashMap SUPPORTED_NTOSKRNL_HASHES[] =
    {
        { 0x0071c6d7, 17085, },     // Win 8.1
        { 0x00721d34, 17041, },     // Win 8.1
        { 0x007120d6, 16452, },     // Win 8.1
        { 0x00554c03, 0 },          // Win 7        18409
        { 0x0054cbb3, 0 },          // Win 7        18247
        { 0x00480ce6, 0 },          // Win Vista    18881
        { 0x00459d47, 0 },          // Win XP        5138
    };
    for (const auto& map : SUPPORTED_NTOSKRNL_HASHES)
    {
        if (checkSum == map.hash)
        {
            KernelVersion = map.version;
            return true;
        }
    }

    // Not. Display the checksum
    std::cout
        << "Unsupported ntoskrnl.exe ("
        << std::hex << std::setw(8) << std::setfill('0') << checkSum
        << ") was detected." << std::endl;
    return false;
}


// Build a full path corresponds to a given file name. Throw std::runtime_error
// when a given file does not exist.
std::basic_string<TCHAR> CreateFullPathFromName(
    __in const std::basic_string<TCHAR>& FileName)
{
    TCHAR fullPath[MAX_PATH];
    if (!::PathSearchAndQualify(FileName.c_str(),
        fullPath, _countof(fullPath)))
    {
        ThrowRuntimeError(TEXT("PathSearchAndQualify failed."));
    }

    if (!::PathFileExists(fullPath))
    {
        ThrowRuntimeError(TEXT("PathFileExists failed."));
    }
    return fullPath;
}


// Returns a base name of a given full path. For example, when the first
// parameter is 'C:\dir\name.exe', then the function returns 'name'.
std::basic_string<TCHAR> CreateServiceName(
    __in const std::basic_string<TCHAR>& DriverFullPath)
{
    TCHAR serviceName[MAX_PATH];
    if (!SUCCEEDED(::StringCchCopy(serviceName, _countof(serviceName),
        DriverFullPath.c_str())))
    {
        ThrowRuntimeError(TEXT("StringCchCopy failed."));
    }

    ::PathRemoveExtension(serviceName);
    ::PathStripPath(serviceName);
    return serviceName;
}


} // End of namespace {unnamed}

