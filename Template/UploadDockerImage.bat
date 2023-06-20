@echo off

if NOT "{Username}" == "" (
    type "{WorkDir}\TempRegistryPassword" | docker login {ChanneldRepoUrl} --username="{Username}" --password-stdin
    if ERRORLEVEL 1 (
        pause>nul
        Exit /b 1
    )
)

:channeldpush
docker push {ChanneldTag} 2>docker_error.txt
if ERRORLEVEL 1 (
    findstr /i /c:"denied: requested access to the resource is denied" docker_error.txt
    if ERRORLEVEL 0 (
        goto channeldlogin
    ) else (
        echo docker push failed.
        pause>nul
        Exit /b 1
    )
)
goto serverpush

:channeldlogin
if "{ChanneldRepoUrl}" == "" (
    echo Please login to dockerhub.
) else (
    echo Please login to {ChanneldRepoUrl}.
)

docker login {ChanneldRepoUrl}
goto channeldpush

:serverpush
docker push {ServerTag} 2>docker_error.txt
if ERRORLEVEL 1 (
    findstr /i /c:"denied: requested access to the resource is denied" docker_error.txt
    if ERRORLEVEL 0 (
        goto serverlogin
    ) else (
        echo docker push failed.
        pause>nul
        Exit /b 1
    )
)
Exit /b 0

:channeldlogin
if "{ServerRepoUrl}" == "" (
    echo Please login to dockerhub.
) else (
    echo Please login to {ServerRepoUrl}.
)
docker login {ServerRepoUrl}
goto serverpush
