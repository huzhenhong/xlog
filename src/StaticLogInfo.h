/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-17 15:21:58
 * LastEditors  : huzhenhong
 * LastEditTime : 2022-10-18 16:47:48
 * FilePath     : \\FmtLog\\src\\StaticLogInfo.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
// #include "fmtlog.h"
#include "Common.h"


struct StaticLogInfo
{
    // Constructor
    constexpr StaticLogInfo(FormatToFn       fn,
                            const char*      loc,
                            LogLevel         level,
                            fmt::string_view fmtString)
        : formatToFn(fn)
        , formatString(fmtString)
        , location(loc)
        , logLevel(level)
        , argIdx(-1)
        , basePos(0)
        , endPos(0)
    {
    }

    void processLocation()
    {
        // 对长路径进行截断
        size_t      size = strlen(location);
        const char* p    = location + size;
        if (size > 255)
        {
            location = p - 255;
        }
        endPos           = p - location;
        const char* base = location;
        while (p > location)
        {
            char c = *--p;
            if (c == '/' || c == '\\')
            {
                base = p + 1;
                break;
            }
        }
        basePos = base - location;
    }

    // 获取文件名
    inline fmt::string_view getBase()
    {
        return fmt::string_view(location + basePos, endPos - basePos);
    }

    // 获取路径
    inline fmt::string_view getLocation()
    {
        return fmt::string_view(location, endPos);
    }

    FormatToFn       formatToFn;
    fmt::string_view formatString;
    const char*      location;
    LogLevel         logLevel;
    int              argIdx;
    uint8_t          basePos;
    uint8_t          endPos;
};