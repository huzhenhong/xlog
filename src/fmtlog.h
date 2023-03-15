#pragma once
#include "Common.h"
#include <cstddef>
#include <functional>
#include <vector>
#include <chrono>
#include <memory>
#include "ISink.h"
#include "TSCNS.h"
#include "SPSCVarQueueOPT.h"
#include "argutil.h"
#include "common/ExportMarco.h"
#include "fmtlogDetail.h"
#include "Distribution.h"


// define FMTLOG_BLOCK=1 if log statment should be blocked when queue is full, instead of discarding the msg
#ifndef FMTLOG_BLOCK
    #define FMTLOG_BLOCK false
#endif

#define FMTLOG_LEVEL_DBG 0
#define FMTLOG_LEVEL_INF 1
#define FMTLOG_LEVEL_WRN 2
#define FMTLOG_LEVEL_ERR 3
#define FMTLOG_LEVEL_OFF 4

// define FMTLOG_ACTIVE_LEVEL to turn off low log level in compile time
#ifndef FMTLOG_ACTIVE_LEVEL
    #define FMTLOG_ACTIVE_LEVEL FMTLOG_LEVEL_DBG
#endif


class fmtlog
{
  public:
    fmtlog()
    {
        TimeStampCounterWarpper::impl.Reset();

        m_distributionUptr = std::make_unique<Distribution>();

        fmtlogDetailWrapper::impl.SetLogCB([this](auto&& PH1,
                                                  auto&& PH2,
                                                  auto&& PH3,
                                                  auto&& PH4,
                                                  auto&& PH5,
                                                  auto&& PH6,
                                                  auto&& PH7)
                                           { m_distributionUptr->LogCB(std::forward<decltype(PH1)>(PH1),
                                                                       std::forward<decltype(PH2)>(PH2),
                                                                       std::forward<decltype(PH3)>(PH3),
                                                                       std::forward<decltype(PH4)>(PH4),
                                                                       std::forward<decltype(PH5)>(PH5),
                                                                       std::forward<decltype(PH6)>(PH6),
                                                                       std::forward<decltype(PH7)>(PH7)); },
                                           LogLevel::DBG);

        fmtlogDetailWrapper::impl.startPollingThread(1000'000'000);
    }

    ~fmtlog()
    {
        fmtlogDetailWrapper::impl.stopPollingThread();
        std::cout << "~fmtlog()\n";
    }

    // void startPollingThread(int64_t pollInterval = 1000'000'000) noexcept
    // {
    //     fmtlogDetailWrapper::impl.startPollingThread(pollInterval);
    // }

    // void stopPollingThread() noexcept
    // {
    //     fmtlogDetailWrapper::impl.stopPollingThread();
    // }

    void setLogFile(const char* filename, bool truncate = false)
    {
        m_distributionUptr->setLogFile(filename, truncate);
    }

    void setLogFile(FILE* fp, bool manageFp)
    {
        m_distributionUptr->setLogFile(fp, manageFp);
    }

    void setFlushDelay(int64_t ns) noexcept
    {
        m_distributionUptr->SetFlushDelay(ns);
    }

    void flushOn(LogLevel flushLogLevel) noexcept
    {
        // m_distributionUptr->flushLogLevel = flushLogLevel;
    }


    void setFlushBufSize(uint32_t bytes) noexcept
    {
        // m_distributionUptr->flushBufSize = bytes;
    }

    void closeLogFile() noexcept
    {
        // m_distributionUptr->closeLogFile();
    }

    void poll(bool forceFlush = false)
    {
        fmtlogDetailWrapper::impl.Poll(forceFlush);
    }

    void preallocate() noexcept
    {
        fmtlogDetailWrapper::impl.PreAllocate();
    }

    // void setLogCB(LogCBFn cb, LogLevel minCBLogLevel) noexcept
    // {
    //     fmtlogDetailWrapper::impl.SetLogCB(cb, minCBLogLevel);
    // }

    void setHeaderPattern(const char* pPattern)
    {
        fmtlogDetailWrapper::impl.SetHeaderPattern(pPattern);
    }

    void setThreadName(const char* name) noexcept
    {
        // preallocate();

        // m_pThreadBuffer->nameSize =
        //     fmt::format_to_n(m_pThreadBuffer->name, sizeof(fmtlog::m_pThreadBuffer->name), "{}", name).size;
    }

    inline void setLogLevel(LogLevel logLevel) noexcept
    {
        m_level = logLevel;
    }

    inline LogLevel getLogLevel() noexcept
    {
        return m_level;
    }

    inline bool IsForbidLevel(LogLevel logLevel) noexcept
    {
#ifdef FMTLOG_NO_CHECK_LEVEL
        return false;
#else
        return logLevel < m_level;
#endif
    }


    template<typename... Args>
    inline void Log(uint32_t&                                        logId,
                    int64_t                                          tsc,
                    const char*                                      location,
                    const char*                                      funcName,
                    LogLevel                                         level,   // 引用反而慢
                    fmt::format_string<fmt::remove_cvref_t<Args>...> format,  // fmt::remove_cvref_t<Args>... 为去除const 以及引用
                    Args&&... args) noexcept
    {
        if (level < m_level) return;

        if (!logId)  // logId 是局部静态变量的引用，任何一条日志首次运行到这里时 logId 均为 0，也就是一定会执行下面的注册流程
        {
            // 去除命名参数，不对命名参数重排序
            auto unnamedFmtStr = UnNameFormat<false>(fmt::string_view(format), nullptr, args...);

            // FormatTo<Args...> 是利用 Args... 作为模版参数先实例化一个函数，函数里面做 DecodeArgs 的时候会用到 Args...
            fmtlogDetailWrapper::impl.RegisterLogInfo(logId, Format<Args...>, location, funcName, level, unnamedFmtStr);
        }


        // cstring 需要手动释放内存，因为归根结底就是个指针
        constexpr size_t num_cstring = fmt::detail::count<IsCstring<Args>()...>();  // 计算有多少个 cstring 类型的参数
        size_t           cstringSizes[std::max(num_cstring, (size_t)1)];            // 需要保证 cstringSizes 不为空，这块代码逻辑应该还有优化空间

        // 从第一个参数开始判断，cstringSizes 记录所有 cstring 类型参数的长度
        uint32_t         alloc_size = 8 + (uint32_t)GetArgSize<0>(cstringSizes, args...);  // 这里的 8 是用来放时间戳的，GetArgSize 计算所有参数需要的内存
        do
        {
            if (auto header = fmtlogDetailWrapper::impl.AllocMsg(alloc_size))
            {
                // 第一个 blank 记录着 logId 和 msg 大小
                header->logId = logId;

                // 写入时戳
                char* pOut      = (char*)(header + 1);
                *(int64_t*)pOut = tsc;
                pOut += 8;

                // 写入参数
                EncodeArgs<0>(cstringSizes, pOut, std::forward<Args>(args)...);

                // 移动写入指针位置
                header->Push(alloc_size);

                break;
            }
        } while (FMTLOG_BLOCK);  // 是否阻塞当前线程直到 push 成功
    }


    // logOnce 就是没有了 id, 不会重复利用之前的资源
    template<typename... Args>
    inline void LogOnce(const char*                 location,
                        LogLevel                    level,
                        fmt::format_string<Args...> format,
                        Args&&... args)
    {
        fmt::string_view sv(format);
        auto&&           fmt_args   = fmt::make_format_args(args...);
        uint32_t         fmt_size   = FormattedSize(sv, fmt_args);
        uint32_t         alloc_size = 8 + 8 + fmt_size;  // 时戳 + 位置 + 格式化数据
        do
        {
            if (auto header = fmtlogDetailWrapper::impl.AllocMsg(alloc_size))
            {
                header->logId = (uint32_t)level;
                char* out     = (char*)(header + 1);  // 跳过消息头

                *(int64_t*)out = TimeStampCounterWarpper::impl.Rdtsc();
                out += 8;
                *(const char**)out = location;
                out += 8;
                VformatTo(out, sv, fmt_args);
                header->Push(alloc_size);
                break;
            }
        } while (FMTLOG_BLOCK);
    }

  private:
    volatile LogLevel             m_level = INF;

    std::unique_ptr<Distribution> m_distributionUptr = nullptr;
};


struct FMTLOG_API fmtlogWrapper
{
    // C++11 后定义static变量多线程安全, 本质上来说应该是原子操作
    static fmtlog impl;  // 只是内部声明(并未分配内存), 所以需要外部再次定义(分配内存)
};
