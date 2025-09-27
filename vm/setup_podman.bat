@echo off
setlocal

set "imagename=gaiaecs-linux-builder"
set "imagename_tmp=%imagename%-tmp"

:: Stop and remove any existing container
podman stop -t 0 %imagename% >nul 2>&1
podman rm %imagename% >nul 2>&1

:: Detect platform architecture
set "currach=%PROCESSOR_ARCHITECTURE%"

:: Build image based on architecture
if "%currach%"=="ARM64" (
    :: Generic Dockerfile
    podman build --file Dockerfile --tag %imagename% .
) else if "%currach%"=="AMD64" (
    :: Intel-specific Dockerfile
    podman build --file amd64.Dockerfile --tag %imagename% .
) else if "%currach%"=="x86" (
    :: Intel-specific Dockerfile
    podman build --file amd64.Dockerfile --tag %imagename% .
) else (
    echo Unknown platform architecture: %currach%
    exit /b 1
)

:: Create volume if it doesn't exist
podman volume create %imagename_tmp% >nul 2>&1

:: Get current directory of the script
set "currdir=%~dp0"

:: Run the container
podman run -p 2022:22 --rm -it --privileged ^
    --name %imagename% ^
    --mount type=volume,source=%imagename_tmp%,target=/work-output ^
    --mount type=bind,source=%currdir%..,target=/gaia-ecs ^
    --workdir /gaia-ecs/docker ^
    %imagename% bash

pause
endlocal@echo off
setlocal

set "imagename=gaiaecs-linux-builder"
set "imagename_tmp=%imagename%-tmp"

:: Stop and remove any existing container
podman stop -t 0 %imagename% >nul 2>&1
podman rm %imagename% >nul 2>&1

:: Detect platform architecture
set "currach=%PROCESSOR_ARCHITECTURE%"

:: Build image based on architecture
if "%currach%"=="ARM64" (
    :: Generic Dockerfile
    podman build --file Dockerfile --tag %imagename% .
) else if "%currach%"=="AMD64" (
    :: Intel-specific Dockerfile
    podman build --file amd64.Dockerfile --tag %imagename% .
) else if "%currach%"=="x86" (
    :: Intel-specific Dockerfile
    podman build --file amd64.Dockerfile --tag %imagename% .
) else (
    echo Unknown platform architecture: %currach%
    exit /b 1
)

:: Create volume if it doesn't exist
podman volume create %imagename_tmp% >nul 2>&1

:: Get current directory of the script
set "currdir=%~dp0"

:: Run the container
podman run -p 2022:22 --rm -it --privileged ^
    --name %imagename% ^
    --mount type=volume,source=%imagename_tmp%,target=/work-output ^
    --mount type=bind,source=%currdir%..,target=/gaia-ecs ^
    --workdir /gaia-ecs/vm ^
    %imagename% bash

pause
endlocal