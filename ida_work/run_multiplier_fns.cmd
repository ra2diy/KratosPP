@echo off
cd /d D:\Workspace\ra2mod\platform\KratosPP\ida_work
"D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\ida.exe" -A -Lida_multiplierfns.log -S"D:\Workspace\ra2mod\platform\KratosPP\ida_work\dump_multiplier_fns.py" gamemd.idb
echo EXITCODE=%ERRORLEVEL%
