@echo off
chcp 65001

cd %~dp0
echo f|xcopy /i /y "..\output\Release\*.dll" "..\output\*.dll"
echo f|xcopy /i /y "..\output\Release\*.pdb" "..\output\*.pdb"

if exist run_yr.bat (
    call run_yr
) else (
    echo run_yr.bat not found. Copy scripts\run_yr.bat.example to scripts\run_yr.bat and set your game path.
)
