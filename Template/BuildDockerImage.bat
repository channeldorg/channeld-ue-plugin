@echo off

{BuildCmd}
:: Get the command line result
set BuildCmdResult=%ERRORLEVEL%
:: Pause the script if the build failed
if %BuildCmdResult% NEQ 0 (
    pause>nil
    EXIT /B %BuildCmdResult%
)

EXIT /B 0