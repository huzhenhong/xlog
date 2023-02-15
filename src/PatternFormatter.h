/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-09 18:18:24
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-16 00:27:01
 * FilePath     : \\xlog\\src\\PatternFormatter.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Str.h"
#include "fmt/format.h"
#include "fmt/core.h"
#include "TSCNS.h"
#include "argutil.h"
#include "SPSCVarQueueOPT.h"
#include "StaticLogInfo.h"
#include <memory>


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
        /*
            Name	Meaning	                                                        Example
            l	    Log level	                                                    INF
            s	    File base name and line num	                                    log_test.cc:48
            g	    File path and line num	                                        /home/raomeng/fmtlog/log_test.cc:48
            t	    Thread id by default, can be reset by fmt::setThreadName()	    main thread
            a	    Weekday	                                                        Mon
            b	    Month name	                                                    May
            Y	    Year	                                                        2021
            C	    Short year	                                                    21
            m	    Month	                                                        05
            d	    Day	                                                            03
            H	    Hour	                                                        16
            M	    Minute	                                                        08
            S	    Second	                                                        09
            e	    Millisecond	                                                    796
            f	    Microsecond	                                                    796341
            F	    Nanosecond	                                                    796341126
            Ymd	    Year-Month-Day	                                                2021-05-03
            HMS	    Hour:Minute:Second	                                            16:08:09
            HMSe	Hour:Minute:Second.Millisecond	                                16:08:09.796
            HMSf	Hour:Minute:Second.Microsecond	                                16:08:09.796341
            HMSF	Hour:Minute:Second.Nanosecond	                                16:08:09.796341126
            YmdHMS	Year-Month-Day Hour:Minute:Second	                            2021-05-03 16:08:09
            YmdHMSe	Year-Month-Day Hour:Minute:Second.Millisecond	                2021-05-03 16:08:09.796
            YmdHMSf	Year-Month-Day Hour:Minute:Second.Microsecond	                2021-05-03 16:08:09.796341
            YmdHMSF	Year-Month-Day Hour:Minute:Second.Nanosecond	                2021-05-03 16:08:09.796341126
            func    function name                                                   main
            Note that using concatenated named args is more efficient than seperated ones, e.g.
            {YmdHMS} is faster than {Y}-{m}-{d} {H}:{M}:{S}
        */
        SetHeaderPattern("{YmdHMSF} {s:<16} {func} {l} [{t:<6}] ");
        // memset(m_membuf.data(), 0, m_membuf.capacity());
    }

    ~PatternFormatter()
    {
    }


    void Format(fmt::string_view                       threadName,
                const SpScVarQueue::MsgHeader*         pHeader,
                /* const  */ StaticLogInfo&            info,
                fmt::basic_memory_buffer<char, 10000>& membuf,
                int64_t&                               logTimeNs,
                size_t&                                bodyPos)
    {
        SetArgVal<6>(threadName);

        const char* pData = (const char*)(pHeader + 1);  // sizeof(SpScVarQueue::MsgHeader) 为 8，最前面的8个字节存放消息头信息
        const char* pEnd  = (const char*)pHeader + pHeader->size;

        // 格式化时间戳
        int64_t     tsc = *(int64_t*)pData;
        // int64_t     logTimeNs = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
        logTimeNs       = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
        // the date could go back when polling different threads
        uint64_t tmp    = (logTimeNs > midnightNs) ? (logTimeNs - midnightNs) : 0;  // 貌似有问题，跳变了就直接记录为 00:00:00？
        m_nanosecond.fromi(tmp % 1000'000'000);
        tmp /= 1000'000'000;  // s
        m_second.fromi(tmp % 60);
        tmp /= 60;
        m_minute.fromi(tmp % 60);
        tmp /= 60;
        uint32_t h = tmp;  // hour
        if (h > 23)        // 24:00:00，两条日志间隔超过有可能很大
        {
            h %= 24;
            ResetDate();  // 重置年月日
        }
        m_hour.fromi(h);

        pData += 8;  // 跳过 tsc
        // StaticLogInfo& info = m_allLogInfoVec[pHeader->logId];  // m_allLogInfoVec 都是按顺序 push 的
        if (!info.formatToFn)
        {
            // log once
            info.m_pLocation = *(const char**)pData;
            pData += 8;  // 跳过 location
            info.ProcessLocation();
        }

        SetArgVal<14>(info.GetBase());      // 文件名+行号
        SetArgVal<15>(info.GetLocation());  // 绝对路径名+行号
        SetArgVal<25>(info.GetFuncName());  // 函数名

        // 日志级别
        m_logLevel = (const char*)"DBG INF WRN ERR OFF" + (info.m_logLevel << 2);


        // m_membuf里追加写入固定日志头，m_patternArgVec 里面的 arg 已经填充了 val
        vformat_to(membuf, m_headerPattern, fmt::basic_format_args(m_patternArgVec.data(), parttenArgSize));
        bodyPos = membuf.size();

        // boby process
        if (info.formatToFn)
        {
            // decode args
            info.formatToFn(info.formatString, pData, membuf, info.m_argIdx, m_patternArgVec);
        }
        else
        {
            // log once
            membuf.append(fmt::string_view(pData, pEnd - pData));
        }

        // msg = fmt::string_view(m_membuf.data() + headerPos, m_membuf.size() - headerPos);

        // // 可以将单条日志推送上去
        // if (logCB && info.m_logLevel >= m_minCBLogLevel)
        // {
        //     logCB(curTimeNs,
        //           info.m_logLevel,
        //           info.GetLocation(),
        //           info.m_basePosOffset,
        //           threadName,
        //           fmt::string_view(m_membuf.data() + headerPos, m_membuf.size() - headerPos),
        //           bodyPos - headerPos /* ,
        //            m_fpos + headerPos */
        //     );
        // }
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

        // 目的就是找到每个罗列的命名参数对应在 pattern 中的位置索引，后面再根据这个索引来设置参数的值
        // pattern：{YmdHMSF} {s:<16} {l}[{t:<6}]
        // headerPattern：{} {:<16} {}[{:<6}]
        // 下面列出了所有支持的模式

        // m_reorderIdx：当前罗列的这个参数，对应在 pattern 中的哪个位置
        // 初始化后是 {24, 24, 24, 24, 24, 24, 24, 24, 24, ...}，一共 parttenArgSize 个，那这个地方就有问题了，意思就是 pattern 中最多只能有 parttenArgSize 个需要格式化输出的条目
        // UnNameFormat 之后 {24, 24, 24, 24, 24, 24, 3, 24, 24, 24, 24, 24, 24, 2, 1, 24, 24, 24, 24, 24, 24, 24, 24, 24, 0}
        // t 在罗列的参数里的索引是 6， 在pattern 中的位置是第 3
        // l 在罗列的参数里的索引是 13， 在pattern 中的位置是第 2
        // s 在罗列的参数里的索引是 14， 在pattern 中的位置是第 1
        // YmdHMSF 在罗列的参数里的索引是 24， 在pattern 中的位置是第 0
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
                                             "YmdHMSF"_a = "",
                                             "func"_a    = "");

        // 如果有命名参数, 会对"pattern"进行修改, 需要对其进行释放
        m_shouldDeallocateHeader = m_headerPattern.data() != pattern;

        // 确定参数类型，占个坑，因为后面的setArgVal只是设置值，没有改变参数类型和占用内存大小
        // 没有的话会导致后面vformat_to时崩溃，特别是后面也没有setArgVal再手动设置时间戳
        // 虽然可以选择
        SetArg<0>(fmt::string_view(m_weekdayName.str, 3));  // a	    Weekday	                                         Mon
        SetArg<1>(fmt::string_view(m_monthName.str, 3));    // b	    Month name	                                     May
        SetArg<2>(fmt::string_view(&m_year[2], 2));         // C	    Short year	                                     21
        SetArg<3>(fmt::string_view(m_year.str, 4));         // Y	    Year	                                         2021
        SetArg<4>(fmt::string_view(m_month.str, 2));        // m	    Month	                                         05
        SetArg<5>(fmt::string_view(m_day.str, 2));          // d	    Day	                                             03
        SetArg<6>(fmt::string_view());                      // t	    Thread id by default 	                         main thread
        SetArg<7>(fmt::string_view(m_nanosecond.str, 9));   // F	    Nanosecond	                                     796341126
        SetArg<8>(fmt::string_view(m_nanosecond.str, 6));   // f	    Microsecond	                                     796341
        SetArg<9>(fmt::string_view(m_nanosecond.str, 3));   // e	    Millisecond	                                     796
        SetArg<10>(fmt::string_view(m_second.str, 2));      // S	    Second	                                         09
        SetArg<11>(fmt::string_view(m_minute.str, 2));      // M	    Minute	                                         08
        SetArg<12>(fmt::string_view(m_hour.str, 2));        // H	    Hour	                                         16
        SetArg<13>(fmt::string_view(m_logLevel.str, 3));    // l	    Log level	                                     INF
        SetArg<14>(fmt::string_view());                     // s	    File base name and line num	                     log_test.cc:48
        SetArg<15>(fmt::string_view());                     // g	    File path and line num	                         /home/raomeng/fmtlog/log_test.cc:48
        SetArg<16>(fmt::string_view(m_year.str, 10));       // Ymd      Year-Month-Day	                                 2021-05-03
        SetArg<17>(fmt::string_view(m_hour.str, 8));        // HMS      Hour:Minute:Second	                             16:08:09
        SetArg<18>(fmt::string_view(m_hour.str, 12));       // HMSe	    Hour:Minute:Second.Millisecond	                 16:08:09.796
        SetArg<19>(fmt::string_view(m_hour.str, 15));       // HMSf	    Hour:Minute:Second.Microsecond	                 16:08:09.796341
        SetArg<20>(fmt::string_view(m_hour.str, 18));       // HMSF	    Hour:Minute:Second.Nanosecond	                 16:08:09.796341126
        SetArg<21>(fmt::string_view(m_year.str, 19));       // YmdHMS	Year-Month-Day Hour:Minute:Second	             2021-05-03 16:08:09
        SetArg<22>(fmt::string_view(m_year.str, 23));       // YmdHMSe	Year-Month-Day Hour:Minute:Second.Millisecond	 2021-05-03 16:08:09.796
        SetArg<23>(fmt::string_view(m_year.str, 26));       // YmdHMSf	Year-Month-Day Hour:Minute:Second.Microsecond	 2021-05-03 16:08:09.796341
        SetArg<24>(fmt::string_view(m_year.str, 29));       // YmdHMSF	Year-Month-Day Hour:Minute:Second.Nanosecond	 2021-05-03 16:08:09.796341126
        SetArg<25>(fmt::string_view());                     // func	    funciton name                                    main
    }

    template<size_t I, typename T>
    inline void SetArg(const T& arg)
    {
        m_patternArgVec[m_reorderIdx[I]] = fmt::detail::make_arg<fmt::format_context>(arg);
    }

    template<size_t I, typename T>
    inline void SetArgVal(const T& arg)
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
        m_year.fromi(1900 + timeinfo->tm_year);
        m_month.fromi(1 + timeinfo->tm_mon);
        m_day.fromi(timeinfo->tm_mday);
        static const char* weekdays[7]    = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        m_weekdayName                     = weekdays[timeinfo->tm_wday];
        static const char* monthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        m_monthName                       = monthNames[timeinfo->tm_mon];
    }

  private:
    int64_t                                                 midnightNs;
    fmt::string_view                                        m_headerPattern;
    std::vector<fmt::basic_format_arg<fmt::format_context>> m_patternArgVec;  // 命名参数索引 —— 参数类型&值
    bool                                                    m_shouldDeallocateHeader = false;
    const static int                                        parttenArgSize           = 25 + 1;
    uint32_t                                                m_reorderIdx[parttenArgSize];

    Str<3>                                                  m_weekdayName;
    Str<3>                                                  m_monthName;
    Str<3>                                                  m_logLevel;

    // 这块必须是连续的，根据偏移量来format
    Str<4>                                                  m_year;
    char                                                    m_dash1 = '-';
    Str<2>                                                  m_month;
    char                                                    m_dash2 = '-';
    Str<2>                                                  m_day;
    char                                                    m_space = ' ';
    Str<2>                                                  m_hour;
    char                                                    m_colon1 = ':';
    Str<2>                                                  m_minute;
    char                                                    m_colon2 = ':';
    Str<2>                                                  m_second;
    char                                                    m_dot1 = '.';
    Str<9>                                                  m_nanosecond;
};


using PatternFormatterSptr = std::shared_ptr<PatternFormatter>;