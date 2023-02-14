/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-08-09 13:56:54
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-02-14 17:57:58
 * FilePath     : \\xlog\\test\\TestClass.cpp
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#include <iostream>
#include <chrono>
#include <thread>
#include "../src/logger.h"
// #include <thread>


void runBenchmark()
{
    const int                                      RECORDS = 1000000;
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

    std::thread::id tid = std::this_thread::get_id();

    logi("1, {}", *(unsigned int*)&tid);
    logi("1, {}", 1);
    logi("2, {}", 2);


    fmtlog::poll();
    // fmtlog::startPollingThread();
    runBenchmark();

    return 0;
}