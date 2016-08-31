@echo off

set projdir=w:\ani

if not exist %projdir%\build mkdir %projdir%\build
pushd %projdir%\build

::
:: Compiler flags
::

:: Preproc flags
:: -DINTERNAL_BUILD : is it debug build?
set cplflags=-DINTERNAL_BUILD=1

:: Optimization flags
:: -Oi : generate intrinsic funcs
:: -Od : disable optimization
set cplflags=%cplflags% -Oi -Od

:: Code gen flags
:: -fp:fast    : fast floating-point behavior
:: -Gm-        : disable minimal rebuild
:: -GR-        : disable run-time type info (RTTI)
:: -EHa-       : diasble exception handling
:: -MTd        : put multithreaded debug CRT into .obj
:: -Z7         : debug info format, put info into .obj
set cplflags=%cplflags% -fp:fast -Gm- -GR- -EHa- -MTd -Z7

:: Misc flags
:: -nologo : suppress the banner
:: -FC     : display full path for source codes
:: -WX     : treat warnings as errors
:: -W4     : level of warning
:: -wd4100 : unreferenced formal parameter
:: -wd4201 : nonstandard extension used: nameless struct/union
:: -wd4505 : unreferenced local function has been removed
:: -wd4189 : local variable is initialized but not referenced
:: -wd4101 : unreferenced local variable 
set cplflags=%cplflags% -nologo -FC -WX -W4 -wd4100 -wd4201 -wd4505 -wd4189 -wd4101

::
:: Linker flags
::

:: -nologo         : suppress the banner
:: -incremental:no : disable incremental linking
:: -opt:ref        : eliminate unreferenced funcs and data
set linkflags=-nologo -incremental:no -opt:ref

::
:: Build
::

cl %cplflags% %projdir%\code\win32_ani_platform.cpp -link %linkflags% user32.lib opengl32.lib gdi32.lib wsock32.lib

popd
