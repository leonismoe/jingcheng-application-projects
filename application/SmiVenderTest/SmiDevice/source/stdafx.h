﻿#pragma once

#ifndef _WIN32_WINNT		//                
#define _WIN32_WINNT 0x0501	//
#endif		

//#ifdef 1
#define LOGGER_LEVEL LOGGER_LEVEL_DEBUGINFO
//#else
//#define LOGGER_LEVEL LOGGER_LEVEL_NOTICE
//#endif

#ifdef _DEBUG
#define LOG_OUT_CLASS_SIZE
#define LOCAL_LOG_LEV	LOGGER_LEVEL_DEBUGINFO
#else
#define LOCAL_LOG_LEV	LOGGER_LEVEL_ERROR
#endif

#include <stdio.h>
#include <tchar.h>
