@echo off
setlocal

set SERVER=%~dp0..\build\Server.exe
set CLIENT=%~dp0..\build\Client.exe
set FILE=%~dp0test.bin
set IP=127.0.0.1

if not exist "%FILE%" (
    echo ERROR: test.bin not found in benchmark\
    echo Create it with: fsutil file createnew "%FILE%" 1073741824
    exit /b 1
)

if not exist "%~dp0logs" mkdir "%~dp0logs"
for /L %%i in (1,1,4) do (
    if not exist "%~dp0client%%i" mkdir "%~dp0client%%i"
)

echo ================================
echo  MaxxCast Benchmark Suite
echo ================================

call :sweep 256  1
call :sweep 256  2
call :sweep 256  4
call :sweep 256  8
call :sweep 1024 1
call :sweep 1024 2
call :sweep 1024 4
call :sweep 1024 8

echo.
echo All sweeps complete. See benchmark\logs\
goto :eof

:sweep
set CHUNK=%1
set N=%2
echo.
echo --- %CHUNK% KB chunk  x  %N% client(s) ---

start "" /min cmd /c "%SERVER% "%FILE%" %CHUNK% > "%~dp0logs\srv_%CHUNK%_%N%.txt" 2>&1"
timeout /t 15 >nul

for /L %%i in (1,1,%N%) do (
    start "" /D "%~dp0client%%i" /min cmd /c "%CLIENT% %IP% < nul > "%~dp0logs\cli_%CHUNK%_%N%_%%i.txt" 2>&1"
)

timeout /t 2 >nul

:wait
tasklist /fi "imagename eq Client.exe" 2>nul | find "Client.exe" >nul
if not errorlevel 1 (timeout /t 1 >nul & goto wait)

taskkill /f /im Server.exe >nul 2>&1
timeout /t 2 >nul

for /L %%i in (1,1,%N%) do (
    echo   [Client %%i]:
    type "%~dp0logs\cli_%CHUNK%_%N%_%%i.txt" | findstr /c:"MB/s" /c:"RESULT" /c:"FAIL"
)
echo   [Server]:
type "%~dp0logs\srv_%CHUNK%_%N%.txt" | findstr /c:"Aggregate" /c:"Disk reads"
goto :eof
