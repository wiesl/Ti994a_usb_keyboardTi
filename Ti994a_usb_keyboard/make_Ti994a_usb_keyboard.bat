@echo off
rem Teensy 2.0 Make batch file

set PROJECT=TI994A USB KEYBOARD
set LOADER=D:\TeensyAvr\tools\teensy.exe
set HDLSTN=D:\TeensyAvr\tools\hid_listen.exe


title %PROJECT%
:START
echo off
cls
color 1E
echo.
echo.
echo.    %PROJECT%
echo.
echo.    Press
echo.
echo.     1 for Make release
echo.     2 for Make debug
echo.     3 for Clean all
echo.     4 for Start Teensy loader
echo.     5 for Start HID listen tool
echo.     6 for Exit
echo.
choice /C "123456" /N 

if %errorlevel%==1 goto :RELEASE 
if %errorlevel%==2 goto :DEBUG
if %errorlevel%==3 goto :CLEAN
if %errorlevel%==4 goto :LOADER
if %errorlevel%==5 goto :LISTEN
if %errorlevel%==6 goto :EXIT

:RETURN
goto :START


:RELEASE
cls
echo. MAKE RELEASE ...
call :MAKE FALSE
goto :RETURN

:DEBUG
cls
echo. MAKE DEBUG ...
call :MAKE TRUE
goto :RETURN

:CLEAN
call :MAKE TRUE  clean
call :MAKE FALSE clean
goto :RETURN

:LOADER
echo. Start %LOADER%
start %LOADER%
echo.
echo.    Make sure to select correct .HEX file first
echo.    before presseing Teensy reset button.
echo.
pause
goto :RETURN

:LISTEN
echo. Start %HDLSTN%
start %HDLSTN%
@choice /C "X" /N /T 2 /D X 
goto :RETURN

:MAKE
@echo on
make --makefile=makefile PROJECT_DBG=%1 %2
@if %errorlevel%==0 goto :MAKEOK
@echo.
@echo. ERRORS ...
@echo.
@pause
exit /B0

:MAKEOK:
@if [%2]==[clean] exit /B 0
@echo.
@echo. NO ERRORS ...
@echo.
@choice /C "X" /N /T 2 /D X 
exit /B 0

:EXIT
:EOF
