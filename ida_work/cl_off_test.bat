@echo off
call "%~dp0..\scripts\run_vsdevcmd.bat"
cl /nologo /EHsc /c /I "%~dp0..\YRpp" /Fo"%~dp0off_test.obj" "%~dp0off_test.cpp"
