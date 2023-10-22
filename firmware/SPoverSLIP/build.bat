cl65.exe -t none spoverslip.s -o spoverslip.bin
@IF %ERRORLEVEL% NEQ 0 goto error

copy spoverslip.bin ..\..\resource
@goto end

:error
@echo "cl65 failed"

:end
