/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-17 15:21:58
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-14 14:34:03
 * FilePath     : \\xlog\\src\\StaticLogInfo.h
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
                            const char*      funcName,
                            LogLevel         level,
                            fmt::string_view fmtString)
        : formatToFn(fn)
        , formatString(fmtString)
        , m_pLocation(loc)
        , m_pFuncName(funcName)
        , m_logLevel(level)
        , m_argIdx(-1)
        , m_basePosOffset(0)
        , m_locationLength(0)
    {
    }

    void ProcessLocation()
    {
        // 对长路径进行截断
        size_t      size = strlen(m_pLocation);
        const char* pEnd = m_pLocation + size;  // 指向末尾
        if (size > 255)
        {
            m_pLocation = pEnd - 255;
        }
        m_locationLength = pEnd - m_pLocation;

        // 分割文件名
        const char* pBase = m_pLocation;
        while (pEnd > m_pLocation)
        {
            char c = *--pEnd;
            if (c == '/' || c == '\\')
            {
                pBase = pEnd + 1;
                break;
            }
        }
        m_basePosOffset = pBase - m_pLocation;
    }

    // 获取文件名
    inline fmt::string_view GetBase()
    {
        return fmt::string_view(m_pLocation + m_basePosOffset,
                                m_locationLength - m_basePosOffset);
    }

    // 获取路径
    inline fmt::string_view GetLocation()
    {
        return fmt::string_view(m_pLocation, m_locationLength);
    }

    // 获取函数名
    inline fmt::string_view GetFuncName()
    {
        return fmt::string_view(m_pFuncName);
    }

    FormatToFn       formatToFn;
    fmt::string_view formatString;
    const char*      m_pLocation;
    const char*      m_pFuncName;
    LogLevel         m_logLevel;
    int              m_argIdx;
    uint8_t          m_basePosOffset;
    uint8_t          m_locationLength;
};