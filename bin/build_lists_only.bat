@echo off
bin\lists
if errorlevel 1 goto error
active.html
goto done

:error
echo ***********************************
echo ********** build failure **********
echo ***********************************

:done

