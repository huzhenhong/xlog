/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-19 11:00:55
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 19:34:03
 * FilePath     : \\xlog\\src\\FileSink.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "ISink.h"
#include "TSCNS.h"


class FileSink : public ISink
{
  public:
    FileSink(const char* filename, bool truncate)
    {
        Open(filename, truncate);
    }

    ~FileSink()
    {
        // Flush();  // 防止启动就退出日志不落盘
        Close();
        std::cout << "~FileSink()" << std::endl;
    }

    void Sink(LogLevel level, fmt::string_view msg) override
    {
        if (level >= m_flushLevel)
        {
            m_membuf.append(msg);
            m_membuf.push_back('\n');

            if (m_membuf.size() >= m_flushBufSize)
            {
                // 保护磁盘
                int64_t tsc = TimeStampCounterWarpper::impl.Rdtsc();
                int64_t now = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
                if (now > m_nextFlushTime)
                {
                    Flush();
                }
                else if (m_nextFlushTime == std::numeric_limits<int64_t>::max())  // 首次执行
                {
                    m_nextFlushTime = now + m_flushDelay;
                }
            }
        }
    }

    size_t GetFilePos() noexcept
    {
        return m_fpos;
    }

    void SetFlushDelay(int64_t ns) noexcept
    {
        m_flushDelay = ns;
    }

    void SetFlushLevel(LogLevel flushLevel) noexcept
    {
        m_flushLevel = flushLevel;
    }

    void SetFlushBufSize(uint32_t bufSize) noexcept
    {
        m_flushBufSize = bufSize;
    }

  private:
    bool Open(const char* filename, bool truncate)
    {
        m_outputFp = fopen(filename, truncate ? "w" : "a");
        if (!m_outputFp)
        {
            std::string err = fmt::format("Unable to open file: {}: {}", filename, strerror(errno));
            fmt::detail::throw_format_error(err.c_str());
        }

        setbuf(m_outputFp, nullptr);
        m_fpos       = ftell(m_outputFp);
        m_isManageFp = true;
    }

    void Close()
    {
        if (m_membuf.size())
        {
            Flush(true);
        }

        int ret = 0;
        if (m_isManageFp)
        {
            ret = fclose(m_outputFp);
        }

        m_outputFp   = nullptr;
        m_isManageFp = false;
    }

    void Flush(bool forceFlush = false)
    {
        if (m_membuf.size() == 0)
            return;

        if (m_outputFp)
        {
            fwrite(m_membuf.data(), 1, m_membuf.size(), m_outputFp);

            if (!m_isManageFp || forceFlush)
                fflush(m_outputFp);
            else
                m_fpos += m_membuf.size();
        }

        m_membuf.clear();
        m_nextFlushTime = (std::numeric_limits<int64_t>::max)();
    }

  private:
    LogLevel                              m_flushLevel;
    fmt::basic_memory_buffer<char, 10000> m_membuf;
    uint32_t                              m_flushBufSize  = 8 * 1024;
    FILE*                                 m_outputFp      = nullptr;
    bool                                  m_isManageFp    = false;
    size_t                                m_fpos          = 0;            // file position of membuf, used only when manageFp == true
    int64_t                               m_flushDelay    = 100'000'000;  // ns
    int64_t                               m_nextFlushTime = (std::numeric_limits<int64_t>::max)();
};