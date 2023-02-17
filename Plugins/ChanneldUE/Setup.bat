@echo off
SET ChanneldVersion=v0.4.0
SET ChanneldRepoUrl=git@github.com:metaworking/channeld.git
SET WorkspaceDir=%~dp0
SET ChanneldLocalSourceDir=%~dp0Source\ThirdParty\channeld

SET ErrorMessages=run %~dp0%~n0.bat again

echo Start setup

echo checking git...
where git > null
if NOT %ERRORLEVEL% == 0 (
    echo ERROR: Please install git[https://git-scm.com/downloads] first and %ErrorMessages%.
    exit /b 1
)
echo 'git' is installed.

echo Checking golang...
where go > null
if %ERRORLEVEL% == 0 (
    echo 'golang' is installed.
    goto SkipInstallGo
)
:prompt
    : If go is not installed, prompt to install golang.
    set /p installGolang='channel' runs with golang, do you want to install golang now? [y/n]
    if "%installGolang%" == "y" (
        goto downloadGo
    ) else if "%installGolang%" == "n" (
        goto cannelInstallGo
    ) else (
        goto prompt
    )
:downloadGo
    set golangVersion=1.18.10
    set golangOS=windows
    set golangArch=amd64
    set golangDownloadUrl=https://golang.org/dl/go%golangVersion%.%golangOS%-%golangArch%.msi
    set golangDownloadPath=%TEMP%\go%golangVersion%.%golangOS%-%golangArch%.msi
    echo Try to dowanload golang installer from %golangDownloadUrl%.
    echo Downloading...
    powershell -Command "(New-Object System.Net.WebClient).DownloadFile('%golangDownloadUrl%', '%golangDownloadPath%')"
    if %ERRORLEVEL% == 0 (
        goto installGo
    )
    : If dowanload failed, try to use https://golang.google.cn/dl/ to download the golang installer.
    set golangDownloadUrl=https://golang.google.cn/dl/go%golangVersion%.%golangOS%-%golangArch%.msi
    echo Download failed, try to download golang installer from %golangDownloadUrl%.
    echo Downloading...
    powershell -Command "(New-Object System.Net.WebClient).DownloadFile('%golangDownloadUrl%', '%golangDownloadPath%')"
    if %ERRORLEVEL% == 0 (
        goto installGo
    )
    : If dowanload failed, exit the script.
    echo Download golang installer failed.
    goto cannelInstallGo
:installGo
    echo Installing golang...
    msiexec /i "%golangDownloadPath%" /passive /qr /norestart /log "%TEMP%\golang_install.log"
    if NOT %ERRORLEVEL% == 0 (
        echo Download golang install failed.
        goto cannelInstallGo
    )
    goto installGoFinfished
:cannelInstallGo
    echo ERROR: Please install Golang[https://go.dev/dl/] first and %ErrorMessages%.
    exit /b 2
:installGoFinfished
    echo Installing Golang is complete.

:SkipInstallGo

:: Clone channeld from github
if NOT EXIST "%ChanneldLocalSourceDir%\.git" (
    git -c advice.detachedHead=false clone --branch %ChanneldVersion% %ChanneldRepoUrl% "%ChanneldLocalSourceDir%"
    if NOT %ERRORLEVEL% == 0 (
        echo ERROR: Clone channel:%ChanneldVersion% failed, please clone channeld[%ChanneldRepoUrl%]:%ChanneldVersion% manually and %ErrorMessages%.
        exit /b 3
    )
    : If clone failed with internal error like "unable to access", the ERRORLEVEL still be 0, so we need to check the .git dir.
    if NOT EXIST "%ChanneldLocalSourceDir%\.git"
    (
        echo ERROR: Clone channel:%ChanneldVersion% failed, please clone channeld[%ChanneldRepoUrl%]:%ChanneldVersion% manually and %ErrorMessages%.
        exit /b 3
    )
 
    echo Clone channeld:%ChanneldVersion% successfully, the source code is at %ChanneldLocalSourceDir%
)

echo Set post-merge hook to .git\hooks

SET PostMergeHook=%~dp0..\..\.git\hooks\post-merge
:: SET PostMergeHook=%~dp0.git\hooks\post-merge
echo #!/bin/sh > "%PostMergeHook%"
echo "$(cd $(dirname ${BASH_SOURCE[0]}); pwd)/../../Plugins/ChanneldUE/Source/ThirdParty/update_channeld.sh" >> "%PostMergeHook%"
:: echo "$(cd $(dirname ${BASH_SOURCE[0]}); pwd)/Source/ThirdParty/update_channeld.sh" > "%PostMergeHook%"
echo exit 0 >> "%PostMergeHook%"

:: Set CHANNELD_PATH user env to channeld source dir when the CHANNELD_PATH is not set
if NOT DEFINED CHANNELD_PATH (
    echo Set CHANNELD_PATH to %ChanneldLocalSourceDir%
    setx CHANNELD_PATH %ChanneldLocalSourceDir%
    :: invoke refreshenv.bat to make the CHANNELD_PATH take effect
    call "%~dp0Source\ThirdParty\refrenv.bat"
    echo Set user environment variable CHANNELD_PATH to %ChanneldLocalSourceDir%
    echo If you are not running the script via cmd.exe, please restart your shell before update ChanneldUE source code !!!
) else (
    echo CHANNELD_PATH is already set to %CHANNELD_PATH%
)

cd "%ChanneldLocalSourceDir%"
SET GOPROXY=https://goproxy.io,direct
go mod download -x
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.28
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.2
cd "%WorkspaceDir%"

echo Setup Completed
