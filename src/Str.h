/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-01 17:57:09
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-08 09:34:53
 * FilePath     : \\xlog\\src\\Str.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include <cstddef>  // size_t: long unsigned int
#include <cstdint>  // uint16_t

// https://github.com/MengRao/str
template<std::size_t SIZE>  // std::size_t : long unsigned int
class Str
{
  public:
    Str() {}

    Str(const char* ptr)
    {
        *this = *(const Str<SIZE>*)ptr;
    }

    char* Ptr()
    {
        return charArr;
    }

    char& operator[](int i)
    {
        return charArr[i];
    }

    char operator[](int i) const  // 何必加这个const？
    {
        return charArr[i];
    }

    template<typename T>
    void FromInteger(T num)
    {
        // year Size=4 num=2022
        auto n = SIZE;

        if constexpr (SIZE & 1)  // SIZE 为奇数
        {
            charArr[SIZE - 1] = '0' + (num % 10);  // 处理个位
            num /= 10;                             // num 变为偶数位（注意不是偶数，是偶数位）
        }

        /*
            源码        反码        补码
        4   00000100   00000100   00000100
        -2  10000010   11111101   11111110
            4 & -2 为 00000100

        3   00000011   00000011   00000011
        -2  10000010   11111101   11111110
            3 & -2 为 00000010

        -2 的补码为 11111110，正好可以去掉 charArr 的最后一位，余下总位数为偶数位
        */

        switch (SIZE & -2)
        {
            /*
                以
                case 18:
                    *(uint16_t*)(charArr + 16) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                为例

                case 18 是因为已经单独处理了最后一位
                (num % 100) 拿到最后两位数字，比如为 99，然后 99 << 1 是因为 pDigitTable 里面每个数字占两个字节
                转为 (uint16_t*) 是因为 uint16_t 正好可以取两个字节

                charArr + 16 是因为在 18 的基础上刚刚又处理了最后面的两位

                num /= 100 剩余数字就再减少两位

                    这里以 100 为单位应该是比 10 少一半的运算，因为要处理的 case 情况减半，改为以 1000 为单位也可以，
                    只是这样就很复杂了，要单独先处理两位数及一位数的情况，pDigitTable 需要存储 000~999
                */

            // 最大支持 19 位数 case 18:
            *(uint16_t*)(charArr + 16) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
            num /= 100;
            case 16:
                *(uint16_t*)(charArr + 14) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 14:
                *(uint16_t*)(charArr + 12) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 12:
                *(uint16_t*)(charArr + 10) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 10:
                *(uint16_t*)(charArr + 8) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 8:
                *(uint16_t*)(charArr + 6) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 6:
                *(uint16_t*)(charArr + 4) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 4:
                *(uint16_t*)(charArr + 2) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
            case 2:
                *(uint16_t*)(charArr + 0) = *(uint16_t*)(pDigitTable + ((num % 100) << 1));
                num /= 100;
        }
    }

  private:
    static constexpr const char* pDigitTable = "00010203040506070809"
                                               "10111213141516171819"
                                               "20212223242526272829"
                                               "30313233343536373839"
                                               "40414243444546474849"
                                               "50515253545556575859"
                                               "60616263646566676869"
                                               "70717273747576777879"
                                               "80818283848586878889"
                                               "90919293949596979899";

    char                         charArr[SIZE];  // 直接访问和通过接口获取速度大概差 30%，但是基本可以忽略
};