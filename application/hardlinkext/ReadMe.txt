﻿========================================================================
    活动模板库 : hardlinkext 项目概述
========================================================================
/////////////////////////////////////////////////////////////////////////////
// -- 应用概述：
这是一个文件硬连接的shell扩展。
设计思想：磁盘中存在很多重复的文件，例如不同版本的f/w package, design kit,不同版本的workspace等，占据大量磁盘空间。可以利用文件硬连接功能，将重复的文件转换为硬连接，提高磁盘利用率。但是硬连接也存在问题：
(1) shell并没有方法区别文件时硬连接还是单独存在。
(2) 察看连接到文件的所有文件名
(3) 硬连接文件的修改，会影响到所有的连接。有时候需要避免这个功能。例如在workspace的分支中，首先会从原始workspace中复制，这是可以使用硬连接以节省磁盘空间。但是修改每个分支中的文件时，希望不能影响原来的workspace。

功能:
(1) 在所有硬连接文件(连接数>1)的文件图标上，叠加硬连接符号。
	-- 已实现
(2) 上下文菜单中实现所有连接到此文件的文件名。 （待实现）
(3) 上下文菜单中实现de-link功能。	（待实现）
(4) 实现copy on write功能

本文件概要介绍组成项目的每个文件的内容。

/////////////////////////////////////////////////////////////////////////////
// -- How To Install：
(1) 编译并生成.dll
(2) 将生成的.dll文件复制到目标目录
	- 如果旧版本的.dll正在运行，需要执行以下命令终止explorer
	taskkill /f /im explorer.exe
(3) 复制resource/link.ico到目标目录
(4) 注册COM
	- regsvr32 /s hlchk.dll
(5) 注册SHELL EXT
	- 双击install_com.reg
(6) 重新启动explorer
	- start /b explorer


hardlinkext.vcproj
    这是使用应用程序向导生成的 VC++ 项目的主项目文件，
    其中包含生成该文件的 Visual C++ 的版本信息，以及有关使用应用程序向导选择的平台、配置和项目功能的信息。

hardlinkext.idl
    此文件包含项目中定义的类型库、接口和 co-class 的 IDL 定义。
    此文件将由 MIDL 编译器进行处理以生成：
        C++ 接口定义和 GUID 声明 (hardlinkext.h)
        GUID 定义                                (hardlinkext_i.c)
        类型库                                  (hardlinkext.tlb)
        封送处理代码                                 （hardlinkext_p.c 和 dlldata.c）

hardlinkext.h
    此文件包含 hardlinkext.idl 中定义的项目的 C++ 接口定义和 GUID 声明。它将在编译过程中由 MIDL 重新生成。

hardlinkext.cpp
    此文件包含对象映射和 DLL 导出的实现。

hardlinkext.rc
    这是程序使用的所有 Microsoft Windows 资源的列表。

hardlinkext.def
    此模块定义文件为链接器提供有关 DLL 所要求的导出的信息，其中包含用于以下内容的导出：
DllGetClassObject
DllCanUnloadNow
DllRegisterServer
DllUnregisterServer

/////////////////////////////////////////////////////////////////////////////
其他标准文件：

StdAfx.h, StdAfx.cpp
    这些文件用于生成名为 hardlinkext.pch 的预编译头 (PCH) 文件和名为 StdAfx.obj 的预编译类型文件。

Resource.h
    这是用于定义资源 ID 的标准头文件。

/////////////////////////////////////////////////////////////////////////////
代理/存根 (stub) DLL 项目和模块定义文件：

hardlinkextps.vcproj
    此文件是用于生成代理/存根 (stub) DLL 的项目文件（若有必要）。
	主项目中的 IDL 文件必须至少包含一个接口，并且
	在生成代理/存根 (stub) DLL 之前必须先编译 IDL 文件。	此进程生成
	dlldata.c、hardlinkext_i.c 和 hardlinkext_p.c，这些文件是
	生成代理/存根 (stub) DLL 所必需的。

hardlinkextps.def
    此模块定义文件为链接器提供有关代理/存根 (stub) 所要求的导出的信息。

/////////////////////////////////////////////////////////////////////////////