@echo off
cd /d D:\Workspace\ra2mod\platform\KratosPP\ida_work
"D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\ida.exe" -A -Lida_p21.log -S"D:\Workspace\ra2mod\platform\KratosPP\ida_work\p21_setheight.py" gamemd_p1.idb
echo EXITCODE=%ERRORLEVEL%
