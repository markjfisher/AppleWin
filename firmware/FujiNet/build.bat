cl65.exe fujinet.s -C fujinet.cfg -o fujinet.bin
@IF %ERRORLEVEL% NEQ 0 goto error

copy fujinet.bin ..\..\resource
@goto end

:error
@echo "cl65 failed"

:end
