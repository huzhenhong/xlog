/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-01 17:57:09
 * LastEditors  : huzhenhong
 * LastEditTime : 2022-08-04 15:56:57
 * FilePath     : \\logger\\src\\Str.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include <cstdint>

// https://github.com/MengRao/str
template<std::size_t SIZE>
class Str
{
  public:
    char str[SIZE];  // 直接访问和通过函数获取速度到底差多少呢，这个类应该是用来快速格式化数字的，所以改成接口问题不大

    Str() {}

    Str(const char* p)
    {
        *this = *(const Str<SIZE>*)p;
    }

    char& operator[](int i)
    {
        return str[i];
    }

    char operator[](int i) const  // 何必加这个const？
    {
        return str[i];
    }

    template<typename T>
    void fromi(T num)
    {
        // year Size=4 num=2022
        // 处理个位数
        // if constexpr (Size & 1)
        if constexpr (SIZE & 1)
        {
            str[SIZE - 1] = '0' + (num % 10);
            // str[Size - 1] = '0' + (num % 10);
            num /= 10;
        }

        // 余下总位数为偶数位
        // 00001001 => 00001001 =>     00001001
        // 10000010 => 11111101 + 1 => 11111110 选-2就是为了消除最末尾的１，因为Size去掉最末尾的１之后余下的肯定是２的倍数
        // 00001000

        // *(uint16_t*)(digit_pairs + ((num % 100) << 1))　为直接查表，digit_pairs为数字表，左移一位是因为每个数字占两位，从00~99
        // num /= 100 剩余数字就再减少两位
        // 这里以100为单位应该是比10少一半的运算，因为要处理的case情况减半，改为以1000为单位也可以，只是这样就很复杂了，要单独先处理两位数及一位数的情况

        // 处理十位数及以上
        // switch (Size & -2)
        switch (SIZE & -2)
        {
            case 18:
                *(uint16_t*)(str + 16) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 16:
                *(uint16_t*)(str + 14) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 14:
                *(uint16_t*)(str + 12) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 12:
                *(uint16_t*)(str + 10) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 10:
                *(uint16_t*)(str + 8) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 8:
                *(uint16_t*)(str + 6) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 6:
                *(uint16_t*)(str + 4) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 4:
                *(uint16_t*)(str + 2) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
            case 2:
                *(uint16_t*)(str + 0) = *(uint16_t*)(digit_pairs + ((num % 100) << 1));
                num /= 100;
        }
    }

  private:
    // static const int             Size = SIZE;

    // char                         str[SIZE];

    static constexpr const char* digit_pairs = "00010203040506070809"
                                               "10111213141516171819"
                                               "20212223242526272829"
                                               "30313233343536373839"
                                               "40414243444546474849"
                                               "50515253545556575859"
                                               "60616263646566676869"
                                               "70717273747576777879"
                                               "80818283848586878889"
                                               "90919293949596979899";
};