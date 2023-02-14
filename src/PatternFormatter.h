/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-09 18:18:24
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-14 11:46:23
 * FilePath     : \\xlog\\src\\PatternFormatter.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Str.h"
#include "fmt/format.h"
#include "fmt/core.h"
#include "TSCNS.h"


class PatternFormatter
{
  public:
    PatternFormatter()
    {
        std::cout << "fmtlogDetail()" << std::endl;

        m_patternArgVec.reserve(4096);
        m_patternArgVec.resize(parttenArgSize);

        TimeStampCounterWarpper::impl.Reset();

        ResetDate();
        // fmtlog::setLogFile(stdout);
        SetHeaderPattern("{HMSF} {s:<16} {l}[{t:<6}] ");
        memset(m_membuf.data(), 0, m_membuf.capacity());
    }

    ~PatternFormatter()
    {
    }


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


  private:
    void SetHeaderPattern(const char* pattern)
    {
        if (m_shouldDeallocateHeader)
            delete[] m_headerPattern.data();

        // 每个参数参数对应的位置，先全部初始化为最后YmdHMSF
        // 相当于pattern里有parttenArgSize个参数，每一个都是YmdHMSF
        for (int i = 0; i < parttenArgSize; i++)
        {
            m_reorderIdx[i] = parttenArgSize - 1;
        }


        /*
        "a"_a       = ""
        先调用
        constexpr auto operator"" _a(const char* s, size_t) -> detail::udl_arg<char>
        {
            return {s};
        }
        得到 udl_arg 对象, 再调用其
        template <typename T> auto operator=(T&& value) const -> named_arg<Char, T>
        {
            return {str, std::forward<T>(value)};
        }
        得到 named_arg 对象
        */

        // 下面列出了所有支持的模式
        // 如果有命名参数, 会对"pattern"进行修改, 需要对其进行释放
        // pattern：{HMSf} {s:<16} {l}[{t:<6}]
        // headerPattern：{} {:<16} {}[{:<6}]
        // reorderIdx：{24, 24, 24, 24, 24, 24, 3, 24, 24, ...}
        // 目的就是找到每个参数对应的索引，后面再根据这个索引来设置参数的值
        using namespace fmt::literals;
        m_headerPattern = UnNameFormat<true>(pattern,
                                             m_reorderIdx,
                                             "a"_a       = "",
                                             "b"_a       = "",
                                             "C"_a       = "",
                                             "Y"_a       = "",
                                             "m"_a       = "",
                                             "d"_a       = "",
                                             "t"_a       = "thread name",
                                             "F"_a       = "",
                                             "f"_a       = "",
                                             "e"_a       = "",
                                             "S"_a       = "",
                                             "M"_a       = "",
                                             "H"_a       = "",
                                             "l"_a       = "INF",
                                             //  "l"_a       = LogLevel(),
                                             "s"_a       = "fmtlog.cpp:123",
                                             "g"_a       = "/home/raomeng/fmtlog/fmtlog.cpp:123",
                                             "Ymd"_a     = "",
                                             "HMS"_a     = "",
                                             "HMSe"_a    = "",
                                             "HMSf"_a    = "",
                                             "HMSF"_a    = "",
                                             "YmdHMS"_a  = "",
                                             "YmdHMSe"_a = "",
                                             "YmdHMSf"_a = "",
                                             "YmdHMSF"_a = "");

        m_shouldDeallocateHeader = m_headerPattern.data() != pattern;

        // 确定参数类型，占个坑，因为后面的setArgVal只是设置值，没有改变参数类型和占用内存大小
        // 没有的话会导致后面vformat_to时崩溃，特别是后面也没有setArgVal再手动设置时间戳
        // 虽然可以选择
        setArg<0>(fmt::string_view(weekdayName.str, 3));
        setArg<1>(fmt::string_view(monthName.str, 3));
        setArg<2>(fmt::string_view(&year[2], 2));
        setArg<3>(fmt::string_view(year.str, 4));
        setArg<4>(fmt::string_view(month.str, 2));
        setArg<5>(fmt::string_view(day.str, 2));
        setArg<6>(fmt::string_view());  // 对应线程名，无法确定长度，需要每次手动设置
        setArg<7>(fmt::string_view(nanosecond.str, 9));
        setArg<8>(fmt::string_view(nanosecond.str, 6));
        setArg<9>(fmt::string_view(nanosecond.str, 3));
        setArg<10>(fmt::string_view(second.str, 2));
        setArg<11>(fmt::string_view(minute.str, 2));
        setArg<12>(fmt::string_view(hour.str, 2));
        setArg<13>(fmt::string_view(m_logLevel.str, 3));
        setArg<14>(fmt::string_view());              // 日志位置相对路径，无法确定长度，需要每次手动设置
        setArg<15>(fmt::string_view());              // 日志位置绝对路径，无法确定长度，需要每次手动设置
        setArg<16>(fmt::string_view(year.str, 10));  // Ymd
        setArg<17>(fmt::string_view(hour.str, 8));   // HMS
        setArg<18>(fmt::string_view(hour.str, 12));  // HMSe
        setArg<19>(fmt::string_view(hour.str, 15));  // HMSf
        setArg<20>(fmt::string_view(hour.str, 18));  // HMSF，精确到纳秒
        // setArg<20>(fmt::string_view());              // HMSF，意思就是后面不手动设置的话就是不要
        setArg<21>(fmt::string_view(year.str, 19));  // YmdHMS
        setArg<22>(fmt::string_view(year.str, 23));  // YmdHMSe
        setArg<23>(fmt::string_view(year.str, 26));  // YmdHMSf
        setArg<24>(fmt::string_view(year.str, 29));  // YmdHMSF
    }

    template<size_t I, typename T>
    inline void setArg(const T& arg)
    {
        m_patternArgVec[m_reorderIdx[I]] = fmt::detail::make_arg<fmt::format_context>(arg);
    }

    template<size_t I, typename T>
    inline void setArgVal(const T& arg)
    {
        fmt::detail::value<fmt::format_context>& value_ = *(fmt::detail::value<fmt::format_context>*)&m_patternArgVec[m_reorderIdx[I]];
        value_                                          = fmt::detail::arg_mapper<fmt::format_context>().map(arg);
        int i                                           = 0;
    }

    void ResetDate()
    {
        time_t     rawtime  = TimeStampCounterWarpper::impl.Rdns() / 1000'000'000;  // s
        struct tm* timeinfo = localtime(&rawtime);
        timeinfo->tm_sec = timeinfo->tm_min = timeinfo->tm_hour = 0;
        midnightNs                                              = mktime(timeinfo) * 1000'000'000;
        year.fromi(1900 + timeinfo->tm_year);
        month.fromi(1 + timeinfo->tm_mon);
        day.fromi(timeinfo->tm_mday);
        static const char* weekdays[7]    = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        weekdayName                       = weekdays[timeinfo->tm_wday];
        static const char* monthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        monthName                         = monthNames[timeinfo->tm_mon];
    }

  private:
    int64_t                                                 midnightNs;
    fmt::string_view                                        m_headerPattern;
    bool                                                    m_shouldDeallocateHeader = false;
    fmt::basic_memory_buffer<char, 10000>                   m_membuf;  // 日志写入的地方
    const static int                                        parttenArgSize = 25;
    uint32_t                                                m_reorderIdx[parttenArgSize];

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