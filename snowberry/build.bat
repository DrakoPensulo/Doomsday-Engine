@ECHO OFF
REM -- This batch file uses Py2exe to create a Windows executable.
REM -- A beta distribution package is compiled by compressing the
REM -- executable, the necessary resource files, and the Snowberry 
REM -- scripts into a self-extracting RAR archive.

REM ---- Python Interpreter
SET PYTHON_DIR=C:\Python26

rd/s/q dist

REM -- Make the executable.
"%PYTHON_DIR%"\python setup.py py2exe

REM -- Additional binary dependencies.
ECHO Copying dependencies to ./dist...
copy "%PYTHON_DIR%"\DLLs\MSVCP90.dll dist
copy "%PYTHON_DIR%"\DLLs\MSVCR90.dll dist
