@echo off

set jqPath=%1
set podStatusJsonPath=%2
set podSelector=%3
set podReplicas=%4
set podDescriptionName=%5

set prevReason=""

kubectl get deploy %podDescriptionName% -o json | %jqPath% -r ".spec.template.spec.imagePullSecrets == null or .spec.template.spec.imagePullSecrets == []" -e > nul
set isImagePullSecretsEmpty=%ERRORLEVEL%
if %isImagePullSecretsEmpty% == 0 (
    echo WARNING: The imagePullSecrets of %podDescriptionName% is empty. If the docker image is in a private registry, the pod may not be able to pull the image.
)

:check

kubectl get pods -l %podSelector% -o=jsonpath="{.items}" > %podStatusJsonPath%
type %podStatusJsonPath% | %jqPath% -r ".[] | select(.status.phase == \"Running\")" -e > nul
set isPodPhaseRunning=%ERRORLEVEL%
type %podStatusJsonPath% | %jqPath% -r ".[] | .status.containerStatuses[0].state | has(\"running\")" -e > nul
set isContainerRunning=%ERRORLEVEL%
set readyCount=0
for /f "delims=" %%i in ('type %podStatusJsonPath% ^| %jqPath% -r ".[] | .status.conditions[] | (.type == \"Ready\" and .status == \"True\")" ^| find /c "true"') do (set /a readyCount=%%i)

if %isPodPhaseRunning% == 0 (
    if %isContainerRunning% == 0 (
        if %readyCount% == %podReplicas% (
            echo %podDescriptionName% pod^(s^) are ready
            EXIT /B 0
        )
    )
)

type %podStatusJsonPath% | %jqPath% -r ".[] | .status.containerStatuses[0].state.waiting | has(\"message\")" -e > nul
set hasMessage=%ERRORLEVEL%

if %hasMessage% == 0 (
    echo ERROR: Something wrong with %podDescriptionName% pod^(s^) >&2
    goto errorexit
)
goto recheck

:recheck
for /f "delims=" %%a in ('type %podStatusJsonPath% ^| %jqPath% -r ".[] | .status.containerStatuses[0].state.waiting.reason"') do set reason=%%a
if "%reason%" NEQ %prevReason% (
    if "%reason%" NEQ "" (
        if "%reason%" NEQ "null" (
            echo Waiting for %podDescriptionName% pod^(s^) to be ready: %reason%
            set prevReason="%reason%"
        )
    )
)
timeout /t 1 /nobreak 1>nul 2>&1
goto check

:errorexit
echo ERROR: Please check the pod status %podStatusJsonPath%>&2
EXIT /B 1
