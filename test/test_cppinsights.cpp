/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-03-01 18:15:48
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-07 09:41:12
 * FilePath     : \\xlog\\test\\test_cppinsights.cpp
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/

#include "fmt/core.h"
#include "logger.h"
#include "fmt/ranges.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <cassert>


template<typename FMT, typename... Args>
void Test(const FMT& fmt, Args&&... args)
{
    fmt::print("=========\n");

    fmt::detail::check_format_string<Args...>(fmt);
    auto        fmtStr        = fmt::string_view(fmt);
    size_t      formattedSize = fmt::formatted_size(fmt::runtime(fmtStr), std::forward<Args>(args)...);
    std::string formatedStr   = fmt::format(fmt::runtime(fmtStr), std::forward<Args>(args)...);
    assert(formatedStr.size() == formattedSize);

    // 命名参数会以参数索引替代
    // "Hello, {name}! The answer is {number}. Goodbye, {name}.", "number"_a = 42, "name"_a = "World" 会处理成
    // "Hello, {1}! The answer is {0}. Goodbye, {1}."
    auto unnamedFormatStr = UnNameFormat<false>(fmtStr, nullptr, args...);
    fmt::print("namedFormatStr: {}\n", fmtStr);
    fmt::print("unnamedFormatStr: {}\n", unnamedFormatStr);

    size_t      cstringSizes[1000];  // 记录每个 cstring 的长度，因为其没有类似 size() 的接口获取 自身长度
    size_t      totalArgSize = GetArgSize<0>(cstringSizes, args...);

    char        argSaveBuf[1024];
    const char* pRet = EncodeArgs<0>(cstringSizes, argSaveBuf, std::forward<Args>(args)...);
    assert(pRet - argSaveBuf == totalArgSize);

    // using Context      = fmt::format_context;
    // using MemoryBuffer = fmt::basic_memory_buffer<char, 10000>;
    fmt::basic_memory_buffer<char, 10000>                   decBuf;
    int                                                     argIdx = -1;
    std::vector<fmt::basic_format_arg<fmt::format_context>> extraArgVec;  // 额外需要格式化的参数

    pRet = Format<Args...>(unnamedFormatStr, argSaveBuf, decBuf, argIdx, extraArgVec);  // 返回拷贝执行到 buf 的哪里了
    assert(pRet - argSaveBuf == totalArgSize);

    std::string_view encDecStr(decBuf.data(), decBuf.size());
    fmt::print("encDecStr: {}\n", encDecStr);
    fmt::print("formatedStr: {}\n", formatedStr);
    assert(encDecStr == formatedStr);

    fmt::print("=========\n");
}


int main()
{
    char        cstr[100] = "hello ";
    const char* pCstr     = "world";
    const char* pCstring  = cstr;
    std::string str("xlog");
    char        ch       = 'f';
    char&       chRef    = ch;
    int         i        = 5;
    int&        iRef     = i;
    double      d        = 3.45;
    float       f        = 55.2;
    uint16_t    shortInt = 2222;

    Test("test basic types: {}, {}, {}, {}, {}, {}, {}, {}, {:.1f}, {}, {}, {}, {}, {}, {}, {}",
         cstr,
         pCstr,
         pCstring,
         "say",
         'x',
         5,
         str,
         std::string_view(str),
         1.34,
         ch,
         chRef,
         i,
         iRef,
         d,
         f,
         shortInt);

    return 0;
}
