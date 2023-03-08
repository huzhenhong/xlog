#include "fmtlogDetail.h"
#include "PatternFormatter.h"
#include "fmt/core.h"
#include "TSCNS.h"
#include "argutil.h"
#include <memory>


fmtlogDetail::fmtlogDetail()
{
    std::cout << "fmtlogDetail()" << std::endl;

    m_patternFormaterSptr = std::make_shared<PatternFormatter>();

    TimeStampCounterWarpper::impl.Reset();

    m_newLogInfo.reserve(32);
    m_allLogInfoVec.reserve(128);
    m_newThreadBufVec.reserve(8);
    m_allThreadBufVec.reserve(8);
    memset(m_membuf.data(), 0, m_membuf.capacity());
}

fmtlogDetail::~fmtlogDetail()
{
    std::cout << "~fmtlogDetail()" << std::endl;
    stopPollingThread();
    poll(true);
}

void fmtlogDetail::RegisterLogInfo(uint32_t&        logId,
                                   FormatToFn       fn,
                                   const char*      location,
                                   const char*      funcName,
                                   LogLevel         level,
                                   fmt::string_view fmtString)
{
    std::lock_guard<std::mutex> lock(m_logInfoMutex);  // 注册时必须加锁
    if (logId)
        return;  // 其实根本不存在这种可能

    // m_newLogInfo 在 poll 的时候都会移到 bgLogInfos m_allLogInfoVec 不会删除日志，所以logId 只增不减
    logId = m_newLogInfo.size() + m_allLogInfoVec.size();

    m_newLogInfo.emplace_back(fn, location, funcName, level, fmtString);
}

SpScVarQueue::MsgHeader* fmtlogDetail::AllocMsg(uint32_t size)
{
    if (!m_pThreadBuffer)
        PreAllocate();

    return m_pThreadBuffer->varq.AllocMsg(size);  // 当前线程新打印一条日志
}

void fmtlogDetail::PreAllocate()
{
    if (m_pThreadBuffer)  // 这里其实不用再判断了
        return;

    // 多线程环境创建各自的缓冲区
    m_pThreadBuffer = new ThreadBuffer();


#ifdef _WIN32
    uint32_t tid = static_cast<uint32_t>(::GetCurrentThreadId());
#else
    uint32_t tid = static_cast<uint32_t>(::syscall(SYS_gettid));
#endif

    m_pThreadBuffer->nameSize = fmt::format_to_n(m_pThreadBuffer->name,
                                                 sizeof(m_pThreadBuffer->name),
                                                 "{}",
                                                 tid)
                                    .size;

    // 这里的用途就是为了在线程退出时会根据
    // fmtlog::threadBuffer->shouldDeallocate = true;
    // 释放 HeapNode 里的 ThreadBuffer，也就是 fmtlog::m_pThreadBuffer
    thread_local ThreadBufferDestroyer sbc;  // 定义为局部变量自带 static 属性
    sbc.threadBufferCreated();               // 延迟初始化 threadlocal，static 变量只有在第一次使用时才会初始化

    std::unique_lock<std::mutex> guard(m_threadBufMtx);
    m_newThreadBufVec.push_back(m_pThreadBuffer);
}


void fmtlogDetail::startPollingThread(int64_t pollInterval)
{
    stopPollingThread();

    m_isThreadRunning = true;

    m_thr = std::thread([pollInterval, this]()
                        {
                            while (m_isThreadRunning)
                            {
                                int64_t before = TimeStampCounterWarpper::impl.Rdns();

                                poll(false);

                                int64_t delay = TimeStampCounterWarpper::impl.Rdns() - before;

                                if (delay < pollInterval)
                                {
                                    std::this_thread::sleep_for(std::chrono::nanoseconds(pollInterval - delay));
                                }
                            }
                            poll(true); });
}

void fmtlogDetail::stopPollingThread()
{
    if (!m_isThreadRunning) return;

    m_isThreadRunning = false;

    if (m_thr.joinable()) m_thr.join();
}


void fmtlogDetail::handleLog(fmt::string_view               threadName,
                             const SpScVarQueue::MsgHeader* pHeader)
{
    StaticLogInfo& info = m_allLogInfoVec[pHeader->logId];  // m_allLogInfoVec 都是按顺序 push 的

    int64_t        logTimeNs;
    size_t         headPos = m_membuf.size();
    size_t         bodyPos;

    m_patternFormaterSptr->Format(threadName, pHeader, info, m_membuf, logTimeNs, bodyPos);

    // 可以将单条日志推送上去
    if (logCB && info.m_logLevel >= m_minCBLogLevel)
    {
        logCB(logTimeNs,
              info.m_logLevel,
              info.GetLocation(),
              info.GetFuncName(),
              threadName,
              fmt::string_view(m_membuf.data() + headPos, m_membuf.size() - headPos),
              bodyPos - headPos);
    }
}

// 堆化
void fmtlogDetail::AdjustHeap(size_t i)
{
    while (true)
    {
        size_t min_i = i;

        /*
        整个for循环就是为了找出左右子节点中最小的那个
        i 为父节点
        2 * i + 1 为左子节点
        2 * i + 2 为右子节点
        ch+1 为右节点，每次都判断　ch < m_allThreadBufVec.size() 不就行了么
        end = std::min(ch + 2, m_allThreadBufVec.size()) 为左右子节点的边界，为了让循环最多只运行两次，真是会想
        ch++ 变成右子节点
        */
        for (size_t ch = i * 2 + 1, end = std::min(ch + 2, m_allThreadBufVec.size());
             ch < end;
             ch++)
        {
            // 比较当前子节点（先左子节点，再右子节点）和父节点
            auto h_ch  = m_allThreadBufVec[ch].pHeader;
            auto h_min = m_allThreadBufVec[min_i].pHeader;

            // h_ch 子节点有日志需要处理
            // !h_min 现在指向第一个线程buf，判断是否还有日志需要处理，没有了就需要下沉
            // *(int64_t*)(h_ch + 1) < *(int64_t*)(h_min + 1)) 子节点时戳小需要下沉
            if (h_ch && (!h_min || *(int64_t*)(h_ch + 1) < *(int64_t*)(h_min + 1)))
                min_i = ch;  // 最小索引可能先置为左子节点，再和右子节点比较后变为右子节点
        }

        if (min_i == i)
            break;

        // 交换父节点和子节点，继续下沉
        std::swap(m_allThreadBufVec[i], m_allThreadBufVec[min_i]);
        i = min_i;
    }
}

void fmtlogDetail::poll(bool forceFlush)
{
    TimeStampCounterWarpper::impl.Calibrate();  // 持续校准

    int64_t tsc = TimeStampCounterWarpper::impl.Rdtsc();

    // 将新添加的日志移到日志容器
    if (m_newLogInfo.size() > 0)
    {
        std::unique_lock<std::mutex> lock(m_logInfoMutex);  // registerLogInfo 的时候也会用到 m_newLogInfo
        for (auto& info : m_newLogInfo)
        {
            info.ProcessLocation();
        }
        m_allLogInfoVec.insert(m_allLogInfoVec.end(), m_newLogInfo.begin(), m_newLogInfo.end());
        m_newLogInfo.clear();
    }

    // 将新创建的线程 buf 移到 buf 容器里去
    if (m_newThreadBufVec.size() > 0)
    {
        std::unique_lock<std::mutex> lock(m_threadBufMtx);
        for (auto threadBuf : m_newThreadBufVec)
        {
            m_allThreadBufVec.emplace_back(threadBuf);
        }
        m_newThreadBufVec.clear();
    }

    // 删除退出线程
    for (size_t i = 0; i < m_allThreadBufVec.size(); ++i)
    {
        auto& node = m_allThreadBufVec[i];
        if (node.pHeader) continue;  // 不可能出现吧

        // 拿到buf头指针
        node.pHeader = node.pThreadBuf->varq.Front();

        // 当前线程buf里没有数据了，且线程需要退出，则删除buf
        if (!node.pHeader && node.pThreadBuf->shouldDeallocate)
        {
            delete node.pThreadBuf;
            node = m_allThreadBufVec.back();  // 将最后面的buf指针放到当前位置
            m_allThreadBufVec.pop_back();     // 删除最后面的
            --i;
        }
    }

    if (m_allThreadBufVec.empty()) return;

    // build heap，小顶堆
    // 最后一个非叶子节点（size / 2 - 1）
    // for (int i = m_allThreadBufVec.size() / 2; i >= 0; i--)
    for (int i = m_allThreadBufVec.size() / 2 - 1; i >= 0; --i)
    {
        AdjustHeap(i);
    }
    // 现在 m_allThreadBufVec[0].pHeader 是最小的

    // 这里没有采用先排好序再一次性处理的原因就是因为排序数组不是固定的
    // 当前线程的第2条日志可能比其余线程的第1条日志时间都要早
    // 所以相当于要对一个二维数组排序，而且不好直接原地排，重新申请内存存放也不是个好主意
    // 最好就是找到一条一条的处理吧
    while (true)
    {
        auto h = m_allThreadBufVec[0].pHeader;
        // !h 表示处理完了
        // h->logId >= m_allLogInfoVec.size() 这不可能吧
        // *(int64_t*)(h + 1) >= tsc 时钟跳变，但是就算本轮退出日志的真是时戳也还是不准
        if (!h || h->logId >= m_allLogInfoVec.size() || *(int64_t*)(h + 1) >= tsc)  // header 之后保存着时戳，检测时钟跳变
            break;

        // 处理当前线程 buf
        auto tb = m_allThreadBufVec[0].pThreadBuf;
        handleLog(fmt::string_view(tb->name, tb->nameSize), h);
        tb->varq.Pop();                                   // 擦除标记，不是释放
        m_allThreadBufVec[0].pHeader = tb->varq.Front();  // 指向下一条日志

        // 堆从小到大排序，确保 m_allThreadBufVec[0].pHeader 为时戳最早的那个线程
        AdjustHeap(0);  // 下沉，再次拿到最小的放在开始位置
    }
}

void fmtlogDetail::setLogFile(const char* filename, bool truncate)
{
    m_fileSinkSptr = std::make_shared<FileSink>("fmtlog.txt", true);

    // fmtlogDetailWrapper::impl.logCB = std::bind(&fmtlog::LogCB,
    //                                             this,
    //                                             std::placeholders::_1,
    //                                             std::placeholders::_2,
    //                                             std::placeholders::_3,
    //                                             std::placeholders::_4,
    //                                             std::placeholders::_5,
    //                                             std::placeholders::_6,
    //                                             std::placeholders::_7);

    fmtlogDetailWrapper::impl.logCB = [m_fileSinkSptr = m_fileSinkSptr](int64_t          ns,
                                                                        LogLevel         level,
                                                                        fmt::string_view location,
                                                                        fmt::string_view funcname,
                                                                        // size_t           basePos,
                                                                        fmt::string_view threadName,
                                                                        fmt::string_view msg,
                                                                        size_t           bodyOffSet)
    {
        // auto head = fmt::string_view(msg.data(), bodyOffSet);
        // auto body = fmt::string_view(msg.data() + bodyOffSet, msg.size() - bodyOffSet);
        m_fileSinkSptr->Sink(level, msg);
    };
}