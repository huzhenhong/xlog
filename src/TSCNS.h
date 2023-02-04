#pragma once
#include <chrono>
#include <atomic>
#include <thread>

#ifdef _MSC_VER
    #include <intrin.h>
#endif
#include <iostream>


class TimeStampCounter
{
  public:
    TimeStampCounter()
    {
        std::cout << "TimeStampCounter() \n";
    }

    ~TimeStampCounter()
    {
        std::cout << "~TimeStampCounter() \n";
    }

    void Reset(int64_t calibrateDurationNs = 20'000'000,  // 20ms
               int64_t calibateIntervalNs  = 3 * 1000'000'000)
    {
        m_calibateIntervalNs = calibateIntervalNs;

        int64_t baseTsc, baseNs;
        GetSyncTime(baseTsc, baseNs);

        int64_t expireNs = baseNs + calibrateDurationNs;
        while (Rdsysns() < expireNs)
        {
            std::this_thread::yield();  // 放弃本次执行
        }

        int64_t currentTsc, currentNs;
        GetSyncTime(currentTsc, currentNs);

        double nsPerTsc = static_cast<double>(currentNs - baseNs) / (currentTsc - baseTsc);

        SaveParam(baseTsc, baseNs, baseNs, nsPerTsc);
    }

    void Calibrate()
    {
        if (Rdtsc() < m_nextCalibrateTsc) return;

        int64_t tsc, ns;

        GetSyncTime(tsc, ns);

        int64_t calulated_ns                     = Tsc2ns(tsc);
        int64_t ns_err                           = calulated_ns - ns;
        int64_t expected_err_at_next_calibration = ns_err + (ns_err - m_baseNsErr) * m_calibateIntervalNs / (ns - m_baseNs + m_baseNsErr);

        double  new_ns_per_tsc = m_nsPerTsc * (1.0 - (double)expected_err_at_next_calibration / m_calibateIntervalNs);

        SaveParam(tsc, calulated_ns, ns, new_ns_per_tsc);
    }

    static inline int64_t Rdtsc()
    {
#ifdef _MSC_VER
        return __rdtsc();
#elif defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
        return __builtin_ia32_rdtsc();
#else
        return Rdsysns();
#endif
    }

    inline int64_t Tsc2ns(int64_t tsc) const
    {
        while (true)
        {
            uint32_t beforeSequence = m_paramSequence.load(std::memory_order_acquire) & ~1;  // ～ 取反

            std::atomic_signal_fence(std::memory_order_acq_rel);

            int64_t ns = m_baseNs + (int64_t)((tsc - m_baseTsc) * m_nsPerTsc);

            std::atomic_signal_fence(std::memory_order_acq_rel);

            uint32_t afterSequence = m_paramSequence.load(std::memory_order_acquire);

            if (beforeSequence == afterSequence)
            {
                return ns;
            }
        }
    }

    inline int64_t Rdns() const
    {
        return Tsc2ns(Rdtsc());
    }

    static inline int64_t Rdsysns()
    {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    double getTscGhz() const
    {
        return 1.0 / m_nsPerTsc;
    }

    // Linux kernel sync time by finding the first trial with tsc diff < 50000
    // We try several times and return the one with the mininum tsc diff.
    // Note that MSVC has a 100ns resolution clock, so we need to combine those ns with the same
    // value, and drop the first and the last value as they may not scan a full 100ns range
    static void GetSyncTime(int64_t& tsc, int64_t& ns)
    {
#ifdef _MSC_VER
        const int N = 15;
#else
        const int N = 3;
#endif
        int64_t tscArr[N + 1];
        int64_t nsArr[N + 1];

        tscArr[0] = Rdtsc();
        for (int i = 1; i <= N; i++)
        {
            tscArr[i] = Rdtsc();
            nsArr[i]  = Rdsysns();
        }

#ifdef _MSC_VER
        int j = 1;
        for (int i = 2; i <= N; i++)  // i = 2 =》drop the first
        {
            if (nsArr[i] == nsArr[i - 1]) continue;

            tscArr[j - 1] = tscArr[i - 1];  // 舍弃tsc没变的
            nsArr[j++]    = nsArr[i];       // 舍弃ns没变的
        }
        j--;  // drop the last，最后总数不是 N + 1，最多是 N - 1
#else
        int       j = N + 1;
#endif

        // 找出间隔最短的两次计数
        int bestIdx = 1;
        for (int i = 2; i < j; i++)
        {
            if (tscArr[i] - tscArr[i - 1] < tscArr[bestIdx] - tscArr[bestIdx - 1])
            {
                bestIdx = i;
            }
        }

        tsc = (tscArr[bestIdx] + tscArr[bestIdx - 1]) >> 1;  // 求平均
        ns  = nsArr[bestIdx];                                // 标准库对应ns
    }

    void SaveParam(int64_t baseTsc, int64_t baseNs, int64_t sysNs, double NsPerTsc)
    {
        m_baseNsErr        = baseNs - sysNs;
        m_nextCalibrateTsc = baseTsc + (int64_t)((m_calibateIntervalNs - 1000) / NsPerTsc);
        uint32_t sequence  = m_paramSequence.load(std::memory_order_relaxed);
        m_paramSequence.store(++sequence, std::memory_order_release);
        std::atomic_signal_fence(std::memory_order_acq_rel);
        m_baseTsc  = baseTsc;
        m_baseNs   = baseNs;
        m_nsPerTsc = NsPerTsc;
        std::atomic_signal_fence(std::memory_order_acq_rel);
        m_paramSequence.store(++sequence, std::memory_order_release);
    }

    alignas(64) std::atomic<uint32_t> m_paramSequence = 0;
    double  m_nsPerTsc;
    int64_t m_baseTsc;
    int64_t m_baseNs;
    int64_t m_calibateIntervalNs;
    int64_t m_baseNsErr;
    int64_t m_nextCalibrateTsc;
};

struct TimeStampCounterWarpper
{
    static TimeStampCounter impl;
};
