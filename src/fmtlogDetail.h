/*************************************************************************************
 * Description  : 线程日志容器
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:46
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 17:23:30
 * FilePath     : \\xlog\\src\\fmtlogDetail.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include <mutex>
#include <thread>
#include <vector>
#include "Common.h"
#include "Str.h"
#include "StaticLogInfo.h"
#include "SPSCVarQueueOPT.h"
#include <iostream>
#include "FileSink.h"
#include "PatternFormatter.h"


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

    SpScVarQueue msgQueue;              // 通过 volatile 实现的无锁队列
    bool         isThreadExit = false;  // 线程是否退出
    size_t       nameSize;
    char         name[32];
};


class fmtlogDetail
{
  public:
    fmtlogDetail();
    ~fmtlogDetail();

    void                     SetHeaderPattern(const char* pPattern);

    void                     RegisterLogInfo(uint32_t&        logId,
                                             FormatToFn       fn,
                                             const char*      location,
                                             const char*      funcName,
                                             LogLevel         level,
                                             fmt::string_view fmtString);

    SpScVarQueue::MsgHeader* AllocMsg(uint32_t size);

    void                     PreAllocate();

    void                     startPollingThread(int64_t pollInterval);

    void                     stopPollingThread();

    void                     HandleOneLog(fmt::string_view threadName, const SpScVarQueue::MsgHeader* header);

    void                     AdjustHeap(size_t i);

    void                     Poll(bool forceFlush);

    // void                     setLogFile(const char* filename, bool truncate = false);

    // void                     setLogFile(FILE* fp, bool manageFp);

    void                     SetLogCB(LogCBFn logCB, LogLevel minCBLogLevel);

    // void                     SetFlushDelay(int64_t ns) noexcept;

  public:
    static FAST_THREAD_LOCAL ThreadBuffer* m_pThreadBuffer;  // __thread 时只能修饰static成员变量

  private:
    std::mutex                            m_threadBufMtx;

    std::vector<ThreadBuffer*>            m_newThreadBufVec;

    std::vector<ThreadBuffer*>            m_allThreadBufVec;

    std::mutex                            m_logInfoMutex;

    std::vector<StaticLogInfo>            m_newLogInfo;

    std::vector<StaticLogInfo>            m_allLogInfoVec;

    LogCBFn                               m_logCB = nullptr;

    LogLevel                              m_minCBLogLevel;

    fmt::basic_memory_buffer<char, 10000> m_membuf;  // 日志写入的地方

    volatile bool                         m_isThreadRunning = false;

    std::thread                           m_thr;

    // std::shared_ptr<ISink>                m_fileSinkSptr = nullptr;

    PatternFormatterSptr                  m_patternFormaterSptr;
};


struct fmtlogDetailWrapper
{
    static fmtlogDetail impl;
};


class ThreadLifeMonitor
{
  public:
    explicit ThreadLifeMonitor()
    {
        auto tid = std::this_thread::get_id();
        std::cout << *(unsigned int*)&tid << " ThreadLifeMonitor()\n";
    }

    void Activate() {}

    ~ThreadLifeMonitor()
    {
        auto tid = std::this_thread::get_id();
        std::cout << *(unsigned int*)&tid << " ~ThreadLifeMonitor()\n";

        if (fmtlogDetail::m_pThreadBuffer != nullptr)
        {
            fmtlogDetail::m_pThreadBuffer->isThreadExit = true;
            fmtlogDetail::m_pThreadBuffer               = nullptr;
        }
    }
};
