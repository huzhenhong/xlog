/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-10-19 11:38:46
 * LastEditors  : huzhenhong
 * LastEditTime : 2022-10-19 13:44:52
 * FilePath     : \\FmtLog\\src\\ISink.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once
#include "Common.h"


class ISink
{
  public:
    ISink() {}

    virtual ~ISink() {}

    virtual void Sink(LogLevel level, fmt::string_view msg) = 0;
};