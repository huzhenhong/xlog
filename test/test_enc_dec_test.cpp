/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2023-01-06 20:03:41
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-26 18:04:28
 * FilePath     : \\xlog\\test\\test_enc_dec_test.cpp
 * Copyright (C) 2023 huzhenhong. All rights reserved.
 *************************************************************************************/
// #include "../fmtlog.h"
#include "fmt/core.h"
#include "logger.h"
#include "fmt/ranges.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <iterator>
#include <vector>

using namespace std;
using namespace fmt::literals;


struct MyType
{
    // explicit 会阻止编译器自动地调用构造函数或转换函数来创建对象或改变类型，会导致 vector 利用初始化列表初始化失败
    /* explicit */ MyType(int val)
        : value(val)
    {
    }

    // 自定义析构函数后，对象不可移动
    ~MyType()
    {
        dtorCnt++;
        fmt::print("dtorCnt: {}\n", dtorCnt);
    }

    int        value;
    static int dtorCnt;
};

int MyType::dtorCnt = 0;


template<>
struct fmt::formatter<MyType> : formatter<int>
{
    // parse is inherited from formatter<string_view>.
    template<typename FormatContext>
    auto format(const MyType& val, FormatContext& ctx) const
    {
        return formatter<int>::format(val.value, ctx);
    }
};


struct MovableType
{
  public:
    explicit MovableType(int val = 0)
        : value{MyType(val)}  // 拷贝构造，完成后会调一次 ~MyType()
    {
    }

    // 默认移动构造函数生成条件：
    // 1、没有 默认移动构造函数 = default，这个相当于自己定义了
    // 2、没有自定义拷贝构造函数，编译器认为你希望手动去拷贝
    // 3、没有自定义赋值构造函数，编译器认为你希望手动去拷贝
    // 4、没有自定义析构函数，，编译器认为你希望自己释放资源
    // 另外如果是POD之类的简单类型，移动构造函数内部应该调用的就是拷贝构造函数
    // ~MovableType()

    std::vector<MyType> value;
};


template<>
struct fmt::formatter<MovableType> : formatter<int>
{
    // parse is inherited from formatter<string_view>.
    template<typename FormatContext>
    auto format(const MovableType& val, FormatContext& ctx)
    {
        return formatter<int>::format(val.value[0].value, ctx);
    }
};


template<typename S, typename... Args>
void Test(const S& format, Args&&... args)
{
    fmt::print("=========\n");

    fmt::detail::check_format_string<Args...>(format);
    auto   sv            = fmt::string_view(format);
    size_t formattedSize = fmt::formatted_size(fmt::runtime(sv), std::forward<Args>(args)...);
    string formatedStr   = fmt::format(fmt::runtime(sv), std::forward<Args>(args)...);
    assert(formatedStr.size() == formattedSize);

    // 命名参数会以参数索引替代
    // "Hello, {name}! The answer is {number}. Goodbye, {name}.", "number"_a = 42, "name"_a = "World" 会处理成
    // "Hello, {1}! The answer is {0}. Goodbye, {1}."
    auto unnamedFormatStr = UnNameFormat<false>(sv, nullptr, args...);
    fmt::print("namedFormatStr: {}\n", sv);
    fmt::print("unnamedFormatStr: {}\n", unnamedFormatStr);
    size_t      cstringSizes[1000];  // 记录每个 cstring 的长度，因为其没有类似 size() 的接口获取 自身长度
    char        buf[1024];
    size_t      allocSize = GetArgSize<0>(cstringSizes, args...);
    const char* pRet      = EncodeArgs<0>(cstringSizes, buf, std::forward<Args>(args)...);
    assert(pRet - buf == allocSize);

    using Context      = fmt::format_context;
    using MemoryBuffer = fmt::basic_memory_buffer<char, 10000>;
    MemoryBuffer                                buffer;
    int                                         argIdx = -1;
    std::vector<fmt::basic_format_arg<Context>> formatArgVec;

    pRet = FormatTo<Args...>(unnamedFormatStr, buf, buffer, argIdx, formatArgVec);  // 返回拷贝执行到 buf 的哪里了
    assert(pRet - buf == allocSize);

    string_view encDecStr(buffer.data(), buffer.size());
    fmt::print("encDecStr: {}\n", encDecStr);
    fmt::print("formatedStr: {}\n", formatedStr);
    assert(encDecStr == formatedStr);

    fmt::print("=========\n");
}


int main()
{
    // {
    //     char tm[sizeof(int) + sizeof(int)];
    //     // MovableType tm[sizeof(int) + sizeof(int)];
    //     tm[0] = 1;
    //     // auto tm = new MovableType(3);
    //     // auto tm = MovableType(3);
    //     fmt::print("tm size: {}\n", tm->value.size());
    //     // fmt::print("tm size: {}\n", tm.value.size());

    //     auto pTm = new (tm) MovableType;
    //     // auto pTm = new (&tm) MovableType;
    //     fmt::print("size: {}\n", pTm->value.size());

    //     pTm->~MovableType();
    //     fmt::print("size: {}\n", pTm->value.size());
    //     // fmt::print("tm size: {}\n", tm.value.size());
    //     fmt::print("tm size: {}\n", tm->value.size());
    // }

    {
        auto a = MyType(1);
        auto b = std::move(a);

        auto c = MovableType(2);
        auto d = std::move(c);

        int  i = 0;
    }
    fmt::print("MyType size: {}\n", sizeof(MyType));
    fmt::print("MovableType size: {}\n", sizeof(MovableType));
    // fmt::print("MovableType size: {}\n", sizeof(std::vector<MyType>{1}));


    int g_dtorCnt{0};
    {
        MovableType obj(123);         // 构造时先构造一个 MyType 对象，然后调用拷贝构造函数，然后释放 MyType 对象
        g_dtorCnt = MyType::dtorCnt;  // MyType::dtorCnt = 1,
        Test("test copy: {}", obj);   // MyType::dtorCnt = 2, FormatTo 调用完 DecodeArgs 后会调用 Destruct 函数释放 placement new 对象

        // placement new 在堆上构建的 MovableType 用完后会调用 MyType 析构函数 对 dtorCnt +1
        assert(MyType::dtorCnt == g_dtorCnt + 1);
        assert(obj.value.size() == 1);  // 局部变量仍未释放
        g_dtorCnt = MyType::dtorCnt;
    }
    assert(MyType::dtorCnt == g_dtorCnt + 1);  // MyType::dtorCnt = 3，此时调用 MyType 析构函数 对 dtorCnt +1

    {
        MovableType val(456);  // MyType::dtorCnt = 4，构造时先构造一个 MyType 对象，然后调用拷贝构造函数，然后释放 MyType 对象
        g_dtorCnt = MyType::dtorCnt;
        Test("test move: {}", std::move(val));  // MyType::dtorCnt = 5, FormatTo 调用完 DecodeArgs 后会调用 Destruct 函数释放 placement new 对象
        // placement new 在堆上构建的 MovableType 用完后会调用 MyType 析构函数 对 dtorCnt +1
        assert(MyType::dtorCnt == g_dtorCnt + 1);
        assert(val.value.size() == 0);
        g_dtorCnt = MyType::dtorCnt;
    }
    assert(MyType::dtorCnt == g_dtorCnt);  // MyType::dtorCnt = 5，move 后局部变量不再调用析构

    // 一个 MovableType 构造时同上，MyType::dtorCnt = 6
    // Destruct 会析构两个 MyType，MovableType 析构时会析构成员 value（只有一个 MyType）,注意此时析构 MovableType 时已将 value vector 清空
    // 总过调用3次 ~MyType(), MyType::dtorCnt = 9
    Test("test custom types: {}, {}, {}", MyType(1), MyType(2), MovableType(3));
    // 接着会析构 MyType(1) 和 MyType(2)，MyType::dtorCnt = 11
    // MovableType(3) value vector 已经被清空

    // MyType::dtorCnt = 14, FormatTo 调用完 DecodeArgs 后会调用 Destruct 函数释放 placement new 对象
    Test("test ranges: {}, {}", vector<int>{1, 2, 3}, vector<MyType>{4, 5, 6});  // 怎样写构造函数可以支持列表初始化
    // 析构3个MyType，MyType::dtorCnt = 17


    char        cstr[100] = "hello ";
    const char* pCstr     = "world";
    const char* pCstring  = cstr;
    string      str("xlog");
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
         string_view(str),
         1.34,
         ch,
         chRef,
         i,
         iRef,
         d,
         f,
         shortInt);

    // {two:>5} : 右对齐，总宽度为5
    // {0:.1f} : 保留一位小数，没有小树补0
    Test("test positional, {one}, {two:>5}, {three}, {four}, {0:.1f}",
         5.012,
         "three"_a = 3,
         "two"_a   = "two",
         "one"_a   = string("one"),
         "four"_a  = string("4"));

    // {:*^30} : 内容居中，两边补*
    Test("test dynamic spec: {:.{}f}, {:*^30}", 3.14, 1, "centered");

    Test("test positional spec: int: {0:d};  hex: {0:#x};  oct: {0:#o};  bin: {0:#b}", 42);

    fmt::print("tests passed\n");

    return 0;
}
