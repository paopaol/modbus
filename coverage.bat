set SOURCE_DIR=%cd%
OpenCppCoverage.exe --source=%SOURCE_DIR% --excluded_sources=%SOURCE_DIR%\thrd   --excluded_sources=%SOURCE_DIR%\build %SOURCE_DIR%\build\test\Debug\modbus_test.exe