#include "stdafx.h"
#include "serviceManagement.h"

// C/C++ standard headers
// Other external headers
// Windows headers
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

// Returns true when a specified service has been installed.
bool IsServiceInstalled(
    __in const std::basic_string<TCHAR>& ServiceName)
{
    auto scmHandle = std::experimental::unique_resource(
        ::OpenSCManager(nullptr, nullptr, GENERIC_READ), 
        ::CloseServiceHandle);
    return (FALSE != ::CloseServiceHandle(
        ::OpenService(scmHandle, ServiceName.c_str(), GENERIC_READ)));
}


// Loads a driver file by registering a service. It creates some extra keys that
// needed when the service is a file system mini-filter driver.
bool LoadDriver(
    __in const std::basic_string<TCHAR>& ServiceName,
    __in const std::basic_string<TCHAR>& DriverFile)
{
    static const TCHAR INSTANCE_NAME[] = TEXT("instance_name");
    static const TCHAR ALTITUDE[] = TEXT("370040");

    std::basic_string<TCHAR> registry =
        TEXT("SYSTEM\\CurrentControlSet\\Services\\");
    registry += ServiceName;
    registry += TEXT("\\Instances");

    // Create registry key for the service
    HKEY key = nullptr;
    auto result = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, registry.c_str(), 0,
        nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, nullptr);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }
    auto keyDeleter = std::experimental::scope_guard(
        [key]() { ::RegCloseKey(key); });

    // Set 'DefaultInstance'. It may be used when the service is a file system
    // mini-filter driver. Otherwise, it will simply be ignored.
    result = ::RegSetValueEx(key, TEXT("DefaultInstance"), 0, REG_SZ,
        reinterpret_cast<const BYTE*>(INSTANCE_NAME), sizeof(INSTANCE_NAME));
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    registry += TEXT("\\");
    registry += INSTANCE_NAME;

    // Create a sub key
    HKEY keySub = nullptr;
    result = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, registry.c_str(), 0,
        nullptr, 0, KEY_ALL_ACCESS, nullptr, &keySub, nullptr);
    if (result != ERROR_SUCCESS)
    {
        return false;
    }
    auto keySubDeleter = std::experimental::scope_guard(
        [keySub]() { ::RegCloseKey(keySub); });

    // Set 'Altitude'. It may be used when the service is a file system
    // mini-filter driver. Otherwise, it will simply be ignored.
    result = ::RegSetValueEx(keySub, TEXT("Altitude"), 0,
        REG_SZ, reinterpret_cast<const BYTE*>(ALTITUDE), sizeof(ALTITUDE));
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    // Set 'Flags'. It may be used when the service is a file system
    // mini-filter driver. Otherwise, it will simply be ignored
    DWORD regValue = 0;
    result = ::RegSetValueEx(keySub, TEXT("Flags"), 0,
        REG_DWORD, reinterpret_cast<const BYTE*>(&regValue), sizeof(regValue));
    if (result != ERROR_SUCCESS)
    {
        return false;
    }

    auto scmHandle = std::experimental::unique_resource(
        ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE), 
        ::CloseServiceHandle);
    if (!scmHandle)
    {
        return false;
    }

    // Create a service as a file system mini-filter driver regardless of its
    // actual type since it is just more capable.
    auto serviceHandle = std::experimental::unique_resource(
        ::CreateService(
            scmHandle, ServiceName.c_str(), ServiceName.c_str(),
            SERVICE_ALL_ACCESS, SERVICE_FILE_SYSTEM_DRIVER,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, DriverFile.c_str(),
            TEXT("FSFilter Activity Monitor"), nullptr, TEXT("FltMgr"), nullptr,
            nullptr),
        ::CloseServiceHandle);
    if (!serviceHandle)
    {
        return false;
    }

    // Start the service and wait until its status becomes anything but
    // SERVICE_START_PENDING.
    SERVICE_STATUS status = {};
    if (::StartService(serviceHandle, 0, nullptr))
    {
        while (::QueryServiceStatus(serviceHandle, &status))
        {
            if (status.dwCurrentState != SERVICE_START_PENDING)
            {
                break;
            }
            ::Sleep(500);
        }
    }

    // Error if the status is not SERVICE_RUNNING.
    if (status.dwCurrentState != SERVICE_RUNNING)
    {
        ::DeleteService(serviceHandle);
        return false;
    }
    return true;
}


// Unloads a driver and deletes its service.
bool UnloadDriver(
    __in const std::basic_string<TCHAR>& ServiceName)
{
    // Delete the service
    auto scmHandle = std::experimental::unique_resource(
        ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT), 
        ::CloseServiceHandle);
    if (!scmHandle)
    {
        return false;
    }

    auto serviceHandle = std::experimental::unique_resource(
        ::OpenService(scmHandle, ServiceName.c_str(), 
            DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS),
        ::CloseServiceHandle);
    if (!serviceHandle)
    {
        return false;
    }

    ::DeleteService(serviceHandle);

    // Stop the service
    SERVICE_STATUS status = {};
    if (::ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status))
    {
        while (::QueryServiceStatus(serviceHandle, &status))
        {
            if (status.dwCurrentState != SERVICE_START_PENDING)
            {
                break;
            }
            ::Sleep(500);
        }
    }

    // Delete all related registry keys
    std::basic_string<TCHAR> registry =
        TEXT("SYSTEM\\CurrentControlSet\\Services\\");
    registry += ServiceName;
    registry += TEXT("\\");
    ::SHDeleteKey(HKEY_LOCAL_MACHINE, registry.c_str());

    return (status.dwCurrentState == SERVICE_STOPPED);
}

