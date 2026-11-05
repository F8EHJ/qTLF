@echo off
rem ---------------------------------------------------------------------------
rem  qTLF - build and deploy on Windows (Qt 6, MinGW 64-bit).
rem
rem    deploy-windows.bat                              without the Hamlib DLLs
rem    deploy-windows.bat C:\hamlib-w64-4.5.5\bin      also copies libhamlib-4.dll & co
rem
rem  Can be started from any console (or by double-click): Qt, the MinGW compiler, CMake
rem  and Ninja are searched in the PATH, then in the default folders of the Qt installer
rem  (C:\Qt\6.x\mingw_64, C:\Qt\Tools\mingw*_64, C:\Qt\Tools\CMake_64, C:\Qt\Tools\Ninja).
rem  Qt is searched in X:\Qt on every drive; other folder:  set QT_ROOT=D:\MyQt  first.
rem
rem  Result: build-win\qtlf-deploy\  = qtlf.exe + Qt DLLs + translations + rules\
rem  (+ Hamlib DLLs). That folder runs on its own: zip it and copy it to any Windows PC.
rem ---------------------------------------------------------------------------
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "HAMLIB_BIN=%~1"
rem Qt folder: QT_ROOT if set, else \Qt on the first drive that has \Qt\Tools (C:, D:, G:...)
if not defined QT_ROOT for %%L in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do if not defined QT_ROOT if exist "%%L:\Qt\Tools\" set "QT_ROOT=%%L:\Qt"
if not defined QT_ROOT set "QT_ROOT=C:\Qt"

echo.
echo === qTLF Windows build ===
echo Qt root : %QT_ROOT%

rem ---- Hamlib folder given?
if defined HAMLIB_BIN if not exist "%HAMLIB_BIN%\libhamlib-4.dll" (
    echo ERROR: "%HAMLIB_BIN%\libhamlib-4.dll" not found: give the "bin" folder of hamlib-w64.
    goto :fail
)

rem ---- Qt 6 MinGW 64-bit (qmake.exe)
set "QTBIN="
for /f "delims=" %%Q in ('where qmake.exe 2^>nul') do if not defined QTBIN set "QTBIN=%%~dpQ"
if not defined QTBIN (
    for /f "delims=" %%D in ('dir /b /ad /o:n "%QT_ROOT%\6.*" 2^>nul') do if exist "%QT_ROOT%\%%D\mingw_64\bin\qmake.exe" set "QTBIN=%QT_ROOT%\%%D\mingw_64\bin\"
)
if not defined QTBIN (
    echo ERROR: Qt 6 for MinGW 64-bit not found ^(qmake.exe^).
    echo        Install it with the Qt online installer: Qt 6.x ^> MinGW 64-bit,
    echo        or set QT_ROOT to the folder that contains 6.x\mingw_64.
    goto :fail
)
set "QTPREFIX=%QTBIN%.."

rem ---- MinGW compiler: the one of the Qt installer first (avoids other gcc in the PATH)
set "MINGW="
for /f "delims=" %%D in ('dir /b /ad /o:n "%QT_ROOT%\Tools\mingw*_64" 2^>nul') do if exist "%QT_ROOT%\Tools\%%D\bin\g++.exe" set "MINGW=%QT_ROOT%\Tools\%%D\bin"
if not defined MINGW for /f "delims=" %%G in ('where g++.exe 2^>nul') do if not defined MINGW set "MINGW=%%~dpG"
if not defined MINGW (
    echo ERROR: MinGW compiler ^(g++.exe^) not found.
    echo        Qt installer ^> Developer and Designer Tools ^> MinGW 64-bit.
    goto :fail
)
if "%MINGW:~-1%"=="\" set "MINGW=%MINGW:~0,-1%"

rem ---- CMake
set "CMAKEBIN="
for /f "delims=" %%C in ('where cmake.exe 2^>nul') do if not defined CMAKEBIN set "CMAKEBIN=%%~dpC"
if not defined CMAKEBIN for /f "delims=" %%D in ('dir /b /ad /o:n "%QT_ROOT%\Tools\CMake*" 2^>nul') do if exist "%QT_ROOT%\Tools\%%D\bin\cmake.exe" set "CMAKEBIN=%QT_ROOT%\Tools\%%D\bin"
if not defined CMAKEBIN if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKEBIN=%ProgramFiles%\CMake\bin"
if not defined CMAKEBIN if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" set "CMAKEBIN=%ProgramFiles(x86)%\CMake\bin"
if not defined CMAKEBIN if exist "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe" set "CMAKEBIN=%LOCALAPPDATA%\Programs\CMake\bin"
if not defined CMAKEBIN for /f "delims=" %%V in ('dir /b /s "%ProgramFiles%\Microsoft Visual Studio\cmake.exe" 2^>nul ^| findstr /i "\\CMake\\bin\\cmake.exe"') do if not defined CMAKEBIN set "CMAKEBIN=%%~dpV"
if not defined CMAKEBIN (
    echo ERROR: CMake not found.
    echo        Install it ^(once^), then run this script again:
    echo          - %QT_ROOT%\MaintenanceTool.exe ^> Add or remove components ^> Qt ^>
    echo            Developer and Designer Tools ^> tick CMake and Ninja, or
    echo          - in a console: winget install Kitware.CMake Ninja-build.Ninja, or
    echo          - https://cmake.org/download ^(Windows x64 Installer, "Add CMake to the PATH"^)
    goto :fail
)
if "%CMAKEBIN:~-1%"=="\" set "CMAKEBIN=%CMAKEBIN:~0,-1%"

rem ---- Ninja (preferred), else mingw32-make
set "NINJABIN="
for /f "delims=" %%N in ('where ninja.exe 2^>nul') do if not defined NINJABIN set "NINJABIN=%%~dpN"
if not defined NINJABIN if exist "%QT_ROOT%\Tools\Ninja\ninja.exe" set "NINJABIN=%QT_ROOT%\Tools\Ninja"
if defined NINJABIN if "%NINJABIN:~-1%"=="\" set "NINJABIN=%NINJABIN:~0,-1%"

set "PATH=%QTBIN%;%MINGW%;%CMAKEBIN%;%PATH%"
if defined NINJABIN (
    set "PATH=%NINJABIN%;!PATH!"
    set GEN=-G Ninja
) else (
    rem MinGW Makefiles fails when an sh.exe (Git, MSYS) is in the PATH
    for /f "delims=" %%S in ('where sh.exe 2^>nul') do (
        echo ERROR: Ninja not found and "%%S" is in the PATH, which breaks "MinGW Makefiles".
        echo        Install Ninja ^(Qt installer ^> Developer and Designer Tools ^> Ninja^) and run again.
        goto :fail
    )
    set GEN=-G "MinGW Makefiles"
)

echo Qt       : %QTPREFIX%
echo Compiler : %MINGW%\g++.exe
echo CMake    : %CMAKEBIN%\cmake.exe
if defined NINJABIN (echo Generator: Ninja ^(!NINJABIN!^)) else (echo Generator: MinGW Makefiles)
if defined HAMLIB_BIN (echo Hamlib   : !HAMLIB_BIN!) else (echo Hamlib   : not copied ^(no argument^))
echo.

rem ---- a previous failed attempt (other generator / compiler) would block CMake: start clean
if exist build-win rmdir /s /q build-win

echo [1/3] Configure ...
cmake -S . -B build-win %GEN% -DCMAKE_BUILD_TYPE=Release ^
      "-DCMAKE_PREFIX_PATH=%QTPREFIX%" ^
      "-DCMAKE_C_COMPILER=%MINGW%\gcc.exe" "-DCMAKE_CXX_COMPILER=%MINGW%\g++.exe" ^
      "-DQTLF_HAMLIB_BIN=%HAMLIB_BIN%"
if errorlevel 1 (
    echo.
    echo ERROR in step 1 ^(configure^): read the CMake messages above.
    goto :fail
)

echo.
echo [2/3] Compile ...
cmake --build build-win --config Release
if errorlevel 1 (
    echo.
    echo ERROR in step 2 ^(compile^): read the first "error:" above.
    goto :fail
)

echo.
echo [3/3] Deploy ^(windeployqt^) ...
cmake --build build-win --config Release --target deploy_qtlf
if errorlevel 1 (
    echo.
    echo ERROR in step 3 ^(deploy^): windeployqt not found or failed, see above.
    goto :fail
)

echo.
echo Done: %~dp0build-win\qtlf-deploy\qtlf.exe
if not defined HAMLIB_BIN echo ^(no Hamlib DLLs copied: TRX control needs libhamlib-4.dll next to qtlf.exe^)
echo Zip the folder build-win\qtlf-deploy to distribute qTLF.
endlocal
pause
exit /b 0

:fail
echo.
echo Build failed. Copy the lines above and send them for help.
endlocal
pause
exit /b 1
