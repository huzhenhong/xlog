/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:46
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 16:11:35
 * FilePath     : \\xlog\\src\\logger.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "fmtlog.h"

#define __FMTLOG_S1(x) #x
#define __FMTLOG_S2(x) __FMTLOG_S1(x)
#define __FMTLOG_LOCATION __FILE__ ":" __FMTLOG_S2(__LINE__)


// 注意这里 logId 是局部静态变量，所以同一条日志只会初始化一次
// 先判断日志级别应该是防止 log 和 Rdtsc 调用的开销
#define FMTLOG(level, format, ...)                                     \
    do                                                                 \
    {                                                                  \
        if (fmtlogWrapper::impl.IsForbidLevel(level))                  \
            break;                                                     \
                                                                       \
        static uint32_t logId = 0;                                     \
                                                                       \
        fmtlogWrapper::impl.Log(logId,                                 \
                                TimeStampCounterWarpper::impl.Rdtsc(), \
                                __FMTLOG_LOCATION,                     \
                                __FUNCTION__,                          \
                                level,                                 \
                                format,                                \
                                ##__VA_ARGS__);                        \
    } while (0)

// #define SET_LOG_LEVEL(level) fmtlogWrapper::impl.setLogLevel(level);
// #define GET_LOG_LEVEL() fmtlogWrapper::impl.getLogLevel();


#define FMTLOG_LIMIT(min_interval, level, format, ...)                                        \
    do                                                                                        \
    {                                                                                         \
        if (fmtlogWrapper::impl.IsForbidLevel(level))                                         \
            break;                                                                            \
                                                                                              \
        static int64_t limitNs = 0;                                                           \
        int64_t        tsc     = TimeStampCounterWarpper::impl.Rdtsc();                       \
        int64_t        ns      = TimeStampCounterWarpper::impl.Tsc2ns(tsc);                   \
        if (ns < limitNs)                                                                     \
            break;                                                                            \
                                                                                              \
        static uint32_t logId = 0;                                                            \
        limitNs               = ns + min_interval;                                            \
        fmtlogWrapper::impl.Log(logId, tsc, __FMTLOG_LOCATION, level, format, ##__VA_ARGS__); \
    } while (0)

#define FMTLOG_ONCE(level, format, ...)                                               \
    do                                                                                \
    {                                                                                 \
        if (fmtlogWrapper::impl.IsForbidLevel(level))                                 \
            break;                                                                    \
                                                                                      \
        fmtlogWrapper::impl.LogOnce(__FMTLOG_LOCATION, level, format, ##__VA_ARGS__); \
    } while (0)


// #define logd(format, ...) FMTLOG(DBG, format, ##__VA_ARGS__)
// #define logi(format, ...) FMTLOG(INF, format, ##__VA_ARGS__)
// #define logw(format, ...) FMTLOG(WRN, format, ##__VA_ARGS__)
// #define loge(format, ...) FMTLOG(ERR, format, ##__VA_ARGS__)


#if FMTLOG_ACTIVE_LEVEL <= FMTLOG_LEVEL_DBG
    #define logd(format, ...) FMTLOG(DBG, format, ##__VA_ARGS__)
    #define logdo(format, ...) FMTLOG_ONCE(DBG, format, ##__VA_ARGS__)
    #define logdl(min_interval, format, ...) FMTLOG_LIMIT(min_interval, DBG, format, ##__VA_ARGS__)
#else
    #define logd(format, ...) (void)0
    #define logdo(format, ...) (void)0
    #define logdl(min_interval, format, ...) (void)0
#endif

#if FMTLOG_ACTIVE_LEVEL <= FMTLOG_LEVEL_INF
    #define logi(format, ...) FMTLOG(INF, format, ##__VA_ARGS__)
    #define logio(format, ...) FMTLOG_ONCE(INF, format, ##__VA_ARGS__)
    #define logil(min_interval, format, ...) FMTLOG_LIMIT(min_interval, INF, format, ##__VA_ARGS__)
#else
    #define logi(format, ...) (void)0
    #define logio(format, ...) (void)0
    #define logil(min_interval, format, ...) (void)0
#endif

#if FMTLOG_ACTIVE_LEVEL <= FMTLOG_LEVEL_WRN
    #define logw(format, ...) FMTLOG(WRN, format, ##__VA_ARGS__)
    #define logwo(format, ...) FMTLOG_ONCE(WRN, format, ##__VA_ARGS__)
    #define logwl(min_interval, format, ...) FMTLOG_LIMIT(min_interval, WRN, format, ##__VA_ARGS__)
#else
    #define logw(format, ...) (void)0
    #define logwo(format, ...) (void)0
    #define logwl(min_interval, format, ...) (void)0
#endif

#if FMTLOG_ACTIVE_LEVEL <= FMTLOG_LEVEL_ERR
    #define loge(format, ...) FMTLOG(ERR, format, ##__VA_ARGS__)
    #define logeo(format, ...) FMTLOG_ONCE(ERR, format, ##__VA_ARGS__)
    #define logel(min_interval, format, ...) FMTLOG_LIMIT(min_interval, ERR, format, ##__VA_ARGS__)
#else
    #define loge(format, ...) (void)0
    #define logeo(format, ...) (void)0
    #define logel(min_interval, format, ...) (void)0
#endif
