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
    Poll(true);
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

    return m_pThreadBuffer->msgQueue.AllocMsg(size);  // 当前线程新打印一条日志
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
    thread_local ThreadLifeMonitor monitor;  // 定义为局部变量自带 static 属性
    monitor.Activate();                      // 延迟初始化 threadlocal，static 变量只有在第一次使用时才会初始化

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

                                Poll(false);

                                int64_t delay = TimeStampCounterWarpper::impl.Rdns() - before;

                                if (delay < pollInterval)
                                {
                                    std::this_thread::sleep_for(std::chrono::nanoseconds(pollInterval - delay));
                                }
                            }
                            Poll(true); });
}

void fmtlogDetail::stopPollingThread()
{
    if (!m_isThreadRunning) return;

    m_isThreadRunning = false;

    if (m_thr.joinable()) m_thr.join();
}


void fmtlogDetail::HandleOneLog(fmt::string_view               threadName,
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

// 堆化，构建小根堆
void fmtlogDetail::AdjustHeap(size_t i)
{
    // while (true)
    // {
    //     size_t minIdx = i;

    //     /*
    //     整个for循环就是为了找出左右子节点中最小的那个
    //     i 为父节点
    //     2 * i + 1 为左子节点
    //     2 * i + 2 为右子节点
    //     ch + 1 为右节点，每次都判断　ch < m_allThreadBufVec.size() 不就行了么
    //     end = std::min(ch + 2, m_allThreadBufVec.size()) 为左右子节点的边界，为了让循环最多只运行两次，真是会想
    //     ch++ 变成右子节点
    //     */
    //     for (size_t ch = i * 2 + 1, end = std::min(ch + 2, m_allThreadBufVec.size());
    //          ch < end;
    //          ch++)
    //     {
    //         // 比较当前子节点（先左子节点，再右子节点）和父节点
    //         auto pChild = m_allThreadBufVec[ch].pHeader;
    //         auto pRoot  = m_allThreadBufVec[minIdx].pHeader;

    //         if (pChild &&                                            // 子节点有日志需要处理
    //             (!pRoot ||                                           // !pRoot 现在指向第一个线程 buf，判断是否还有日志需要处理，没有了就需要下沉
    //              *(int64_t*)(pChild + 1) < *(int64_t*)(pRoot + 1)))  // 子节点时戳小需要下沉
    //         {
    //             minIdx = ch;  // 最小索引可能先置为左子节点，再和右子节点比较后变为右子节点
    //         }
    //     }

    //     if (minIdx == i)
    //         break;

    //     // 交换父节点和子节点，继续下沉
    //     std::swap(m_allThreadBufVec[i], m_allThreadBufVec[minIdx]);
    //     i = minIdx;
    // }

    int curRootIdx    = i;
    int leftChildIdx  = 2 * i + 1;  // 当前节点的第一个左孩子
    int rightChildIdx = 2 * i + 2;  // 当前节点的第一个右孩子（最后一个非叶子结点不一定有）

    while (leftChildIdx < m_allThreadBufVec.size())
    {
        auto pLeftChild    = m_allThreadBufVec[leftChildIdx]->msgQueue.Front();
        auto pRightChild   = m_allThreadBufVec[rightChildIdx]->msgQueue.Front();
        // auto pLeftChild    = m_allThreadBufVec[leftChildIdx].pHeader;
        // auto pRightChild   = m_allThreadBufVec[rightChildIdx].pHeader;
        int  smallChildIdx = 0;

        // 有右孩子且右孩子比左孩子大
        if (rightChildIdx < m_allThreadBufVec.size() &&
            pRightChild &&                                               // 有日志待处理
            *(int64_t*)(pRightChild + 1) < *(int64_t*)(pLeftChild + 1))  // 右子节点时戳更小
        {
            smallChildIdx = rightChildIdx;
        }
        else
        {
            smallChildIdx = leftChildIdx;
        }

        auto pSmallChild = m_allThreadBufVec[smallChildIdx]->msgQueue.Front();
        auto pCurRoot    = m_allThreadBufVec[curRootIdx]->msgQueue.Front();
        // auto pSmallChild = m_allThreadBufVec[smallChildIdx].pHeader;
        // auto pCurRoot    = m_allThreadBufVec[curRootIdx].pHeader;


        if (!pCurRoot ||                                               // 当前线程没有日志需要处理
            *(int64_t*)(pCurRoot + 1) > *(int64_t*)(pSmallChild + 1))  // 当前节点比子节点时戳大
        {
            std::swap(m_allThreadBufVec[curRootIdx], m_allThreadBufVec[smallChildIdx]);
            curRootIdx    = smallChildIdx;
            leftChildIdx  = 2 * curRootIdx + 1;
            rightChildIdx = 2 * curRootIdx + 2;
        }
        else
        {
            break;
        }
    }
}

void fmtlogDetail::Poll(bool forceFlush)
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
        auto pHeader = m_allThreadBufVec[i]->msgQueue.Front();
        if (pHeader)
        {
            // 此时新添加的线程 node.pHeader 必然不为 nullptr
            // 已有线程每次处理后都会指向下一条日志，这里就是对 buf 里还有日志的线程不做处理
            continue;
        }

        // 下面是已经处理完所有日志的线程，但是 msgQueue 为无锁队列，有可能又放入了新的日志

        pHeader = m_allThreadBufVec[i]->msgQueue.Front();  // 查看有没有新日志

        if (m_allThreadBufVec[i]->isThreadExit &&  // 为真说明线程已经退出了
            !pHeader)                              // 为空说明 msgQueue 里没有数据了
        {
            delete m_allThreadBufVec[i];
            m_allThreadBufVec[i] = m_allThreadBufVec.back();  // 将最后面的 buf 指针放到当前位置
            m_allThreadBufVec.pop_back();                     // 删除最后面的，这样可以避免不必要的拷贝
            --i;
        }

        // auto& node = m_allThreadBufVec[i];
        // if (node.pHeader)
        // {
        //     // 此时新添加的线程 node.pHeader 必然不为 nullptr
        //     // 已有线程每次处理后都会指向下一条日志，这里就是对 buf 里还有日志的线程不做处理
        //     continue;
        // }

        // // 下面是已经处理完所有日志的线程，但是 msgQueue 为无锁队列，有可能又放入了新的日志

        // node.pHeader = node.pThreadBuf->msgQueue.Front();  // 查看有没有新日志

        // if (node.pThreadBuf->isThreadExit &&  // 为真说明线程已经退出了
        //     !node.pHeader)                    // 为空说明 msgQueue 里没有数据了
        // {
        //     delete node.pThreadBuf;
        //     node = m_allThreadBufVec.back();  // 将最后面的 buf 指针放到当前位置
        //     m_allThreadBufVec.pop_back();     // 删除最后面的，这样可以避免不必要的拷贝
        //     --i;
        // }
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
        auto pHeader = m_allThreadBufVec[0]->msgQueue.Front();

        if (!pHeader ||                       // 队列为空，msg 已经处理完了
                                              // pHeader->logId >= m_allLogInfoVec.size() || // 大于不可能
            *(int64_t*)(pHeader + 1) >= tsc)  // header 之后保存着时戳，时钟跳变，只能重头堆化了，跳变错误的日志只能让它错下去，只保证日志文件里时戳有序
        {
            break;
        }

        // 处理当前线程 buf
        auto pThreadBuf = m_allThreadBufVec[0];

        HandleOneLog(fmt::string_view(pThreadBuf->name, pThreadBuf->nameSize), pHeader);

        pThreadBuf->msgQueue.Pop();  // 擦除标记，不是释放

        pThreadBuf->msgQueue.Front();  // 指向下一条日志

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