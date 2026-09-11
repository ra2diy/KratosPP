@echo off
cd /d D:\Workspace\ra2mod\platform\KratosPP\ida_work
"D:\Workspace\ra2mod\platform\IDA_Pro_v8.3_Portable\ida.exe" -A -Lida_crateselect.log -S"D:\Workspace\ra2mod\platform\KratosPP\ida_work\dump_crate_select.py" gamemd.idb
echo EXITCODE=%ERRORLEVEL%
