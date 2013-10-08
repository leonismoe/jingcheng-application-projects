// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#ifndef _WIN32_WINNT            // 指定要求的最低平台是 Windows Vista。
#define _WIN32_WINNT 0x0501     // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif

#ifdef _DEBUG

#define LOGGER_LEVEL LOGGER_LEVEL_DEBUGINFO

#else

#define LOGGER_LEVEL LOGGER_LEVEL_ERROR

#endif

#include <stdio.h>
#include <tchar.h>



// TODO: 在此处引用程序需要的其他头文件
