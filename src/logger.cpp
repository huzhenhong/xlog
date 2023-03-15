/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-14 18:18:11
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-03-15 14:47:43
 * FilePath     : \\xlog\\src\\logger.cpp
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#include "logger.h"
#include "TSCNS.h"
#include "fmtlogDetail.h"


// 按顺序构造
FAST_THREAD_LOCAL ThreadBuffer* fmtlogDetail::m_pThreadBuffer;  // 外部定义
fmtlogDetail                    fmtlogDetailWrapper::impl;      // 外部定义
fmtlog                          fmtlogWrapper::impl;            // 外部定义
TimeStampCounter                TimeStampCounterWarpper::impl;  // 外部定义
