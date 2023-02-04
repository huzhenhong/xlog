/*************************************************************************************
 * Description  :
 * Version      : 1.0
 * Author       : huzhenhong
 * Date         : 2021-06-19 04:02:57
 * LastEditors  : huzhenhong
 * LastEditTime : 2022-02-21 14:38:59
 * FilePath     : \\CMakeProjectFramework\\src\\common\\PlatformInclude.h
 * Copyright (C) 2021 huzhenhong. All rights reserved.
 *************************************************************************************/
#pragma once

#if (defined(WIN32) || defined(WIN64))
    #include <Windows.h>
#else
    #include <dlfcn.h>
    #include <sys/syscall.h>
    #include <unistd.h>
#endif
