@echo off
set jqPath="{JQPath}"

kubectl apply -f "{YAMLFilePath}"

timeout /t 2 /nobreak 1>nul 2>&1
