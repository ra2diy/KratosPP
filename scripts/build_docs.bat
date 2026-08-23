@if not defined _echo echo off

rem Builds Kratos docs.
rem The docs are plain Markdown under ..\docs. If pandoc is available, an HTML
rem bundle is generated; otherwise the Markdown files are verified to exist.

cd /D "%~dp0"
cd ..\docs

if not exist 设计文档.md goto :missing
if not exist 开发文档.md goto :missing
if not exist 使用文档.md goto :missing

where pandoc >nul 2>nul
if %errorlevel%==0 (
    pandoc 设计文档.md 开发文档.md 使用文档.md -o Kratos-Docs.html --toc
    echo Docs built: docs\Kratos-Docs.html
) else (
    echo docs are plain Markdown - no build required. Install pandoc to generate an HTML bundle.
)
exit /b 0

:missing
echo Missing docs\*.md files.
exit /b 1
