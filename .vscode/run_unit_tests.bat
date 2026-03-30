@echo off
setlocal

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Debug"

set "REPO_ROOT=%~dp0.."
set "TEST_DIR=%REPO_ROOT%\build\%BUILD_TYPE%\src\test"

call :run_test gaia_test.exe || exit /b 1
call :run_test gaia_test_no_autoreg.exe --test-case=*component registration* || exit /b 1

endlocal
exit /b 0

:run_test
set "TEST_BIN=%~1"
shift
echo ==^> %TEST_BIN%
call "%TEST_DIR%\%TEST_BIN%" %*
exit /b %ERRORLEVEL%
