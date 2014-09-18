#include "stdafx.h"
#include "util.h"

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

// Return an error message of corresponding Win32 error code.
std::basic_string<TCHAR> GetWin32ErrorMessage(
    __in std::uint32_t ErrorCode)
{
    TCHAR* message = nullptr;
    if (!::FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
        ErrorCode, LANG_USER_DEFAULT, reinterpret_cast<LPTSTR>(&message), 0,
        nullptr))
    {
        return TEXT("");
    }
    if (!message)
    {
        return TEXT("");
    }
    auto messageDeleter = std::experimental::scope_guard(
        [message]() { ::LocalFree(message); });

    const auto length = ::_tcslen(message);
    if (!length)
    {
        return TEXT("");
    }

    if (message[length - 2] == TEXT('\r'))
    {
        message[length - 2] = TEXT('\0');
    }
    return message;
}


// Display an error message with an error message of the current error code.
void PrintErrorMessage(
    __in const std::basic_string<TCHAR>& Message)
{
    const auto errorCode = ::GetLastError();
    const auto errorMessage = GetWin32ErrorMessage(errorCode);
    ::_ftprintf_s(stderr, _T("%s : %lu(0x%08x) : %s\n"),
        Message.c_str(), errorCode, errorCode, errorMessage.c_str());
}


// Throw std::runtime_error with an error message.
void ThrowRuntimeError(
    __in const std::basic_string<TCHAR>& Message)
{
    const auto errorCode = ::GetLastError();
    const auto errorMessage = GetWin32ErrorMessage(errorCode);
    char msg[1024];
#if UNICODE
    static const char FORMAT_STR[] = "%S : %lu(0x%08x) : %S";
#else
    static const char FORMAT_STR[] = "%s : %lu(0x%08x) : %s";
#endif
    StringCchPrintfA(msg, _countof(msg), FORMAT_STR,
        Message.c_str(), errorCode, errorCode, errorMessage.c_str());
    throw std::runtime_error(msg);
}


// Save 64 bits value to the registry
bool RegWrite64Value(
    __in const std::basic_string<TCHAR>& Path,
    __in const std::basic_string<TCHAR>& Name,
    __in std::uint64_t Value)
{
    HKEY key = nullptr;
    auto status = ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, Path.c_str(), 0, nullptr,
        0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (!SUCCEEDED(status))
    {
        return false;
    }
    auto keyDeleter = std::experimental::scope_guard(
        [key]() { ::RegCloseKey(key); });

    status = ::RegSetValueEx(key, Name.c_str(), 0, REG_BINARY,
        reinterpret_cast<const BYTE*>(&Value), sizeof(Value));
    return SUCCEEDED(status);
}

