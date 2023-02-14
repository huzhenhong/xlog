#include "fmtlog.h"
#include "Common.h"
#include "FileSink.h"
#include "fmtlogDetail.h"
#include <__functional/bind.h>
#include <functional>
#include <memory>


fmtlog::fmtlog()
{
    std::cout << "fmtlog()\n";
}


fmtlog::~fmtlog()
{
    std::cout << "~fmtlog()\n";
}


// 每条日志最先执行注册操作
void fmtlog::registerLogInfo(uint32_t&        logId,
                             FormatToFn       fn,
                             const char*      location,
                             const char*      funcName,
                             LogLevel         level,
                             fmt::string_view fmtString) noexcept
{
    fmtlogDetailWrapper::impl.RegisterLogInfo(logId, fn, location, funcName, level, fmtString);
}

SpScVarQueue::MsgHeader* fmtlog::AllocMsg(uint32_t size) noexcept
{
    return fmtlogDetailWrapper::impl.AllocMsg(size);
}

void fmtlog::preallocate() noexcept
{
    fmtlogDetailWrapper::impl.PreAllocate();
}

void fmtlog::setLogLevel(LogLevel logLevel) noexcept
{
    fmtlogWrapper::impl.currentLogLevel = logLevel;
}

LogLevel fmtlog::getLogLevel() noexcept
{
    return fmtlogWrapper::impl.currentLogLevel;
}

// bool fmtlog::checkLogLevel(LogLevel logLevel) noexcept
// {
// #ifdef FMTLOG_NO_CHECK_LEVEL
//     return true;
// #else
//     return logLevel >= fmtlogWrapper::impl.currentLogLevel;
// #endif
// }

void fmtlog::setLogFile(const char* filename, bool truncate)
{
    fmtlogDetailWrapper::impl.setLogFile(filename, truncate);
}


// void fmtlog::setLogFile(FILE* fp, bool manageFp)
// {
//     auto& d = fmtlogDetailWrapper::impl;
//     closeLogFile();
//     if (manageFp)
//     {
//         setbuf(fp, nullptr);
//         d.m_fpos = ftell(fp);
//     }
//     else
//     {
//         d.m_fpos = 0;
//     }

//     d.m_outputFp   = fp;
//     d.m_isManageFp = manageFp;
// }


void fmtlog::setFlushDelay(int64_t ns) noexcept
{
    // fmtlogDetailWrapper::impl.flushDelay = ns;
    // fmtlogWrapper::impl.m_fileSinkSptr->Sink(LogLevel(), "hahaha");
}


void fmtlog::flushOn(LogLevel flushLogLevel) noexcept
{
    // fmtlogDetailWrapper::impl.flushLogLevel = flushLogLevel;
}


void fmtlog::setFlushBufSize(uint32_t bytes) noexcept
{
    // fmtlogDetailWrapper::impl.flushBufSize = bytes;
}


void fmtlog::closeLogFile() noexcept
{
    // fmtlogDetailWrapper::impl.closeLogFile();
}


void fmtlog::poll(bool forceFlush)
{
    fmtlogDetailWrapper::impl.poll(forceFlush);
}

void fmtlog::setThreadName(const char* name) noexcept
{
    preallocate();

    // m_pThreadBuffer->nameSize =
    //     fmt::format_to_n(m_pThreadBuffer->name, sizeof(fmtlog::m_pThreadBuffer->name), "{}", name).size;
}


void fmtlog::setLogCB(LogCBFn cb, LogLevel minCBLogLevel_) noexcept
{
    // Fun fun                         = std::bind(&Test::callback, this, _1, _2);
    auto& d           = fmtlogDetailWrapper::impl;
    d.logCB           = cb;
    d.m_minCBLogLevel = minCBLogLevel_;
}


void fmtlog::setHeaderPattern(const char* pattern)
{
    // fmtlogDetailWrapper::impl.SetHeaderPattern(pattern);
}


void fmtlog::startPollingThread(int64_t pollInterval) noexcept
{
    fmtlogDetailWrapper::impl.startPollingThread(pollInterval);
}


void fmtlog::stopPollingThread() noexcept
{
    fmtlogDetailWrapper::impl.stopPollingThread();
}


void fmtlog::setTscGhz(double tscGhz) noexcept
{
    TimeStampCounterWarpper::impl.Reset(tscGhz);
}
