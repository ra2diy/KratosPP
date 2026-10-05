@echo off
rem Verify the exact vtable displacements MSVC emits for the NEW Kratos call pattern.
call "D:\Workspace\ra2mod\platform\KratosPP\scripts\run_vsdevcmd.bat" -vcvars_ver=14.29 >nul 2>&1
cd /D "D:\Workspace\ra2mod\platform\KratosPP\ida_work\vtprobe"
set INCLUDE=D:\Workspace\ra2mod\platform\KratosPP\src;D:\Workspace\ra2mod\platform\KratosPP\YRpp;D:\Workspace\ra2mod\platform\KratosPP\include;D:\Workspace\ra2mod\platform\KratosPP\lib;%INCLUDE%
cl /c /nologo /FAs /Facalls.asm /D IS_RELEASE_VER /D SYR_VER=2 /D HAS_EXCEPTIONS=0 /D NOMINMAX /D _CRT_SECURE_NO_WARNINGS /D _WIN32_WINNT=0x0601 /D NTDDI_VERSION=0x06010000 /MT /Zp8 /GS- /GR /Gz /std:c++20 /permissive- /wd4100 /wd4201 /wd4530 /wd4731 /wd4740 /wd4458 /wd4819 /wd5103 /wd5105 calls.cpp > calls.log 2>&1
echo CL_EXIT=%ERRORLEVEL%
