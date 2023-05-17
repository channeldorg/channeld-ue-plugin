@echo off
cd "D:/channeld-ue-demos/Intermediate/ChanneldClouldDeployment"
set jqPath="D:\channeld-ue-demos\Plugins\ChanneldUE/Source/ThirdParty/jq-win64.exe"
set podStatusJsonPath="D:/channeld-ue-demos/Intermediate/ChanneldClouldDeployment/ChanneldPodStatus.json"

set prevReason=null

:check

kubectl get pods -l app=channeld-getaway -o=jsonpath="{.items[*].status}" > %podStatusJsonPath%

type %podStatusJsonPath% | %jqPath% -r ".phase" | findstr /i /x "Running" > nul
set isPodPhaseRunning=%ERRORLEVEL%
type %podStatusJsonPath% | %jqPath% -r ".containerStatuses[0].state | has(\"running\")" -e > nul
set isContainerRunning=%ERRORLEVEL%

if %isPodPhaseRunning% == 0 (
    if %isContainerRunning% == 0 (
        echo channeld pod is running
        EXIT /B 0
    )
)

type %podStatusJsonPath% | %jqPath% -r ".phase" | findstr /i /x "Pending" > nul
set isPodPhasePending=%ERRORLEVEL%
type %podStatusJsonPath% | %jqPath% -r ".containerStatuses[0].state.waiting | has(\"message\")" -e > nul
set hasMessage=%ERRORLEVEL%

if %isPodPhasePending% == 0 (
    if %hasMessage% == 0 (
        echo ERROR: Something wrong with channeld pod: >&2
        type %podStatusJsonPath% | %jqPath% -r ".containerStatuses[0].state.waiting" >&2 
        EXIT /B 1
    )
    goto recheck
) else (
    echo ERROR: channeld pod is not deployed >&2
    type %podStatusJsonPath% | %jqPath% -r ".containerStatuses[0].state" >&2
    EXIT /B 1
)

:recheck
for /f "delims=" %%a in ('type %podStatusJsonPath% ^| %jqPath% -r ".containerStatuses[0].state.waiting.reason"') do set reason=%%a
if not %reason% == %prevReason% (
    if not %reason% == null (
        echo Waiting for channeld pod to be ready: %reason%
        set prevReason=%reason%
    )
)
timeout /t 1 /nobreak 1>nul 2>&1
goto check

