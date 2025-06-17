/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void Logger_Log(LogChannel* channel, uint32 logLevel, const char* funcName, uint32 line, const char* message)
    {
        if (!channel)
        {
            channel = &Log_Script;
        }

        if (logLevel > uint32(LogLevel::FATAL))
        {
            logLevel = uint32(LogLevel::FATAL);
        }

        switch (LogLevel(logLevel))
        {
        case LogLevel::DEBUG:
            logging::LogDynamicChannel<logging::Debug(), StaticString("<script>"), StaticString("{}:{}: {}\n")>(logging::GetLogger(), *channel, funcName, line, message);

            break;
        case LogLevel::INFO:
            logging::LogDynamicChannel<logging::Info(), StaticString("<script>"), StaticString("{}:{}: {}\n")>(logging::GetLogger(), *channel, funcName, line, message);

            break;
        case LogLevel::WARNING:
            logging::LogDynamicChannel<logging::Warning(), StaticString("<script>"), StaticString("{}:{}: {}\n")>(logging::GetLogger(), *channel, funcName, line, message);

            break;
        case LogLevel::ERR:
            logging::LogDynamicChannel<logging::Error(), StaticString("<script>"), StaticString("{}:{}: {}\n")>(logging::GetLogger(), *channel, funcName, line, message);

            break;
        case LogLevel::FATAL:
            logging::LogDynamicChannel<logging::Fatal(), StaticString("<script>"), StaticString("{}:{}: {}")>(logging::GetLogger(), *channel, funcName, line, message);

            break;
        }
    }

    HYP_EXPORT const LogChannel* Logger_FindLogChannel(WeakName* name)
    {
        if (!name)
        {
            return nullptr;
        }

        return Logger::GetInstance().FindLogChannel(*name);
    }

    HYP_EXPORT LogChannel* Logger_CreateLogChannel(const char* name)
    {
        const Name channelName = CreateNameFromDynamicString(name);

        return Logger::GetInstance().CreateDynamicLogChannel(channelName, &Log_Script);
    }

    HYP_EXPORT void Logger_DestroyLogChannel(LogChannel* logChannel)
    {
        if (!logChannel)
        {
            return;
        }

        Logger::GetInstance().DestroyDynamicLogChannel(logChannel);
    }

} // extern "C"