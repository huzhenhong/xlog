/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-06 18:27:57
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-02 15:00:43
 * FilePath     : \\xlog\\bench\\bench.cpp
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/

// #include <bits/stdc++.h>
#include <chrono>
#include "logger.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"
// #include "NanoLog/runtime/NanoLogCpp17.h"

/* namespace GeneratedFunctions
{
    size_t numLogIds;
}; */

struct FmtLogBase
{
    void flush()
    {
        fmtlog::poll(true);
    }
};

struct StaticString : public FmtLogBase
{
    inline void log()
    {
        logi("Starting backup replica garbage collector thread");
    }
};

struct StringConcat : public FmtLogBase
{
    inline void log()
    {
        logi("Opened session with coordinator at {}", "basic+udp:host=192.168.1.140,port=12246");
    }
};

struct SingleInteger : public FmtLogBase
{
    inline void log()
    {
        logi("Backup storage speeds (min): {} MB/s read", 181);
    }
};

struct TwoIntegers : public FmtLogBase
{
    inline void log()
    {
        logi("buffer has consumed {} bytes of extra storage, current allocation: {} bytes", 1032024, 1016544);
    }
};

struct SingleDouble : public FmtLogBase
{
    inline void log()
    {
        logi("Using tombstone ratio balancer with ratio = {}", 0.400000);
    }
};

struct ComplexFormat : public FmtLogBase
{
    inline void log()
    {
        logi("Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
             50000,
             97,
             50,
             0,
             26.2);
    }
};

// struct NanoLogBase
// {
//     void flush()
//     {
//         NanoLog::sync();
//     }
// };

// struct NanoLogStaticString : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE, "Starting backup replica garbage collector thread");
//     }
// };

// struct NanoLogStringConcat : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE, "Opened session with coordinator at %s", "basic+udp:host=192.168.1.140,port=12246");
//     }
// };

// struct NanoLogSingleInteger : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE, "Backup storage speeds (min): %d MB/s read", 181);
//     }
// };

// struct NanoLogTwoIntegers : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE, "buffer has consumed %u bytes of extra storage, current allocation: %u bytes", 1032024, 1016544);
//     }
// };

// struct NanoLogSingleDouble : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE, "Using tombstone ratio balancer with ratio = %0.6lf", 0.400000);
//     }
// };

// struct NanoLogComplexFormat : public NanoLogBase
// {
//     inline void log()
//     {
//         NANO_LOG(NOTICE,
//                  "Initialized InfUdDriver buffers: %u receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms",
//                  50000,
//                  97,
//                  50,
//                  0,
//                  26.2);
//     }
// };

struct SpdlogBase
{
    SpdlogBase(spdlog::logger* logger)
        : pLogger(logger) {}

    void flush()
    {
        pLogger->flush();
    }

    spdlog::logger* pLogger;
};

struct SpdlogStaticString : public SpdlogBase
{
    SpdlogStaticString(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(pLogger, "Starting backup replica garbage collector thread");
    }
};

struct SpdlogStringConcat : public SpdlogBase
{
    SpdlogStringConcat(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(pLogger, "Opened session with coordinator at {}", "basic+udp:host=192.168.1.140,port=12246");
    }
};

struct SpdlogSingleInteger : public SpdlogBase
{
    SpdlogSingleInteger(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(pLogger, "Backup storage speeds (min): {} MB/s read", 181);
    }
};

struct SpdlogTwoIntegers : public SpdlogBase
{
    SpdlogTwoIntegers(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(pLogger, "buffer has consumed {} bytes of extra storage, current allocation: {} bytes", 1032024, 1016544);
    }
};

struct SpdlogSingleDouble : public SpdlogBase
{
    SpdlogSingleDouble(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(pLogger, "Using tombstone ratio balancer with ratio = {}", 0.400000);
    }
};

struct SpdlogComplexFormat : public SpdlogBase
{
    SpdlogComplexFormat(spdlog::logger* logger)
        : SpdlogBase(logger) {}
    inline void log()
    {
        SPDLOG_LOGGER_INFO(
            pLogger,
            "Initialized InfUdDriver buffers: {} receive buffers ({} MB), {} transmit buffers ({} MB), took {:.1f} ms",
            50000,
            97,
            50,
            0,
            26.2);
    }
};

template<typename T>
void Bench(T obj)
{
    const int                                      RECORDS = 10000;
    std::chrono::high_resolution_clock::time_point t0, t1, t2;
    t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < RECORDS; ++i)
    {
        obj.log();
    }

    t1 = std::chrono::high_resolution_clock::now();

    obj.flush();

    t2 = std::chrono::high_resolution_clock::now();

    double span1 = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();  // 单位是s
    double span2 = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t0).count();  // 单位是s

    fmt::print("{}: front-end latency is {:.1f} ns/msg average, throughput  is {:.2f} million msgs/sec average\n",
               typeid(obj).name(),  // typeid操作符的返回结果是名为type_info的标准库类型的对象的引用
               (span1 / RECORDS) * 1e9,
               RECORDS / span2 / 1e6);  // 为了计算百万条日制耗时
}

int main()
{
    fmtlog::setLogFile("fmtlog.txt", true);
    fmtlog::setHeaderPattern("[{YmdHMSe}] [fmtlog] [{l}] [{s}] ");
    fmtlog::preallocate();

    // NanoLog::setLogFile("nanalog.bin");
    // NanoLog::preallocate();

    auto spdlogger = spdlog::basic_logger_st("spdlog", "spdlog.txt", true);

    Bench(StaticString());
    Bench(StringConcat());
    Bench(SingleInteger());
    Bench(TwoIntegers());
    Bench(SingleDouble());
    Bench(ComplexFormat());

    fmt::print("\n");

    // bench(NanoLogStaticString());
    // bench(NanoLogStringConcat());
    // bench(NanoLogSingleInteger());
    // bench(NanoLogTwoIntegers());
    // bench(NanoLogSingleDouble());
    // bench(NanoLogComplexFormat());

    // fmt::print("\n");

    Bench(SpdlogStaticString(spdlogger.get()));
    Bench(SpdlogStringConcat(spdlogger.get()));
    Bench(SpdlogSingleInteger(spdlogger.get()));
    Bench(SpdlogTwoIntegers(spdlogger.get()));
    Bench(SpdlogSingleDouble(spdlogger.get()));
    Bench(SpdlogComplexFormat(spdlogger.get()));

    return 0;
}
