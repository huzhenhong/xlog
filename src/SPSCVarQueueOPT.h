#pragma once
#include <_types/_uint32_t.h>

// https://github.com/MengRao/SPSC_Queue

class SpScVarQueue
{
  public:
    struct MsgHeader
    {
        inline void Push(uint32_t sz)
        {
            *(volatile uint32_t*)&size = sz + sizeof(MsgHeader);  // 没必要 volatile 吧
        }

        uint32_t size;
        uint32_t logId;
    };

    MsgHeader* AllocMsg(uint32_t size) noexcept
    {
        size += sizeof(MsgHeader);                                                   // 多一个消息头
        uint32_t needBlankCnt = (size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);  // 需要多少个块

        // m_freeCnt 代表剩余空间
        // 第一次肯定是代表尾巴上的剩余
        // 之后可能代表开头、可能代表中间、也可能代表尾巴

        if (needBlankCnt >= m_freeCnt)
        {
            uint32_t tmpReadIdx = *(volatile uint32_t*)&m_readIdx;

            // 读在写之前
            if (tmpReadIdx <= m_writeIdx)
            {
                // 尾巴上的剩余空间
                m_freeCnt = BLANK_CNT - m_writeIdx;

                // 尾巴上的剩余空间也不够
                // 读也不在开头位置，则将开头到读位置变为剩余空间
                if (needBlankCnt >= m_freeCnt && tmpReadIdx != 0)
                {
                    // wrap around，翻译为回绕比较好理解
                    m_blank[m_writeIdx].size = 1;           // 之前写到的位置（不包括当前），后面没有数据了
                    m_writeIdx               = 0;           // 从头写
                    m_blank[0].size          = 0;           // 0 代表当前写到的位置（不包括当前）
                    m_freeCnt                = tmpReadIdx;  // 读之前的全部可写
                }
            }
            else  // 读在写之后
            {
                // 可写为中间部分
                m_freeCnt = tmpReadIdx - m_writeIdx;
            }

            // 可写容量不足
            if (m_freeCnt <= needBlankCnt)
            {
                return nullptr;
            }
        }

        // 剩余容量够，直接往后挪
        MsgHeader* pCur = &m_blank[m_writeIdx];
        m_writeIdx += needBlankCnt;
        m_blank[m_writeIdx].size = 0;  // 0 代表当前写到的位置（不包括当前）
        m_freeCnt -= needBlankCnt;

        return pCur;
    }

    inline const MsgHeader* Front()
    {
        uint32_t size = m_blank[m_readIdx].size;
        if (size == 1)      // 之前写到这里了，后面没有了
        {                   // wrap around
            m_readIdx = 0;  // 从头再读
            size      = m_blank[0].size;
        }

        if (size == 0)  // 还没有放入数据
        {
            return nullptr;
        }

        return &m_blank[m_readIdx];
    }

    inline void Pop()
    {
        // m_blank[m_readIdx].size 表示本条日志大小
        // m_blank[m_readIdx].size 若为零表示还没有使用，在 font 时就会返回 nullptr
        uint32_t needBlankCnt           = (m_blank[m_readIdx].size + sizeof(MsgHeader) - 1) / sizeof(MsgHeader);
        *(volatile uint32_t*)&m_readIdx = m_readIdx + needBlankCnt;
    }

  private:
    static constexpr uint32_t BLANK_CNT      = (1 << 20) / sizeof(MsgHeader);
    // alignas 参考 https://developer.aliyun.com/article/996348、https://developer.aliyun.com/article/1068586
    alignas(64) MsgHeader m_blank[BLANK_CNT] = {};  // 对齐的是数组的首地址，而不是每个数组元素
    uint32_t m_writeIdx                      = 0;
    uint32_t m_freeCnt                       = BLANK_CNT;
    alignas(128) uint32_t m_readIdx          = 0;
};
