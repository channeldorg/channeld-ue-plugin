@echo off
SET ChanneldVersion=v0.4.0
SET ChanneldRepoUrl=git@github.com:metaworking/channeld.git
SET WorkspaceDir=%~dp0
SET ChanneldLocalSourceDir=%~dp0Source\ThirdParty\channeld

SET ErrorMessages=run %~dp0%~n0.bat manually

where git
if NOT %ERRORLEVEL% == 0 (
    echo ERROR: Please install git[https://git-scm.com/downloads] first and %ErrorMessages%
    exit /b 1
)
where go
if NOT %ERRORLEVEL% == 0 (
    echo ERROR: Please install golang[https://go.dev/dl/] first and run %ErrorMessages%
    exit /b 2
)

:: Clone channeld from github
if NOT EXIST "%ChanneldLocalSourceDir%\.git" (
    git -c advice.detachedHead=false clone --branch %ChanneldVersion% %ChanneldRepoUrl% "%ChanneldLocalSourceDir%"
    if NOT %ERRORLEVEL% == 0 (
        echo ERROR: Clone channel:%ChanneldVersion% failed, please clone channeld[%ChanneldRepoUrl%]:%ChanneldVersion% manually and %ErrorMessages%.
        exit /b 3
    )
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
    echo If you are not running the script via cmd.exe, please restart your shell before update ChanneldUE source code !!!
)

cd "%ChanneldLocalSourceDir%"
SET GOPROXY=https://goproxy.io,direct
go mod download -x
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.28
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.2
cd "%WorkspaceDir%"

echo Setup Completed
