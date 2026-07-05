@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo  从 AP 到 AI 网络 — 一键部署
echo ========================================
echo.

:: 设置代理（梯子）
set HTTP_PROXY=http://localhost:15236
set HTTPS_PROXY=http://localhost:15236

:: 确认提交信息
set /p msg="提交说明（直接回车则自动生成）: "
if "%msg%"=="" (
    for /f "tokens=1-3 delims=/- " %%a in ('echo %date%') do set d=%%a%%b%%c
    for /f "tokens=1-2 delims=: " %%a in ('echo %time%') do set t=%%a%%b
    set msg=update %d%
)

echo.
echo [1/3] 本地构建 Hugo ...
cd /d %~dp0
hugo >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Hugo 构建失败，请检查错误
    pause
    exit /b 1
)
echo      ✓ 构建完成

echo [2/3] 提交到 Git ...
git add -A
git commit -m "%msg%" >nul 2>&1
echo      ✓ 已提交

echo [3/3] 推送到 GitHub Pages ...
git push >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] 推送失败，可能是网络问题
    echo     尝试重试一次...
    timeout /t 3 >nul
    git push >nul 2>&1
    if !errorlevel! neq 0 (
        echo [!] 推送失败，请手动执行：
        echo     set HTTP_PROXY=http://localhost:15236
        echo     set HTTPS_PROXY=http://localhost:15236
        echo     git push
    ) else (
        echo      ✓ 推送成功
    )
) else (
    echo      ✓ 推送成功
)

echo.
echo ========================================
echo  发布完成！
echo  等待 1-2 分钟，博客将在以下地址更新：
echo  https://Jeffrey-Hu-171778195.github.io/
echo ========================================
echo.
pause