// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

extern "C" {
#pragma warning(push, 0)
#include <fltKernel.h>
#include <windef.h>
#include <ntimage.h>
#include <stdarg.h>
#define NTSTRSAFE_NO_CB_FUNCTIONS
#include <ntstrsafe.h>
#include <ntddstor.h>
#include <mountdev.h>
#include <ntddvol.h>
#pragma warning(pop)
#pragma warning(disable:4100)
}

#ifndef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS     0
#endif
#include "../../Common/unique_resource.h"
#include "../../Common/scope_guard.h"



////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//

// Specifies where the code should be located
#ifdef ALLOC_PRAGMA
#define ALLOC_TEXT(Section, Name)   __pragma(alloc_text(Section, Name))
#else
#define ALLOC_TEXT(Section, Name)
#endif

// _countof. You do not want to type RTL_NUMBER_OF, do you?
#ifndef _countof
#define _countof(x)    RTL_NUMBER_OF(x)
#endif

// DbgPrintEx displays messages regardless of the filter settings
#ifndef DBG_PRINT
#define DBG_PRINT(format, ...)  \
    DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, (format), __VA_ARGS__)
#endif

// Break point that works only when a debugger is enabled
#ifndef DBG_BREAK
#define DBG_BREAK()             \
    if (KD_DEBUGGER_ENABLED) { __debugbreak(); } else {} ((void*)0)
#endif


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

