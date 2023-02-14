/*************************************************************************************
 * Description  : 线程日志容器
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:46
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-14 18:09:21
 * FilePath     : \\xlog\\src\\fmtlogDetail.h
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
                                             const char*      funcName,
                                             LogLevel         level,
                                             fmt::string_view fmtString);

    SpScVarQueue::MsgHeader* AllocMsg(uint32_t size);
    void                     PreAllocate();
    void                     startPollingThread(int64_t pollInterval);
    void                     stopPollingThread();
    void                     handleLog(fmt::string_view threadName, const SpScVarQueue::MsgHeader* header);
    void                     AdjustHeap(size_t i);
    void                     poll(bool forceFlush);
    void                     setLogFile(const char* filename, bool truncate = false);


  public:
    std::mutex                             m_threadBufMtx;
    std::vector<ThreadBuffer*>             m_newThreadBufVec;
    std::vector<HeapNode>                  m_allThreadBufVec;
    std::mutex                             m_logInfoMutex;
    std::vector<StaticLogInfo>             m_newLogInfo;
    std::vector<StaticLogInfo>             m_allLogInfoVec;
    LogCBFn                                logCB = nullptr;
    LogLevel                               m_minCBLogLevel;
    fmt::basic_memory_buffer<char, 10000>  m_membuf;  // 日志写入的地方
    volatile bool                          m_isThreadRunning = false;
    std::thread                            m_thr;
    static FAST_THREAD_LOCAL ThreadBuffer* m_pThreadBuffer;  // __thread 时只能修饰static成员变量
    std::shared_ptr<ISink>                 m_fileSinkSptr = nullptr;

    PatternFormatterSptr                   m_patternFormaterSptr;
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
