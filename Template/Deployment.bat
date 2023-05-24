@echo off
set jqPath="{JQPath}"

set contextName="{ContextName}"

for /f "tokens=*" %%a in ('kubectl config current-context') do set currentContext=%%a

if not %currentContext% == %contextName% (
    kubectl config use-context %contextName%

    if %ERRORLEVEL% NEQ 0 (
        Exit /b 1
    )
)

kubectl apply -f "{YAMLFilePath}"

if %ERRORLEVEL% NEQ 0 (
    Exit /b 1
)

timeout /t 2 /nobreak 1>nul 2>&1

{CheckPodStatusCommand}

kubectl get svc channeld-getaway -o jsonpath='{.status.loadBalancer.ingress[0].ip}' -n {Namespace} > "{ChanneldExternalIPFilePath}"
kubectl get svc channeld-grafana -o jsonpath='{.status.loadBalancer.ingress[0].ip}' -n {Namespace} > "{GrafanaExternalIPFilePath}"

echo Channeld External IP: 
type "{ChanneldExternalIPFilePath}"