/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-18 16:44:49
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-08 10:30:57
 * FilePath     : \\xlog\\src\\Common.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "fmt/format.h"
#include <functional>
#include <type_traits>


#ifdef _WIN32
    #define FAST_THREAD_LOCAL thread_local  // 首次访问初始化，之后每次访问都要判断
#else
    #define FAST_THREAD_LOCAL __thread  // 手动初始化，之后每次访问不再判断，更快
#endif


enum LogLevel : uint8_t
{
    DBG = 0,
    INF,
    WRN,
    ERR,
    OFF
};

// 用来decode arg
using FormatToFn = const char* (*)(fmt::string_view                                         format,
                                   const char*                                              data,
                                   fmt::basic_memory_buffer<char, 10000>&                   out,
                                   int&                                                     argIdx,
                                   std::vector<fmt::basic_format_arg<fmt::format_context>>& args);


// using LogCBFn = void (*)(int64_t          ns,
//                          LogLevel         level,
//                          fmt::string_view location,
//                          size_t           basePos,
//                          fmt::string_view threadName,
//                          fmt::string_view msg,
//                          size_t           bodyPos /* ,
//                           size_t           logFilePos */
// );

using LogCBFn = std::function<void(int64_t          ns,
                                   LogLevel         level,
                                   fmt::string_view location,
                                   fmt::string_view funcname,
                                   fmt::string_view threadName,
                                   fmt::string_view msg,
                                   size_t           bodyOffSet)>;


using ThreadStopCB = std::function<void(int threadID)>;
