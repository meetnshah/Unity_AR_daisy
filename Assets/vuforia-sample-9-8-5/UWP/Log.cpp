/*===============================================================================
Copyright (c) 2020, PTC Inc. All rights reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "pch.h"
#include "Log.h"

#include <winrt/windows.foundation.diagnostics.h>

#include <cassert>
#include <cstdio>
#include <string>

using namespace winrt;

constexpr size_t MAX_LOG_MESSAGE = 1024;
static Windows::Foundation::Diagnostics::LoggingChannel sLoggingChannel(nullptr);


void LOG(const char* message, ...)
{
    assert(message != nullptr);
    char str[MAX_LOG_MESSAGE];

    va_list args;
    va_start(args, message);
    _vsnprintf_s(str, MAX_LOG_MESSAGE, message, args);
    va_end(args);

    OutputDebugStringA(str);
    OutputDebugStringA("\n");

    if (sLoggingChannel == nullptr)
    {
        sLoggingChannel = Windows::Foundation::Diagnostics::LoggingChannel(winrt::to_hstring(LOG_TAG), Windows::Foundation::Diagnostics::LoggingChannelOptions());
        if (sLoggingChannel == nullptr)
        {
            OutputDebugStringW(L"Error creating LoggingChannel\n");
        }
        else
        {
            std::wstring logId;
            logId.append(L"Setup Windows Diagnostics logging to GUID ").append(winrt::to_hstring(sLoggingChannel.Id())).append(L"\n");
            OutputDebugStringW(logId.c_str());
        }
    }
    if (sLoggingChannel != nullptr && sLoggingChannel.IsEnabled())
    {
        std::wstring toLog;
        size_t u16_length = MultiByteToWideChar(CP_UTF8, 0, str, int(strlen(str)), NULL, 0);
        toLog.resize(u16_length);
        u16_length = MultiByteToWideChar(CP_UTF8, 0, str, int(strlen(str)), const_cast<wchar_t*>(toLog.data()), int(toLog.size()));
        if (u16_length == 0)
        {
            OutputDebugStringW(L"Log message dropped. Unicode conversion failed.");
            return;
        }

        sLoggingChannel.LogMessage(toLog.c_str(), Windows::Foundation::Diagnostics::LoggingLevel::Information);
    }
}
