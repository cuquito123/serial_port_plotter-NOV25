@echo OFF

echo 0. WARNING
echo    ~~~~~~~
echo    windeployqt and iscc MUST be on PATH!!
echo.
echo 1. Defining variables
echo    ~~~~~~~~~~~~~~~~~~
set deps_folder=.\build\installer\deps
set exe_path=.\build\release\MPCC_CIOp.exe
echo    Dependencies folder = %deps_folder%
echo    Main EXE path = %exe_path%
echo.
echo 2. Creating dependencies folder
echo    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
windeployqt --dir %deps_folder% --compiler-runtime %exe_path%
echo.
echo 3. Compiling installer
echo    ~~~~~~~~~~~~~~~~~~~
iscc installer.iss
echo.
echo Finished!

pause