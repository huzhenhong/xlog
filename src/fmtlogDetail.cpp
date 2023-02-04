#include "fmtlogDetail.h"
#include "fmt/core.h"
#include "TSCNS.h"
#include "argutil.h"


fmtlogDetail::fmtlogDetail()
{
    std::cout << "fmtlogDetail()" << std::endl;

    m_patternArgVec.reserve(4096);
    m_patternArgVec.resize(parttenArgSize);

    TimeStampCounterWarpper::impl.Reset();

    resetDate();
    // fmtlog::setLogFile(stdout);
    setHeaderPattern("{HMSF} {s:<16} {l}[{t:<6}] ");
    // setHeaderPattern("{HMSf} {s:<16} {l}[{t:<6}] ");
    m_newLogInfo.reserve(32);
    m_allLogInfoVec.reserve(128);
    // m_allLogInfoVec.emplace_back(nullptr, nullptr, DBG, fmt::string_view());
    // m_allLogInfoVec.emplace_back(nullptr, nullptr, INF, fmt::string_view());
    // m_allLogInfoVec.emplace_back(nullptr, nullptr, WRN, fmt::string_view());
    // m_allLogInfoVec.emplace_back(nullptr, nullptr, ERR, fmt::string_view());
    m_newThreadBufVec.reserve(8);
    m_allThreadBufVec.reserve(8);
    memset(m_membuf.data(), 0, m_membuf.capacity());
}

fmtlogDetail::~fmtlogDetail()
{
    std::cout << "~fmtlogDetail()" << std::endl;
    stopPollingThread();
    poll(true);
    // closeLogFile();
}

void fmtlogDetail::RegisterLogInfo(uint32_t&        logId,
                                   FormatToFn       fn,
                                   const char*      location,
                                   LogLevel         level,
                                   fmt::string_view fmtString)
{
    std::lock_guard<std::mutex> lock(logInfoMutex);
    if (logId)
        return;

    // logInfos 都会移到 bgLogInfos 里面去，bgLogInfos 不会删除日志，所以logId 只增不减
    logId = m_newLogInfo.size() + m_allLogInfoVec.size();

    m_newLogInfo.emplace_back(fn, location, level, fmtString);
}

SpScVarQueue::MsgHeader* fmtlogDetail::AllocMsg(uint32_t size)
{
    if (!m_pThreadBuffer)
        preallocate();

    return m_pThreadBuffer->varq.AllocMsg(size);  // 当前线程新打印一条日志
}

void fmtlogDetail::setHeaderPattern(const char* pattern)
{
    if (shouldDeallocateHeader)
        delete[] m_headerPattern.data();

    // 每个参数参数对应的位置，先全部初始化为最后YmdHMSF
    // 相当于pattern里有parttenArgSize个参数，每一个都是YmdHMSF
    for (int i = 0; i < parttenArgSize; i++)
    {
        m_reorderIdx[i] = parttenArgSize - 1;
    }


    /*
    "a"_a       = ""
    先调用
    constexpr auto operator"" _a(const char* s, size_t) -> detail::udl_arg<char>
    {
        return {s};
    }
    得到 udl_arg 对象, 再调用其
    template <typename T> auto operator=(T&& value) const -> named_arg<Char, T>
    {
        return {str, std::forward<T>(value)};
    }
    得到 named_arg 对象
    */

    using namespace fmt::literals;
    m_headerPattern = UnNameFormat<true>(pattern,
                                         m_reorderIdx,
                                         "a"_a       = "",
                                         "b"_a       = "",
                                         "C"_a       = "",
                                         "Y"_a       = "",
                                         "m"_a       = "",
                                         "d"_a       = "",
                                         "t"_a       = "thread name",
                                         "F"_a       = "",
                                         "f"_a       = "",
                                         "e"_a       = "",
                                         "S"_a       = "",
                                         "M"_a       = "",
                                         "H"_a       = "",
                                         "l"_a       = "INF",
                                         //  "l"_a       = LogLevel(),
                                         "s"_a       = "fmtlog.cpp:123",
                                         "g"_a       = "/home/raomeng/fmtlog/fmtlog.cpp:123",
                                         "Ymd"_a     = "",
                                         "HMS"_a     = "",
                                         "HMSe"_a    = "",
                                         "HMSf"_a    = "",
                                         "HMSF"_a    = "",
                                         "YmdHMS"_a  = "",
                                         "YmdHMSe"_a = "",
                                         "YmdHMSf"_a = "",
                                         "YmdHMSF"_a = "");
    // 上面列出了所有支持的模式

    // 如果有命名参数, 会对"pattern"进行修改, 需要对其进行释放
    // pattern：{HMSf} {s:<16} {l}[{t:<6}]
    // headerPattern：{} {:<16} {}[{:<6}]
    // reorderIdx：{24, 24, 24, 24, 24, 24, 3, 24, 24, ...}
    // 目的就是找到每个参数对应的索引，后面再根据这个索引来设置参数的值
    shouldDeallocateHeader = m_headerPattern.data() != pattern;

    // 确定参数类型，占个坑，因为后面的setArgVal只是设置值，没有改变参数类型和占用内存大小
    // 没有的话会导致后面vformat_to时崩溃，特别是后面也没有setArgVal再手动设置时间戳
    // 虽然可以选择
    setArg<0>(fmt::string_view(weekdayName.str, 3));
    setArg<1>(fmt::string_view(monthName.str, 3));
    setArg<2>(fmt::string_view(&year[2], 2));
    setArg<3>(fmt::string_view(year.str, 4));
    setArg<4>(fmt::string_view(month.str, 2));
    setArg<5>(fmt::string_view(day.str, 2));
    setArg<6>(fmt::string_view());  // 对应线程名，无法确定长度，需要每次手动设置
    setArg<7>(fmt::string_view(nanosecond.str, 9));
    setArg<8>(fmt::string_view(nanosecond.str, 6));
    setArg<9>(fmt::string_view(nanosecond.str, 3));
    setArg<10>(fmt::string_view(second.str, 2));
    setArg<11>(fmt::string_view(minute.str, 2));
    setArg<12>(fmt::string_view(hour.str, 2));
    setArg<13>(fmt::string_view(m_logLevel.str, 3));
    setArg<14>(fmt::string_view());              // 日志位置相对路径，无法确定长度，需要每次手动设置
    setArg<15>(fmt::string_view());              // 日志位置绝对路径，无法确定长度，需要每次手动设置
    setArg<16>(fmt::string_view(year.str, 10));  // Ymd
    setArg<17>(fmt::string_view(hour.str, 8));   // HMS
    setArg<18>(fmt::string_view(hour.str, 12));  // HMSe
    setArg<19>(fmt::string_view(hour.str, 15));  // HMSf
    setArg<20>(fmt::string_view(hour.str, 18));  // HMSF，精确到纳秒
    // setArg<20>(fmt::string_view());              // HMSF，意思就是后面不手动设置的话就是不要
    setArg<21>(fmt::string_view(year.str, 19));  // YmdHMS
    setArg<22>(fmt::string_view(year.str, 23));  // YmdHMSe
    setArg<23>(fmt::string_view(year.str, 26));  // YmdHMSf
    setArg<24>(fmt::string_view(year.str, 29));  // YmdHMSF
}


void fmtlogDetail::resetDate()
{
    time_t     rawtime  = TimeStampCounterWarpper::impl.Rdns() / 1000'000'000;  // s
    struct tm* timeinfo = localtime(&rawtime);
    timeinfo->tm_sec = timeinfo->tm_min = timeinfo->tm_hour = 0;
    midnightNs                                              = mktime(timeinfo) * 1000'000'000;
    year.fromi(1900 + timeinfo->tm_year);
    month.fromi(1 + timeinfo->tm_mon);
    day.fromi(timeinfo->tm_mday);
    const char* weekdays[7]    = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    weekdayName                = weekdays[timeinfo->tm_wday];
    const char* monthNames[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    monthName                  = monthNames[timeinfo->tm_mon];
}

void fmtlogDetail::preallocate()
{
    if (m_pThreadBuffer)
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
    thread_local ThreadBufferDestroyer sbc;  // 定义为局部变量自带static属性
    sbc.threadBufferCreated();               // 延迟初始化 threadlocal

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
    setArgVal<6>(threadName);

    const char* pData = (const char*)(pHeader + 1);  // sizeof(SpScVarQueue::MsgHeader) 为 8，最前面的8个字节存放消息头信息
    const char* pEnd  = (const char*)pHeader + pHeader->size;

    // 格式化时间戳
    int64_t     tsc       = *(int64_t*)pData;
    int64_t     curTimeNs = TimeStampCounterWarpper::impl.Tsc2ns(tsc);
    // the date could go back when polling different threads
    uint64_t    tmp       = (curTimeNs > midnightNs) ? (curTimeNs - midnightNs) : 0;
    nanosecond.fromi(tmp % 1000'000'000);
    tmp /= 1000'000'000;  // s
    second.fromi(tmp % 60);
    tmp /= 60;
    minute.fromi(tmp % 60);
    tmp /= 60;
    uint32_t h = tmp;  // hour
    if (h > 23)        // 24:00:00，两条日志间隔超过有可能很大
    {
        h %= 24;
        resetDate();  // 重置年月日
    }
    hour.fromi(h);

    pData += 8;  // 跳过 tsc
    StaticLogInfo& info = m_allLogInfoVec[pHeader->logId];
    if (!info.formatToFn)
    {
        // log once
        info.location = *(const char**)pData;
        pData += 8;  // 跳过 location
        info.processLocation();
    }

    setArgVal<14>(info.getBase());      // 函数名
    setArgVal<15>(info.getLocation());  // 调用代码位置

    // 日志级别
    m_logLevel = (const char*)"DBG INF WRN ERR OFF" + (info.logLevel << 2);

    size_t headerPos = m_membuf.size();

    // m_membuf里追加写入固定日志头，m_patternArgVec 里面的 arg 已经填充了 val
    vformat_to(m_membuf, m_headerPattern, fmt::basic_format_args(m_patternArgVec.data(), parttenArgSize));
    size_t bodyPos = m_membuf.size();

    if (info.formatToFn)
    {
        // decode args
        info.formatToFn(info.formatString, pData, m_membuf, info.argIdx, m_patternArgVec);
    }
    else
    {
        // log once
        m_membuf.append(fmt::string_view(pData, pEnd - pData));
    }

    // 可以将单条日志推送上去
    if (logCB && info.logLevel >= m_minCBLogLevel)
    {
        logCB(curTimeNs,
              info.logLevel,
              info.getLocation(),
              info.basePos,
              threadName,
              fmt::string_view(m_membuf.data() + headerPos, m_membuf.size() - headerPos),
              bodyPos - headerPos /* ,
               m_fpos + headerPos */
        );
    }
}

// 堆化
void fmtlogDetail::adjustHeap(size_t i)
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
        std::unique_lock<std::mutex> lock(logInfoMutex);  // registerLogInfo 的时候也会用到 m_newLogInfo
        for (auto& info : m_newLogInfo)
        {
            info.processLocation();
        }
        m_allLogInfoVec.insert(m_allLogInfoVec.end(), m_newLogInfo.begin(), m_newLogInfo.end());
        m_newLogInfo.clear();
    }

    // 将新创建的线程buf移到buf容器里去
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
    for (size_t i = 0; i < m_allThreadBufVec.size(); i++)
    {
        auto& node = m_allThreadBufVec[i];
        if (node.pHeader) continue;  // 不可能出现吧

        // 拿到buf头指针
        node.pHeader = node.thBuf->varq.Front();

        // 当前线程buf里没有数据了，且线程需要退出，则删除buf
        if (!node.pHeader && node.thBuf->shouldDeallocate)
        {
            delete node.thBuf;
            node = m_allThreadBufVec.back();  // 将最后面的buf指针放到当前位置
            m_allThreadBufVec.pop_back();     // 删除最后面的
            i--;
        }
    }

    if (m_allThreadBufVec.empty()) return;

    // build heap，小顶堆
    // 最后一个非叶子节点（size / 2 - 1）
    // for (int i = m_allThreadBufVec.size() / 2; i >= 0; i--)
    for (int i = m_allThreadBufVec.size() / 2 - 1; i >= 0; i--)
    {
        adjustHeap(i);
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

        // 处理当前线程buf
        auto tb = m_allThreadBufVec[0].thBuf;
        handleLog(fmt::string_view(tb->name, tb->nameSize), h);
        tb->varq.Pop();                                   // 擦除标记，不是释放
        m_allThreadBufVec[0].pHeader = tb->varq.Front();  // 指向下一条日志

        // 堆从小到大排序，确保 m_allThreadBufVec[0].pHeader 为时戳最早的那个线程
        adjustHeap(0);  // 下沉，再次拿到最小的放在开始位置
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
                                                                        size_t           basePos,
                                                                        fmt::string_view threadName,
                                                                        fmt::string_view msg,
                                                                        size_t           bodyPos /* ,
                                                                         size_t           logFilePos */
                                      )
    {
        m_fileSinkSptr->Sink(level, msg);
    };
}