@echo off
rem
rem http://cdetect.sourceforge.net/
rem
rem $Id: configure.bat,v 1.1 2007/02/18 11:19:04 breese Exp $
rem
rem Copyright (C) 2005-2007 Bjorn Reese <breese@users.sourceforge.net>
rem
rem Permission to use, copy, modify, and distribute this software for any
rem purpose with or without fee is hereby granted, provided that the above
rem copyright notice and this permission notice appear in all copies.
rem
rem THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
rem WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
rem MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND
rem CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.
rem

set SOURCE=config.c
set COMMAND=config.exe
set ARGUMENTS=
set DEPEND=%COMMAND% %SOURCE% cdetect\*.c
set COMPILER=cl.exe
set UPDATE=no
set ISUPDATED=no
set CFLAGS=/nologo

if x%1==x--debug (
    set CFLAGS=%CFLAGS% /Zi
    shift /1
)

rem Build command-line arguments
:argloop
set ARGUMENTS=%ARGUMENTS% %1
shift /1
if not x%1==x goto argloop

rem Determine dependencies
if exist %COMMAND% (
    for /f %%f in ('dir /b /o-d %DEPEND%') do call :UpdateIfNew %%f
) else (
    set UPDATE=yes
)

rem Build cDetect executable if needed
if %UPDATE% == yes (
    echo Updating %COMMAND%
    del %COMMAND%
    %COMPILER% %CFLAGS% %SOURCE%
)

rem Execute cDetect if it exists
if exist %COMMAND% (
    %COMMAND% %ARGUMENTS%
) else (
    echo Error: %COMMAND% not found
    exit /b 1
)

goto :eof

rem Functions
:UpdateIfNew
if %ISUPDATED%==yes goto :eof
if not x%1==x%COMMAND% set UPDATE=yes
set ISUPDATED=yes
goto :eof
