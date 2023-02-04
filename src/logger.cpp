/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-14 18:18:11
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-01-05 17:11:29
 * FilePath     : \\FmtLog\\src\\logger.cpp
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#include "logger.h"
// #include "fmtlog.h"
#include "TSCNS.h"
#include "fmtlogDetail.h"


// 按顺序构造
// thread_local ThreadBufferDestroyer fmtlogDetail::sbc;              // 外部定义
FAST_THREAD_LOCAL ThreadBuffer* fmtlogDetail::m_pThreadBuffer;  // 外部定义
fmtlogDetail                    fmtlogDetailWrapper::impl;      // 外部定义
fmtlog                          fmtlogWrapper::impl;            // 外部定义
TimeStampCounter                TimeStampCounterWarpper::impl;  // 外部定义
