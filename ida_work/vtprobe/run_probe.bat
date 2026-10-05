@echo off
rem Build a standalone probe against the Kratos YRpp headers and dump the class layout.
call "D:\Workspace\ra2mod\platform\KratosPP\scripts\run_vsdevcmd.bat" >nul 2>&1
cd /D "D:\Workspace\ra2mod\platform\KratosPP\ida_work\vtprobe"
set INCLUDE=D:\Workspace\ra2mod\platform\KratosPP\src;D:\Workspace\ra2mod\platform\KratosPP\YRpp;D:\Workspace\ra2mod\platform\KratosPP\include;D:\Workspace\ra2mod\platform\KratosPP\lib;%INCLUDE%
echo === cl version ===
cl 2>&1 | findstr /C:"Version"
echo === layout report ===
cl /c /nologo /d1reportSingleClassLayoutObjectClass /d1reportSingleClassLayoutTechnoClass /d1reportSingleClassLayoutFootClass /FAs /Faprobes.asm /D IS_RELEASE_VER /D SYR_VER=2 /D HAS_EXCEPTIONS=0 /D NOMINMAX /D _CRT_SECURE_NO_WARNINGS /D _WIN32_WINNT=0x0601 /D NTDDI_VERSION=0x06010000 /MT /Zp8 /GS- /GR /Gz /std:c++20 /permissive- /wd4100 /wd4201 /wd4530 /wd4731 /wd4740 /wd4458 /wd4819 /wd5103 /wd5105 probe.cpp > layout.txt 2>&1
echo CL_EXIT=%ERRORLEVEL%
