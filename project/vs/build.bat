@rem for windows compile


@echo off
set CSS_APP_NAME=css4.0
set PWD=%CD%
set ROOT_DIR=%PWD%\..\..\
set EXTLIB=%ROOT_DIR%\extlib\


@rem binary dir
set BIN_DIR=%ROOT_DIR%bin\
@rem include dir
set INCLUDE_DIR=%ROOT_DIR%include\
@rem src dir
set SRC_DIR=%ROOT_DIR%src\

set DEPS_ROOT_DIRS=%ROOT_DIR%deps\
@rem --------------------------------------------------------

@rem libuv && gyp.
set LIBUV_NAME_APP=uv
set LIBUV_GIT_ADDR=git@192.168.203.211:libuv.git
set LIBUV_NAME=lib%LIBUV_NAME_APP%
set LIBUV_SOURCE_DIR=%DEPS_ROOT_DIRS%%LIBUV_NAME%\
set LIBUV_SOURCE_BUILD_DIR=%LIBUV_SOURCE_DIR%build\
set LIBUV_INCLUDE_SOURCE_DIR=%LIBUV_SOURCE_DIR%include\
set LIBUV_INCLUDE_DIR=%INCLUDE_DIR%%LIBUV_NAME_APP%\

set GYP_NAME_APP=gyp
set GYP_GIT_ADDR=git@192.168.203.211:gyp.git
set GYP_SOURCE_DEPS_DIR=%DEPS_ROOT_DIRS%%GYP_NAME_APP%
@rem --------------------------------------------------------

@rem mysql connector\c API
set LIBMYSQL_NAME_APP=mysql
set LIBMYSQL_GIT_ADDR=git@192.168.203.211:libmysql.git
set LIBMYSQL_NAME=lib%LIBMYSQL_NAME_APP%
set LIBMYSQL_SOURCE_DIR=%DEPS_ROOT_DIRS%%LIBMYSQL_NAME%\
set LIBMYSQL_BUILD_DIR=%LIBMYSQL_SOURCE_DIR%build\
set LIBMYSQL_INCLUDE_SOURCE_DIR=%LIBMYSQL_SOURCE_DIR%include\
set LIBMYSQL_INCLUDE_DIR=%INCLUDE_DIR%%LIBMYSQL_NAME_APP%\
@rem ----------------------------------

@rem mongo connector\c API
set LIBMONGO_NAME_APP=mongo
set LIBMONGO_GIT_ADDR=git@192.168.203.211:libmongo-c-driver
set LIBMONGO_NAME=lib%LIBMONGO_NAME_APP%
set LIBMONGO_SOURCE_DIR=%DEPS_ROOT_DIRS%%LIBMONGO_NAME%\
set LIBMONGO_BUILD_DIR=%LIBMONGO_SOURCE_DIR%build\
set LIBMONGO_INCLUDE_SOURCE_DIR=%LIBMONGO_SOURCE_DIR%include\
set LIBMONGO_INCLUDE_DIR=%INCLUDE_DIR%%LIBMONGO_NAME_APP%\
@rem ----------------------------------

@rem bson
set LIBBSON_NAME_APP=bson
set LIBBSON_GIT_ADDR=git@192.168.203.211:libbson
set LIBBSON_NAME=lib%LIBBSON_NAME_APP%
set LIBBSON_SOURCE_DIR=%DEPS_ROOT_DIRS%%LIBBSON_NAME%\
set LIBBSON_BUILD_DIR=%LIBBSON_SOURCE_DIR%build\
set LIBBSON_INCLUDE_SOURCE_DIR=%LIBBSON_SOURCE_DIR%include\
set LIBBSON_INCLUDE_DIR=%INCLUDE_DIR%%LIBBSON_NAME_APP%\
@rem ----------------------------------

@rem mxml
set LIBMXML_NAME_APP=mxml
set LIBMXML_GIT_ADDR=git@192.168.203.211:mxml
set LIBMXML_NAME=%LIBMXML_NAME_APP%
set LIBMXML_SOURCE_DIR=%DEPS_ROOT_DIRS%%LIBMXML_NAME%\
set LIBMXML_SLN_DIR=%LIBMXML_SOURCE_DIR%\vcnet\
set LIBMXML_BUILD_DIR=%LIBMXML_SOURCE_DIR%build\
set LIBMXML_INCLUDE_DIR=%INCLUDE_DIR%%LIBMXML_NAME_APP%
@rem -----------------------------------------------------------

@rem dependent.

if "%2"=="x86" (
	set VS_VERSION="Visual Studio 10 2010"
	set UV_SYS_VERSION=x86
	set SYS_VERSION=Win32
) else (
	set VS_VERSION="Visual Studio 10 2010 Win64"
	set UV_SYS_VERSION=x64
	set SYS_VERSION=x64
)

set ALL_DEPS_LIBS=(%LIBUV_GIT_ADDR%,%GYP_GIT_ADDR%,%LIBMYSQL_GIT_ADDR%,%LIBMXML_GIT_ADDR%)

if "%1"=="" (
  goto all
) else (
  goto %1
)

:compile_bson
   echo compile bson lib...
   cd %LIBBSON_SOURCE_DIR%
   del /S/Q CMakeCache.txt
   cmake -DCMAKE_INSTALL_PREFIX=%LIBBSON_SOURCE_DIR%\build -G %VS_VERSION% .
   devenv %LIBBSON_SOURCE_DIR%\libbson.sln /build release /project INSTALL
   copy "%LIBBSON_BUILD_DIR%bin\libbson*.dll" "%BIN_DIR%"
   copy "%LIBBSON_BUILD_DIR%lib\bson*.lib" "%EXTLIB%\libbson\"
   xcopy /e/h/i/q/y %LIBBSON_BUILD_DIR%include\libbson-1.0\*.h %INCLUDE_DIR%\libbson\
   goto end

:compile_mongo
   echo compile mongo connector lib...
   cd %LIBMONGO_SOURCE_DIR%
   cmake -DCMAKE_INSTALL_PREFIX=%LIBMONGO_SOURCE_DIR%\build -DBSON_ROOT_DIR=%LIBBSON_SOURCE_DIR%\build -G %VS_VERSION% .
   devenv %LIBMONGO_SOURCE_DIR%\libmongoc.sln /build release /project INSTALL
   copy "%LIBMONGO_BUILD_DIR%bin\libmongoc*.dll" "%BIN_DIR%"
   copy "%LIBMONGO_BUILD_DIR%lib\mongoc*.lib" "%EXTLIB%\libmongo\"
   xcopy /e/h/i/q/y %LIBMONGO_BUILD_DIR%include\libmongoc-1.0\*.h %INCLUDE_DIR%\libmongo\
   goto end

:compile_mxml
   echo compile mxml lib...
   devenv %LIBMXML_SLN_DIR%\mxml.sln /upgrade
   devenv %LIBMXML_SLN_DIR%\mxml.sln /build "release|%SYS_VERSION%" /project mxml1
   copy "%LIBMXML_SLN_DIR%\mxml1.dll" "%BIN_DIR%"
   copy "%LIBMXML_SLN_DIR%\mxml1.lib" "%EXTLIB%\mxml\"
   xcopy /h/i/q/y %LIBMXML_SOURCE_DIR%mxml.h %INCLUDE_DIR%\mxml\
   goto end

:compile_uv
   echo compile uv lib ...
   call %LIBUV_SOURCE_DIR%\vcbuild.bat release %UV_SYS_VERSION% shared nobuild
   devenv %LIBUV_SOURCE_DIR%\uv.sln /build release /project libuv
   copy "%LIBUV_SOURCE_DIR%\Release\libuv.dll" "%BIN_DIR%"
   copy "%LIBUV_SOURCE_DIR%\Release\libuv.lib" "%EXTLIB%\libuv\"
   xcopy /e/h/i/q/y %LIBUV_SOURCE_DIR%\include %INCLUDE_DIR%\uv
   goto end

:compile_mysql
   echo compile mysql connector lib...
   cd %LIBMYSQL_SOURCE_DIR%
   cmake -G %VS_VERSION%
   devenv.com %LIBMYSQL_SOURCE_DIR%\libmysql.sln /build Release /project libmysql
   copy "%LIBMYSQL_SOURCE_DIR%\libmysql\Release\libmysql.dll" "%BIN_DIR%"
   copy "%LIBMYSQL_SOURCE_DIR%\libmysql\Release\libmysql.lib" "%EXTLIB%\libmysql\"
   xcopy /e/h/i/q/y %LIBMYSQL_SOURCE_DIR%\include %INCLUDE_DIR%\mysql
   goto end

:compile_css
   echo compile css4.0...
   devenv storageServer.sln /build "release|%SYS_VERSION%" /project storageServer
   goto end

:compile_test_css
   echo compile test css4.0...
   devenv storageServer.sln /build debug /project storageServer
   goto end

:all
   echo start css4.0 buid all...
   goto end

:get_deps
    echo clean all deps dir...
@rem    rd %DEPS_ROOT_DIRS% /S/Q
    cd %ROOT_DIR% && git submodule init & git submodule update
    mkdir %LIBUV_SOURCE_BUILD_DIR%
    xcopy /e/h/i/q %GYP_SOURCE_DEPS_DIR% %LIBUV_SOURCE_BUILD_DIR%\%GYP_NAME_APP%
    goto end

:compile_all:
	echo compile all of css4.0 ...
	call build.bat compile_uv %2
	call build.bat compile_mysql %2
	call build.bat compile_mxml %2
	call build.bat compile_bson %2
	call build.bat compile_mongo %2
	call build.bat compile_css %2

:clean_all:
	echo clean all css4.0 ...
	rd /S/Q $(BIN_DIR)*
	
:clean:
	echo clean css4.0 ...
	rd /S/Q $(BIN_DIR)css*
	
:end
    cd %PWD%
    echo make successful.

@rem @pause
