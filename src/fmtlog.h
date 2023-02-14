#pragma once
// #include "fmt/format.h"
// #include <type_traits>
#include "Common.h"
#include <vector>
#include <chrono>
#include <memory>
#include "ISink.h"
#include "TSCNS.h"
#include "SPSCVarQueueOPT.h"
#include "argutil.h"
#include <iostream>
#include "common/ExportMarco.h"
// #include "FileSink.h"


// define FMTLOG_BLOCK=1 if log statment should be blocked when queue is full, instead of discarding the msg
#ifndef FMTLOG_BLOCK
    #define FMTLOG_BLOCK 0
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
    static fmtlog& Instance()
    {
        static fmtlog ins;
        return ins;
    }

    fmtlog();

    ~fmtlog();

    static void                     setTscGhz(double tscGhz) noexcept;

    // Preallocate thread queue for current thread
    static void                     preallocate() noexcept;

    // Set the file for logging
    static void                     setLogFile(const char* filename, bool truncate = false);

    // Set an existing FILE* for logging, if manageFp is false fmtlog will not buffer log internally
    // and will not close the FILE*
    // static void                     setLogFile(FILE* fp, bool manageFp = false);

    // Collect log msgs from all threads and write to log file
    // If forceFlush = true, internal file buffer is flushed
    // User need to call poll() repeatedly if startPollingThread is not used
    static void                     poll(bool forceFlush = false);

    // Set flush delay in nanosecond
    // If there's msg older than ns in the buffer, flush will be triggered
    static void                     setFlushDelay(int64_t ns) noexcept;

    // If current msg has level >= flushLogLevel, flush will be triggered
    static void                     flushOn(LogLevel flushLogLevel) noexcept;

    // If file buffer has more than specified bytes, flush will be triggered
    static void                     setFlushBufSize(uint32_t bytes) noexcept;

    // callback signature user can register
    // ns: nanosecond timestamp
    // level: logLevel
    // location: full file path with line num, e.g: /home/raomeng/fmtlog/fmtlog.h:45
    // basePos: file base index in the location
    // threadName: thread id or the name user set with setThreadName
    // msg: full log msg with header
    // bodyPos: log body index in the msg
    // logFilePos: log file position of this msg

    // using LogCBFn = void (*)(int64_t          ns,
    //                          LogLevel         level,
    //                          fmt::string_view location,
    //                          size_t           basePos,
    //                          fmt::string_view threadName,
    //                          fmt::string_view msg,
    //                          size_t           bodyPos,
    //                          size_t           logFilePos);

    // Set a callback function for all log msgs with a mininum log level
    static void                     setLogCB(LogCBFn cb, LogLevel minCBLogLevel) noexcept;

    // Close the log file and subsequent msgs will not be written into the file,
    // but callback function can still be used
    static void                     closeLogFile() noexcept;

    // Set log header pattern with fmt named arguments
    static void                     setHeaderPattern(const char* pattern);

    // Set a name for current thread, it'll be shown in {t} part in header pattern
    static void                     setThreadName(const char* name) noexcept;

    // Set current log level, lower level log msgs will be discarded
    static inline void              setLogLevel(LogLevel logLevel) noexcept;

    // Get current log level
    static inline LogLevel          getLogLevel() noexcept;


    static inline bool              checkLogLevel(LogLevel logLevel) noexcept;

    // Run a polling thread in the background with a polling interval  in ns
    // Note that user must not call poll() himself when the thread is running
    static void                     startPollingThread(int64_t pollInterval = 1000'000'000) noexcept;

    // Stop the polling thread
    static void                     stopPollingThread() noexcept;

    // // 用来decode arg
    // using FormatToFn = const char* (*)(fmt::string_view                                         format,
    //                                    const char*                                              data,
    //                                    fmt::basic_memory_buffer<char, 10000>&                   out,
    //                                    int&                                                     argIdx,
    //                                    std::vector<fmt::basic_format_arg<fmt::format_context>>& args);

    static void                     registerLogInfo(uint32_t&        logId,
                                                    FormatToFn       fn,
                                                    const char*      location,
                                                    const char*      funcName,
                                                    LogLevel         level,
                                                    fmt::string_view fmtString) noexcept;

    static SpScVarQueue::MsgHeader* AllocMsg(uint32_t size) noexcept;

    template<typename... Args>
    inline void log(uint32_t&                                        logId,
                    int64_t                                          tsc,
                    const char*                                      location,
                    const char*                                      funcName,
                    LogLevel                                         level,
                    fmt::format_string<fmt::remove_cvref_t<Args>...> format,  // fmt::remove_cvref_t<Args>... 为去除const 以及引用
                    Args&&... args) noexcept
    {
        // #ifdef FMTLOG_NO_CHECK_LEVEL
        //         return;
        // #else
        //         if (level < currentLogLevel)
        //         {
        //             return;
        //         }
        // #endif

        if (!logId)  // logId 是局部静态变量的引用，任何一条日志首次运行到这里时 logId 均为 0，也就是一定会执行下面的注册流程
        {
            // 去除命名参数
            auto unnamed_format = UnNameFormat<false>(fmt::string_view(format), nullptr, args...);

            // FormatTo<Args...> 是利用 Args... 作为模版参数先实例化一个函数，函数里面做 DecodeArgs 的时候会用到 Args...
            registerLogInfo(logId, FormatTo<Args...>, location, funcName, level, unnamed_format);
        }

        // cstring 需要手动释放内存，因为归根结底就是个指针
        constexpr size_t num_cstring = fmt::detail::count<IsCstring<Args>()...>();  // 计算有多少个 cstring 类型的参数
        size_t           cstringSizes[std::max(num_cstring, (size_t)1)];            // 需要保证 cstringSizes 不为空，这块代码逻辑应该还有优化空间

        // 从第一个参数开始判断，cstringSizes 记录所有 cstring 类型参数的长度
        uint32_t         alloc_size = 8 + (uint32_t)GetArgSize<0>(cstringSizes, args...);  // 这里的 8 是用来放时间戳的，GetArgSize 计算所有参数需要的内存
        do
        {
            // 应该直接把 logId 也传进去，header在内部就完成初始化
            if (auto header = AllocMsg(alloc_size))  // 申请内存起始为 msg 数据头，指明 logId 和 msg 大小
            {
                header->logId   = logId;
                char* pOut      = (char*)(header + 1);  // 第一个 blank 记录着 logId 和 msg 大小
                *(int64_t*)pOut = tsc;
                pOut += 8;
                EncodeArgs<0>(cstringSizes, pOut, std::forward<Args>(args)...);
                header->push(alloc_size);  // 可以直接在 AllocMsg 里面去初始化size
                break;
            }
        } while (FMTLOG_BLOCK);  // 是否阻塞当前线程直到 push 成功
    }


    // logOnce 就是没有了 id, 不会重复利用之前的资源
    template<typename... Args>
    inline void logOnce(const char*                 location,
                        LogLevel                    level,
                        fmt::format_string<Args...> format,
                        Args&&... args)
    {
        fmt::string_view sv(format);
        auto&&           fmt_args   = fmt::make_format_args(args...);
        uint32_t         fmt_size   = formatted_size(sv, fmt_args);
        uint32_t         alloc_size = 8 + 8 + fmt_size;  // 时戳 + 位置 + 格式化数据
        do
        {
            if (auto header = AllocMsg(alloc_size))
            {
                header->logId  = (uint32_t)level;
                char* out      = (char*)(header + 1);  // 跳过消息头
                // *(int64_t*)out = m_timeStampCounter.Rdtsc();
                *(int64_t*)out = TimeStampCounterWarpper::impl.Rdtsc();
                out += 8;
                *(const char**)out = location;
                out += 8;
                vformat_to(out, sv, fmt_args);
                header->push(alloc_size);
                break;
            }
        } while (FMTLOG_BLOCK);
    }

  private:
    volatile LogLevel currentLogLevel = INF;
    // std::shared_ptr<ISink> m_fileSinkSptr  = nullptr;
};


struct FMTLOG_API fmtlogWrapper
{
    // C++11 后定义static变量多线程安全, 本质上来说应该是原子操作
    static fmtlog impl;  // 只是内部声明(并未分配内存), 所以需要外部再次定义(分配内存)
};


inline bool fmtlog::checkLogLevel(LogLevel logLevel) noexcept
{
#ifdef FMTLOG_NO_CHECK_LEVEL
    return true;
#else
    return logLevel >= fmtlogWrapper::impl.currentLogLevel;
#endif
}
