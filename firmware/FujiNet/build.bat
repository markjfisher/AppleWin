cl65.exe -t none fujinet.s -o fujinet.bin
@IF %ERRORLEVEL% NEQ 0 goto error

copy fujinet.bin ..\..\resource
@goto end

:error
@echo "cl65 failed"

:end
