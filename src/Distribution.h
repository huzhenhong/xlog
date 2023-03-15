/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-03-15 17:07:40
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 18:05:08
 * FilePath     : \\xlog\\src\\Distribution.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Common.h"
#include "FileSink.h"


class Distribution
{
  public:
    Distribution();
    ~Distribution();

    void setLogFile(const char* filename, bool truncate = false);

    void setLogFile(FILE* fp, bool manageFp);

    void SetFlushDelay(int64_t ns) noexcept;

    void LogCB(int64_t          ns,
               LogLevel         level,
               fmt::string_view location,
               fmt::string_view funcname,
               fmt::string_view threadName,
               fmt::string_view msg,
               size_t           bodyOffSet);

  private:
    std::shared_ptr<ISink> m_fileSinkSptr = nullptr;
};
