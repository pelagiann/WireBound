@echo off
setlocal

set SERVER=..\build\Server.exe
set CLIENT=..\build\Client.exe
set FILE=test.bin
set IP=127.0.0.1

if not exist %FILE% (
    echo ERROR: %FILE% not found. Place a test file in the benchmark\ folder.
    exit /b 1
)
if not exist logs mkdir logs

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
echo All sweeps complete. Logs in benchmark\logs\
goto :eof

:sweep
set CHUNK=%1
set N=%2
echo.
echo --- %CHUNK% KB chunk  x  %N% client(s) ---

start "" /min cmd /c "%SERVER% %FILE% %CHUNK% > logs\srv_%CHUNK%_%N%.txt 2>&1"
timeout /t 3 >nul

for /L %%i in (1,1,%N%) do (
    start "" /min cmd /c "%CLIENT% %IP% < nul > logs\cli_%CHUNK%_%N%_%%i.txt 2>&1"
)

timeout /t 2 >nul

:wait
tasklist /fi "imagename eq Client.exe" 2>nul | find "Client.exe" >nul
if not errorlevel 1 (timeout /t 1 >nul & goto wait)

taskkill /f /im Server.exe >nul 2>&1
timeout /t 2 >nul

for /L %%i in (1,1,%N%) do (
    echo   [Client %%i]:
    type logs\cli_%CHUNK%_%N%_%%i.txt | findstr /c:"MB/s" /c:"RESULT" /c:"FAIL"
)
echo   [Server]:
type logs\srv_%CHUNK%_%N%.txt | findstr /c:"Aggregate" /c:"Disk reads"
goto :eof
