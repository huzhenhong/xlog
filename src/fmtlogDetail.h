/*************************************************************************************
 * Description  : 线程日志容器
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:46
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-01-06 19:53:11
 * FilePath     : \\FmtLog\\src\\fmtlogDetail.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include <mutex>
#include <thread>
#include <vector>
#include "Str.h"
#include "StaticLogInfo.h"
#include "SPSCVarQueueOPT.h"
#include <iostream>
#include "FileSink.h"


#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <processthreadsapi.h>
#else
    #include <sys/syscall.h>
    #include <unistd.h>
#endif


struct ThreadBuffer
{
    ThreadBuffer()
    {
        std::cout << "ThreadBuffer()\n";
    }

    ~ThreadBuffer()
    {
        std::cout << "~ThreadBuffer()\n";
    }

    SpScVarQueue varq;
    bool         shouldDeallocate = false;  // 是否需要在线程退出时释放
    char         name[32];
    size_t       nameSize;
};


struct HeapNode
{
    explicit HeapNode(ThreadBuffer* buffer)  // explicit 防止隐式转换
        : thBuf(buffer)
    {
    }

    ThreadBuffer*                  thBuf;
    const SpScVarQueue::MsgHeader* pHeader = nullptr;
};


class fmtlogDetail
{
  public:
    fmtlogDetail();
    ~fmtlogDetail();

    void                     RegisterLogInfo(uint32_t&        logId,
                                             FormatToFn       fn,
                                             const char*      location,
                                             LogLevel         level,
                                             fmt::string_view fmtString);

    SpScVarQueue::MsgHeader* AllocMsg(uint32_t size);
    void                     setHeaderPattern(const char* pattern);
    void                     resetDate();
    void                     preallocate();
    void                     startPollingThread(int64_t pollInterval);
    void                     stopPollingThread();
    void                     handleLog(fmt::string_view threadName, const SpScVarQueue::MsgHeader* header);
    void                     adjustHeap(size_t i);
    void                     poll(bool forceFlush);
    void                     setLogFile(const char* filename, bool truncate = false);

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

  public:
    int64_t                                                 midnightNs;
    fmt::string_view                                        m_headerPattern;
    bool                                                    shouldDeallocateHeader = false;
    std::mutex                                              m_threadBufMtx;
    std::vector<ThreadBuffer*>                              m_newThreadBufVec;
    std::vector<HeapNode>                                   m_allThreadBufVec;
    std::mutex                                              logInfoMutex;
    std::vector<StaticLogInfo>                              m_newLogInfo;
    std::vector<StaticLogInfo>                              m_allLogInfoVec;
    LogCBFn                                                 logCB = nullptr;
    LogLevel                                                m_minCBLogLevel;
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
    volatile bool                                           m_isThreadRunning = false;
    std::thread                                             m_thr;
    static FAST_THREAD_LOCAL ThreadBuffer*                  m_pThreadBuffer;  // __thread 时只能修饰static成员变量
    std::shared_ptr<ISink>                                  m_fileSinkSptr = nullptr;
};


struct fmtlogDetailWrapper
{
    static fmtlogDetail impl;
};

class ThreadBufferDestroyer
{
  public:
    explicit ThreadBufferDestroyer()
    {
        auto tid = std::this_thread::get_id();
        std::cout << *(unsigned int*)&tid << " ThreadBufferDestroyer()\n";
    }

    void threadBufferCreated() {}

    ~ThreadBufferDestroyer()
    {
        auto tid = std::this_thread::get_id();
        std::cout << *(unsigned int*)&tid << " ~ThreadBufferDestroyer()\n";

        if (fmtlogDetail::m_pThreadBuffer != nullptr)
        {
            fmtlogDetail::m_pThreadBuffer->shouldDeallocate = true;  // 其实是当前线程退出标识，因为无法得知当前线程什么时候退出的
            fmtlogDetail::m_pThreadBuffer                   = nullptr;
        }
    }
};
