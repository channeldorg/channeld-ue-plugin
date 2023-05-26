@echo off

if NOT "{Username}" == "" (
    type "{WorkDir}\TempRegistryPassword" | docker login {ChanneldRepoUrl} --username="{Username}" --password-stdin
    if ERRORLEVEL 1 (
        pause>nul
        Exit /b 1
    )
)

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