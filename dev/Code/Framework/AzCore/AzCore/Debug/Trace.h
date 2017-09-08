/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#ifndef AZCORE_TRACE_H
#define AZCORE_TRACE_H 1

#include <AzCore/PlatformDef.h>
#include <AzCore/Debug/TraceResult.h>

#if defined(AZ_COMPILER_MSVC)
//# pragma warning(push)
#   pragma warning(disable:4127) //conditional expression is constant (use for conditional check everywhere)
#endif // AZ_COMPILER_MSVC

#if defined(AZ_COMPILER_SNC)
//# pragma diag_push
#   pragma diag_suppress=237 // controlling expression is constant
#endif // AZ_COMPILER_SNC

#ifndef AZ_ENABLE_BREAK
#define AZ_ENABLE_BREAK 1
#endif

#ifndef AZ_DEBUG_BREAK
#   if defined(AZ_PLATFORM_X360)
#      include <xbdm.h>
#   elif defined(AZ_PLATFORM_PS4)
#      include <libdbg.h>
#   elif defined(AZ_PLATFORM_LINUX) || defined(AZ_PLATFORM_ANDROID)
#      if defined(AZ_PLATFORM_ANDROID)
#          include <android/log.h>
#      endif // AZ_PLATFORM_ANDROID
#      include <AzCore/Debug/TraceCppLinux.inl>
#   elif defined(AZ_PLATFORM_APPLE)
#      include <AzCore/Debug/TraceCppApple.inl>
#   endif

#   if AZ_ENABLE_BREAK
#      if defined(AZ_PLATFORM_WINDOWS) || defined(AZ_PLATFORM_XBONE)
#          define AZ_DEBUG_BREAK    __debugbreak();
#      elif defined(AZ_PLATFORM_X360)
/ / Redacted
#          define AZ_DEBUG_BREAK
#      elif defined(AZ_PLATFORM_LINUX)
#          define AZ_DEBUG_BREAK    DEBUG_BREAK;
#      elif defined(AZ_PLATFORM_ANDROID)
#          include <signal.h>
#          define AZ_DEBUG_BREAK    raise(SIGINT);
#      elif defined(AZ_PLATFORM_APPLE)
#          define AZ_DEBUG_BREAK    __builtin_trap();
#      else
#          define AZ_DEBUG_BREAK    (*((int*)0)) = 1;
#      endif
#   else
#      define AZ_DEBUG_BREAK
#   endif // AZ_ENABLE_BREAK
#endif // AZ_DEBUG_BREAK

/**
    *  Forward declarations
    */
namespace AZStd
{
    template <typename Signature>
    class delegate;
}

namespace AZ
{
    namespace Debug
    {
        /// Global instance to the tracer.
        extern class Trace      g_tracer;

        class Trace
        {
        public:
            static Trace& Instance()    { return g_tracer; }

            bool IsDebuggerPresent();

            /// True or false if we want to handle system exceptions.
            void HandleExceptions(bool isEnabled);

            /// Breaks program execution immediately.
            void Break();

            Debug::Result Assert(const char* expression, const char* fileName, int line, const char* funcName, const char* format, ...);
            Debug::Result Error(const char* expression, const char* fileName, int line, const char* funcName, const char* window, const char* format, ...);
            void Warning(const char* expression, const char* fileName, int line, const char* funcName, const char* window, const char* format, ...);
            void Printf(const char* window, const char* format, ...);

            void Output(const char* window, const char* message);

             void PrintCallstack(const char* window, unsigned int suppressCount = 0, void* nativeContext = 0);

            /// PEXCEPTION_POINTERS on Windows/X360, always NULL on other platforms
            void* GetNativeExceptionInfo();
        };
    }
}

#ifdef AZ_ENABLE_TRACING

/**
* AZ tracing macros provide debug information reporting for assert, errors, warnings, and informational messages.
* The syntax allows printf style formatting for the message, e.g DAZ_Error("System | MyWindow", true/false, "message %d", ...).
* Asserts are always sent to the "System" window, since they cannot be ignored.
*
* The four different types of macro should be used depending on the situation:
*    - Asserts should be used for critical errors, where the program cannot continue. They print the message together
*      with file and line number, and a call stack if available. They then break program execution.
*    - Errors should be used where something is clearly wrong, but the program can continue safely. They print the message
*      together with file and line number, and a call stack if available. Depending on platform they will notify the user that
*      an error has occurred, e.g. with a message box or an on-screen message.
*    - Warnings should be used when something could be wrong. They print the message together with file and line number, and
*      a call stack if available, but take no other action.
*    - Printfs are purely informational. They print the message unadorned.
*    - Traces which have "Once" at the end will display the message only once for the life of the application instance.
*
* \note In the future we can change the macros to #define AZ_Assert(expression,format,...) this is supported by all compilers except
* Metroweirks. They require at least one parameter for "...".
* \note AZCore implements the AZStd::Deafult_Assert so if you want to customize it user the Tracer Listerners...
*/
    #define AZ_Assert(expression, ...) \
        if (!(expression)) { \
            switch (AZ::Debug::Trace::Instance().Assert(#expression, __FILE__, __LINE__, AZ_FUNCTION_SIGNATURE, /*format,*/ __VA_ARGS__)) { \
                case AZ::Debug::Result::Break: { AZ_DEBUG_BREAK; } break; \
                default: break; \
            } \
        }

    #define AZ_Error(window, expression, ...) \
        if (!(expression)) { \
            switch (AZ::Debug::Trace::Instance().Error(#expression, __FILE__, __LINE__, AZ_FUNCTION_SIGNATURE, window, /*format,*/ __VA_ARGS__)) { \
                case AZ::Debug::Result::Break: { AZ_DEBUG_BREAK; } break; \
                default: break; \
            } \
        }

    #define AZ_ErrorOnce(window, expression, ...) \
    if (!(expression)) { \
        static bool isDisplayed = false; \
        if (!isDisplayed) { \
            isDisplayed = true; \
            switch (AZ::Debug::Trace::Instance().Error(#expression, __FILE__, __LINE__, AZ_FUNCTION_SIGNATURE, window, /*format,*/ __VA_ARGS__)) { \
                case AZ::Debug::Result::Break: { AZ_DEBUG_BREAK; } break; \
                default: break; \
            } \
        } \
    }

    #define AZ_Warning(window, expression, ...) \
        if (!(expression)) { \
            AZ::Debug::Trace::Instance().Warning(#expression, __FILE__, __LINE__, AZ_FUNCTION_SIGNATURE, window, /*format,*/ __VA_ARGS__); \
        }

    #define AZ_WarningOnce(window, expression, ...) \
        if (!(expression)) { \
            static bool isDisplayed = false; \
            if (!isDisplayed) { \
                isDisplayed = true; \
                AZ::Debug::Trace::Instance().Warning(#expression, __FILE__, __LINE__, AZ_FUNCTION_SIGNATURE, window, /*format,*/ __VA_ARGS__); \
            } \
        }

    #define AZ_TracePrintf(window, ...) \
        AZ::Debug::Trace::Instance().Printf(window, __VA_ARGS__);

/*!
 * Verify version of the trace checks evaluates the expression even in release.
 *
 * i.e.
 *   // with assert
 *   buffer = azmalloc(size,alignment);
 *   AZ_Assert( buffer != nullptr,"Assert Message");
 *   ...
 *
 *   // with verify
 *   AZ_Verify((buffer = azmalloc(size,alignment)) != nullptr,"Assert Message")
 *   ...
 */
    #define AZ_Verify(expression, ...) AZ_Assert(0 != (expression), __VA_ARGS__)
    #define AZ_VerifyError(window, expression, ...) AZ_Error(window, 0 != (expression), __VA_ARGS__)
    #define AZ_VerifyWarning(window, expression, ...) AZ_Warning(window, 0 != (expression), __VA_ARGS__)

#else // !AZ_ENABLE_TRACING
    #define AZ_Assert(expression, ...)
    #define AZ_Error(window, expression, ...)
    #define AZ_ErrorOnce(window, expression, ...)
    #define AZ_Warning(window, expression, ...)
    #define AZ_WarningOnce(window, expression, ...)
    #define AZ_TracePrintf(window, ...)

    #define AZ_Verify(expression, ...) (expression)
    #define AZ_VerifyError(window, expression, ...) (expression)
    #define AZ_VerifyWarning(window, expression, ...) (expression)

#endif  // AZ_ENABLE_TRACING

#define AZ_Printf(window, ...)       AZ::Debug::Trace::Instance().Printf(window, __VA_ARGS__);

#if defined(AZ_DEBUG_BUILD)
#   define AZ_DbgIf(expression)         if (expression)
#   define AZ_DbgElseIf(expression)     else if (expression)
#else
#   define AZ_DbgIf(expression)         if (0)
#   define AZ_DbgElseIf(expression)     else if (0)
#endif

#if defined(AZ_COMPILER_MSVC)
//# pragma warning(pop)
#endif // AZ_COMPILER_MSVC

#if defined(AZ_COMPILER_SNC)
//# pragma diag_pop
#endif // AZ_COMPILER_SNC

#endif // AZCORE_TRACE_H
#pragma once