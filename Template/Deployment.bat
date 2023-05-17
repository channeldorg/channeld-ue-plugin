@echo off
cd "{WorkDir}"

kubectl apply -f "{YAMLFilePath}"

timeout /t 2 /nobreak 1>nul 2>&1

call "{CheckPodStatusBatPath}"

if ERRORLEVEL 1 (
    Exit /b 1
)