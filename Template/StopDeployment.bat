@echo off
set contextName="{ContextName}"
set namespace="{Namespace}"

for /f "tokens=*" %%a in ('kubectl config current-context') do set currentContext=%%a

if not %currentContext% == %contextName% (
    kubectl config use-context %contextName%
    
    if %ERRORLEVEL% NEQ 0 (
        Exit /b 1
    )
)

kubectl delete deploy {Deployments} -n %namespace%
if %ERRORLEVEL% NEQ 0 (
    Exit /b 1
)