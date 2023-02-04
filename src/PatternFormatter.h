/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-09 18:18:24
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-01-13 13:41:33
 * FilePath     : \\FmtLog\\src\\PatternFormatter.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Str.h"
#include "fmt/format.h"


class PatternFormatter
{
  public:
    void Format(fmt::string_view               threadName,
                const SpScVarQueue::MsgHeader* pHeader)
    {
        setArgVal<6>(threadName);

        const char* pData = (const char*)(pHeader + 1);  // sizeof(SpScVarQueue::MsgHeader) 为 8，最前面的8个字节存放消息头信息
        const char* pEnd  = (const char*)pHeader + pHeader->size;

        // 格式化时间戳
        int64_t     tsc       = *(int64_t*)pData;
        int64_t     curTimeNs = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
        // the date could go back when polling different threads
        uint64_t    tmp       = (curTimeNs > midnightNs) ? (curTimeNs - midnightNs) : 0;
        nanosecond.fromi(tmp % 1000'000'000);
        tmp /= 1000'000'000;  // s
        second.fromi(tmp % 60);
        tmp /= 60;
        minute.fromi(tmp % 60);
        tmp /= 60;
        uint32_t h = tmp;  // hour
        if (h > 23)        // 24:00:00，两条日志间隔超过有可能很大
        {
            h %= 24;
            resetDate();  // 重置年月日
        }
        hour.fromi(h);

        pData += 8;  // 跳过 tsc
        StaticLogInfo& info = m_allLogInfoVec[pHeader->logId];
        if (!info.formatToFn)
        {
            // log once
            info.location = *(const char**)pData;
            pData += 8;  // 跳过 location
            info.processLocation();
        }

        setArgVal<14>(info.getBase());      // 函数名
        setArgVal<15>(info.getLocation());  // 调用代码位置

        // 日志级别
        m_logLevel = (const char*)"DBG INF WRN ERR OFF" + (info.logLevel << 2);

        size_t headerPos = m_membuf.size();

        // m_membuf里追加写入固定日志头，m_patternArgVec 里面的 arg 已经填充了 val
        vformat_to(m_membuf, m_headerPattern, fmt::basic_format_args(m_patternArgVec.data(), parttenArgSize));
        size_t bodyPos = m_membuf.size();

        if (info.formatToFn)
        {
            // decode args
            info.formatToFn(info.formatString, pData, m_membuf, info.argIdx, m_patternArgVec);
        }
        else
        {
            // log once
            m_membuf.append(fmt::string_view(pData, pEnd - pData));
        }
    }

    Str<3>                                                  weekdayName;
    Str<3>                                                  monthName;
    Str<3>                                                  m_logLevel;

    // 这块必须是连续的，根据偏移量来format
    Str<4>                                                  year;
    char                                                    dash1 = '-';
    Str<2>                                                  month;
    char                                                    dash2 = '-';
    Str<2>                                                  day;
    char                                                    space = ' ';
    Str<2>                                                  hour;
    char                                                    colon1 = ':';
    Str<2>                                                  minute;
    char                                                    colon2 = ':';
    Str<2>                                                  second;
    char                                                    dot1 = '.';
    Str<9>                                                  nanosecond;

    std::vector<fmt::basic_format_arg<fmt::format_context>> m_patternArgVec;  // 参数索引-参数类型&值
};