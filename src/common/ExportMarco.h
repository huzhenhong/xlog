/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2022-01-20 14:14:23
 * LastEditors  : huzhenhong
 * LastEditTime : 2023-01-06 18:51:21
 * FilePath     : \\FmtLog\\src\\common\\ExportMarco.h
 * Copyright (C) 2022 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once

#if (defined(WIN32) || defined(WIN64))
    // #define WIN
    #ifdef LIBRARY_EXPORT
        #define FMTLOG_API __declspec(dllexport)
    #else
        #define FMTLOG_API __declspec(dllimport)
    #endif  // LIBRARY_EXPORT
#else       // NOT define win32 or win64
    #define FMTLOG_API __attribute__((visibility("default")))
#endif

#define EXTERN_C extern "C"
