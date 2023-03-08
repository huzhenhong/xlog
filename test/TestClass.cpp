/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:54
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-08 09:59:20
 * FilePath     : \\xlog\\test\\TestClass.cpp
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#include <iostream>
#include <chrono>
#include <thread>
#include "../src/logger.h"
#include "fmt/core.h"
// #include "fmt/format.h"
#include "fmt/args.h"
// #include <thread>
#include "fmt/ranges.h"


void runBenchmark()
{
    const int                                      RECORDS = 10000000;
    // fmtlog::setLogFile("/dev/null", false);
    // fmtlog::closeLogFile();
    // fmtlog::setLogCB(nullptr, WRN);

    std::chrono::high_resolution_clock::time_point t0, t1;

    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < RECORDS; ++i)
    {
        logi("Simple log message with one parameters, {}", i);
    }
    t1 = std::chrono::high_resolution_clock::now();

    double span = std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
    fmt::print("benchmark, front latency: {:.1f} ns/msg average\n", (span / RECORDS) * 1e9);
}

int main(int argc, char* argv[])
{
    // auto func = []()
    // {
    //     std::thread::id tid = std::this_thread::get_id();
    //     logi("1, {}", *(unsigned int*)&tid);
    //     logi("1, {}", 1);
    //     logi("2, {}", 2);
    // };

    // std::thread t1(func);
    // t1.join();

    fmtlog::setLogFile("/dev/null", false);

    std::thread::id                                    tid = std::this_thread::get_id();

    // 命名动态参数:

    fmt::dynamic_format_arg_store<fmt::format_context> store;
    // store.push_back("const"_a = "pi");
    store.push_back(fmt::arg("const", "pi"));
    store.push_back(fmt::arg("val", 3.14f));
    std::string result = fmt::vformat("{const} = {val}\n", store);
    fmt::print(result);  // result is "pi = 3.14"

    // 未命名的动态参数:
    fmt::dynamic_format_arg_store<fmt::format_context> unmanedstore;
    unmanedstore.push_back("answer to everything");
    unmanedstore.push_back(42);
    result = fmt::vformat("{} is {}\n", unmanedstore);
    fmt::print(result);  // result is "answer to everything is 42"

    fmt::print("Hello, {name}\n", fmt::arg("name", "test"));


    using namespace fmt::literals;

    logi("0, {}", 42);
    // logi("I'd rather be {1} than {0}.", "right", "happy");
    // logi("Hello, {name}! The answer is {}.", "name"_a = "World", 42);
    // logi("Hello, {name}! The answer is {number}. Goodbye, {name}.", "name"_a = "World", fmt::arg("number", 42));
    logi("Hello, {name}! The answer is {number}. Goodbye, {name}.", "name"_a = "World", "number"_a = 42);
    std::vector<int> v = {1, 2, 3};
    int              s = sizeof(v);
    logi("ranges: {}", v);

    logi("std::move can be used for objects with non-trivial destructors: {}", std::move(v));
    assert(v.size() == 0);

    std::tuple<int, char> t = {1, 'a'};
    logi("tuples: {}", fmt::join(t, ", "));

    enum class color
    {
        red,
        green,
        blue
    };

    // template<>
    // struct fmt::formatter<color> : formatter<string_view>
    // {
    //     // parse is inherited from formatter<string_view>.
    //     template<typename FormatContext>
    //     auto format(color c, FormatContext& cstx)
    //     {
    //         string_view name = "unknown";
    //         switch (c)
    //         {
    //             case color::red: name = "red"; break;
    //             case color::green: name = "green"; break;
    //             case color::blue: name = "blue"; break;
    //         }
    //         return formatter<string_view>::format(name, ctx);
    //     }
    // };
    // logi("user defined type: {:>10}", color::blue);
    logi("{:*^30}", "centered");
    logi("int: {0:d};  hex: {0:#x};  oct: {0:#o};  bin: {0:#b}", 42);
    logi("dynamic precision: {:.{}f}", 3.14, 1);

    // This gives a compile-time error because d is an invalid format specifier for a string.
    // FMT_STRING() is not needed from C++20 onward
    // logi(FMT_STRING("{:d}"), "I am not a number");


    // logi("1, {YmdHMSF}", 2202);
    logi("1, {}", *(unsigned int*)&tid);
    logi("2, {}", 2);


    runBenchmark();
    fmtlog::startPollingThread();
    // fmtlog::poll();

    return 0;
}