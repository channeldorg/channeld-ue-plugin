@echo off

cd "{WorkDir}"

:channeldpush
docker push {ChanneldTag} 2>docker_error.txt
if ERRORLEVEL 1 (
    type docker_error.txt
    findstr /i /c:"denied: requested access to the resource is denied" docker_error.txt
    if ERRORLEVEL 0 (
        echo Please login to the RepoUrl first.
        goto channeldlogin
    ) else (
        echo docker push failed.
        pause>nul
        Exit /b 1
    )
)
goto serverpush

:channeldlogin
echo Please login to {ChanneldRepoUrl} first.
docker login {ChanneldRepoUrl}
goto channeldpush

:serverpush
docker push {ServerTag} 2>docker_error.txt
if ERRORLEVEL 1 (
    type docker_error.txt
    findstr /i /c:"denied: requested access to the resource is denied" docker_error.txt
    if ERRORLEVEL 0 (
        echo Please login to the RepoUrl first.
        goto serverlogin
    ) else (
        echo docker push failed.
        pause>nul
        Exit /b 1
    )
)
Exit /b 0

:channeldlogin
echo Please login to {ServerRepoUrl} first.
docker login {ServerRepoUrl}
goto serverpush
