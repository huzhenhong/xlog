/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-09 18:18:24
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-07 19:16:52
 * FilePath     : \\xlog\\src\\PatternFormatter.h
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Str.h"
#include "fmt/format.h"
// #include "fmt/core.h"
#include "TSCNS.h"
#include "argutil.h"
#include "SPSCVarQueueOPT.h"
#include "StaticLogInfo.h"
#include <memory>

/*
    Name	Meaning	                                         Example
    a	    Weekday	                                         Mon
    b	    Month name	                                     May
    C	    Short year	                                     21
    Y	    Year	                                         2021
    m	    Month	                                         05
    d	    Day	                                             03
    t	    Thread id by default 	                         main thread
    F	    Nanosecond	                                     796341126
    f	    Microsecond	                                     796341
    e	    Millisecond	                                     796
    S	    Second	                                         09
    M	    Minute	                                         08
    H	    Hour	                                         16
    l	    Log level	                                     INF
    s	    File base name and line num	                     log_test.cc:48
    g	    File path and line num	                         /home/raomeng/fmtlog/log_test.cc:48
    Ymd     Year-Month-Day	                                 2021-05-03
    HMS     Hour:Minute:Second	                             16:08:09
    HMSe	Hour:Minute:Second.Millisecond	                 16:08:09.796
    HMSf	Hour:Minute:Second.Microsecond	                 16:08:09.796341
    HMSF	Hour:Minute:Second.Nanosecond	                 16:08:09.796341126
    YmdHMS	Year-Month-Day Hour:Minute:Second	             2021-05-03 16:08:09
    YmdHMSe	Year-Month-Day Hour:Minute:Second.Millisecond	 2021-05-03 16:08:09.796
    YmdHMSf	Year-Month-Day Hour:Minute:Second.Microsecond	 2021-05-03 16:08:09.796341
    YmdHMSF	Year-Month-Day Hour:Minute:Second.Nanosecond	 2021-05-03 16:08:09.796341126
    func	funciton name                                    main

    Note that using concatenated named args is more efficient than seperated ones, e.g.
    {YmdHMS} is faster than {Y}-{m}-{d} {H}:{M}:{S}
*/

class PatternFormatter
{
  public:
    PatternFormatter(const char* pPattern = "{YmdHMSF} {s:<16} {func} {l} [{t:<6}] ")
    {
        std::cout << "fmtlogDetail()" << std::endl;

        m_patternArgVec.reserve(4096);
        m_patternArgVec.resize(parttenArgSize);

        TimeStampCounterWarpper::impl.Reset();  // ?????????????????????????????????

        ResetDate();

        SetHeaderPattern(pPattern);
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

        const char* pData = (const char*)(pHeader + 1);  // sizeof(SpScVarQueue::MsgHeader) ??? 8???????????????8??????????????????????????????
        const char* pEnd  = (const char*)pHeader + pHeader->size;

        // ??????????????????
        int64_t     tsc = *(int64_t*)pData;
        // int64_t     logTimeNs = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
        logTimeNs       = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
        // the date could go back when polling different threads
        uint64_t tmp    = (logTimeNs > midnightNs) ? (logTimeNs - midnightNs) : 0;  // ????????????????????????????????????????????? 00:00:00???
        m_nanosecond.FromInteger(tmp % 1000'000'000);
        tmp /= 1000'000'000;  // s
        m_second.FromInteger(tmp % 60);
        tmp /= 60;
        m_minute.FromInteger(tmp % 60);
        tmp /= 60;
        uint32_t h = tmp;  // hour
        if (h > 23)        // 24:00:00??????????????????????????????????????????
        {
            h %= 24;
            ResetDate();  // ???????????????
        }
        m_hour.FromInteger(h);

        pData += 8;  // ?????? tsc
        // StaticLogInfo& info = m_allLogInfoVec[pHeader->logId];  // m_allLogInfoVec ??????????????? push ???
        if (!info.formatToFn)
        {
            // log once
            info.m_pLocation = *(const char**)pData;
            pData += 8;  // ?????? location
            info.ProcessLocation();
        }

        SetArgVal<14>(info.GetBase());      // ?????????+??????
        SetArgVal<15>(info.GetLocation());  // ???????????????+??????
        SetArgVal<25>(info.GetFuncName());  // ?????????

        // ????????????
        m_logLevel = (const char*)"DBG INF WRN ERR OFF" + (info.m_logLevel << 2);


        // m_membuf ?????????????????????????????????m_patternArgVec ????????? arg ??????????????? val??????????????? pattern ??????????????????????????????????????????????????????????????? none?????????????????? string ??????
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
    }


  private:
    void SetHeaderPattern(const char* pattern)
    {
        // ????????????delete????????? m_headerPattern ??? UnNameFormat ??? new[] ??????????????????????????????
        if (m_shouldDeallocateHeader)
            delete[] m_headerPattern.data();

        // ???????????????????????????????????????????????????????????????YmdHMSF
        // ?????????pattern??????parttenArgSize???????????????????????????YmdHMSF
        for (int i = 0; i < parttenArgSize; i++)
        {
            m_reorderIdx[i] = parttenArgSize - 1;
        }


        /*
        "a"_a       = ""
        ?????????
        constexpr auto operator"" _a(const char* s, size_t) -> detail::udl_arg<char>
        {
            return {s};
        }
        ?????? udl_arg ??????, ????????????
        template <typename T> auto operator=(T&& value) const -> named_arg<Char, T>
        {
            return {str, std::forward<T>(value)};
        }
        ?????? named_arg ??????
        */

        // ?????????????????????????????????????????????????????? pattern ?????????????????????????????????????????????????????????????????????
        // pattern???{YmdHMSF} {s:<16} {l}[{t:<6}]
        // headerPattern???{} {:<16} {}[{:<6}]
        // ????????????????????????????????????

        // m_reorderIdx?????????????????????????????????????????? pattern ??????????????????
        // ??????????????? {24, 24, 24, 24, 24, 24, 24, 24, 24, ...}????????? parttenArgSize ??????????????????????????????????????????????????? pattern ?????????????????? parttenArgSize ?????????????????????????????????
        // UnNameFormat ?????? {24, 24, 24, 24, 24, 24, 3, 24, 24, 24, 24, 24, 24, 2, 1, 24, 24, 24, 24, 24, 24, 24, 24, 24, 0}
        // t ????????????????????????????????? 6??? ???pattern ?????????????????? 3
        // l ????????????????????????????????? 13??? ???pattern ?????????????????? 2
        // s ????????????????????????????????? 14??? ???pattern ?????????????????? 1
        // YmdHMSF ????????????????????????????????? 24??? ???pattern ?????????????????? 0
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

        // ?????????????????????, ??????"pattern"????????????, ????????????????????????
        m_shouldDeallocateHeader = m_headerPattern.data() != pattern;

        // ????????????????????????????????????????????????setArgVal???????????????????????????????????????????????????????????????
        // ???????????????????????????vformat_to????????????????????????????????????setArgVal????????????????????????
        // ??????????????????
        SetArg<0>(fmt::string_view(m_weekdayName.Ptr(), 3));  // a	    Weekday	                                         Mon
        SetArg<1>(fmt::string_view(m_monthName.Ptr(), 3));    // b	    Month name	                                     May
        SetArg<2>(fmt::string_view(&m_year[2], 2));           // C	    Short year	                                     21
        SetArg<3>(fmt::string_view(m_year.Ptr(), 4));         // Y	    Year	                                         2021
        SetArg<4>(fmt::string_view(m_month.Ptr(), 2));        // m	    Month	                                         05
        SetArg<5>(fmt::string_view(m_day.Ptr(), 2));          // d	    Day	                                             03
        SetArg<6>(fmt::string_view());                        // t	    Thread id by default 	                         main thread
        SetArg<7>(fmt::string_view(m_nanosecond.Ptr(), 9));   // F	    Nanosecond	                                     796341126
        SetArg<8>(fmt::string_view(m_nanosecond.Ptr(), 6));   // f	    Microsecond	                                     796341
        SetArg<9>(fmt::string_view(m_nanosecond.Ptr(), 3));   // e	    Millisecond	                                     796
        SetArg<10>(fmt::string_view(m_second.Ptr(), 2));      // S	    Second	                                         09
        SetArg<11>(fmt::string_view(m_minute.Ptr(), 2));      // M	    Minute	                                         08
        SetArg<12>(fmt::string_view(m_hour.Ptr(), 2));        // H	    Hour	                                         16
        SetArg<13>(fmt::string_view(m_logLevel.Ptr(), 3));    // l	    Log level	                                     INF
        SetArg<14>(fmt::string_view());                       // s	    File base name and line num	                     log_test.cc:48
        SetArg<15>(fmt::string_view());                       // g	    File path and line num	                         /home/raomeng/fmtlog/log_test.cc:48
        SetArg<16>(fmt::string_view(m_year.Ptr(), 10));       // Ymd      Year-Month-Day	                                 2021-05-03
        SetArg<17>(fmt::string_view(m_hour.Ptr(), 8));        // HMS      Hour:Minute:Second	                             16:08:09
        SetArg<18>(fmt::string_view(m_hour.Ptr(), 12));       // HMSe	    Hour:Minute:Second.Millisecond	                 16:08:09.796
        SetArg<19>(fmt::string_view(m_hour.Ptr(), 15));       // HMSf	    Hour:Minute:Second.Microsecond	                 16:08:09.796341
        SetArg<20>(fmt::string_view(m_hour.Ptr(), 18));       // HMSF	    Hour:Minute:Second.Nanosecond	                 16:08:09.796341126
        SetArg<21>(fmt::string_view(m_year.Ptr(), 19));       // YmdHMS	Year-Month-Day Hour:Minute:Second	             2021-05-03 16:08:09
        SetArg<22>(fmt::string_view(m_year.Ptr(), 23));       // YmdHMSe	Year-Month-Day Hour:Minute:Second.Millisecond	 2021-05-03 16:08:09.796
        SetArg<23>(fmt::string_view(m_year.Ptr(), 26));       // YmdHMSf	Year-Month-Day Hour:Minute:Second.Microsecond	 2021-05-03 16:08:09.796341
        SetArg<24>(fmt::string_view(m_year.Ptr(), 29));       // YmdHMSF	Year-Month-Day Hour:Minute:Second.Nanosecond	 2021-05-03 16:08:09.796341126
        SetArg<25>(fmt::string_view());                       // func	    funciton name                                    main
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
        m_year.FromInteger(1900 + timeinfo->tm_year);
        m_month.FromInteger(1 + timeinfo->tm_mon);
        m_day.FromInteger(timeinfo->tm_mday);
        static const char* weekdays[7]    = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        m_weekdayName                     = weekdays[timeinfo->tm_wday];
        static const char* monthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        m_monthName                       = monthNames[timeinfo->tm_mon];
    }

  private:
    int64_t                                                 midnightNs;
    fmt::string_view                                        m_headerPattern;
    std::vector<fmt::basic_format_arg<fmt::format_context>> m_patternArgVec;  // ?????????????????? ?????? ????????????&???
    bool                                                    m_shouldDeallocateHeader = false;
    const static int                                        parttenArgSize           = 25 + 1;
    uint32_t                                                m_reorderIdx[parttenArgSize];

    Str<3>                                                  m_weekdayName;
    Str<3>                                                  m_monthName;
    Str<3>                                                  m_logLevel;

    // ?????????????????????????????????????????????format
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