/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-03-15 17:18:40
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 18:14:21
 * FilePath     : \\xlog\\src\\Distribution.cpp
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-03-15 17:07:40
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 17:12:41
 * FilePath     : \\xlog\\src\\Distribution.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Distribution.h"
#include "fmtlogDetail.h"


Distribution::Distribution()
{
    m_fileSinkSptr = std::make_shared<FileSink>("fmtlog.txt", true);
}

Distribution::~Distribution()
{
}

void Distribution::setLogFile(const char* filename, bool truncate)
{
}

void Distribution::setLogFile(FILE* fp, bool manageFp)
{
}

// void Distribution::SetLogCB(LogCBFn logCB, LogLevel minCBLogLevel)
// {
//     m_logCB         = logCB;
//     m_minCBLogLevel = minCBLogLevel;
// }

void Distribution::SetFlushDelay(int64_t ns) noexcept
{
}

void Distribution::LogCB(int64_t          ns,
                         LogLevel         level,
                         fmt::string_view location,
                         fmt::string_view funcname,
                         fmt::string_view threadName,
                         fmt::string_view msg,
                         size_t           bodyOffSet)
{
    m_fileSinkSptr->Sink(level, msg);
}