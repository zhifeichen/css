css4.0
======

**css4.0 for nvmp3.5+.** 
use libuv,mxml,mysql.
all of css4.0 in [jira](http://192.168.203.103:8080/browse/CSS-2).

---------



opensource libs
----------

 1. **libuv** 
    An introduction to [libuv](http://nikhilm.github.io/uvbook/index.html).
 2. **mxml**
    used to compare xml. An introduction to [mxml](http://www.msweet.org/documentation/project3/Mini-XML.html).
    version **2.8**.
 3. **MySQL Connector/C**
    use mysql c client api.
    version **6.0**.

----------


gits
----------
1. **css4.0**   [css4.0.git](git@192.168.203.211:storageServer4.0.git)
2. **libuv**    [libuv.git](git@192.168.203.211:libuv.git)
3. **mxml**     [mxml.git](git@192.168.203.211:mxml)
4. **MySQL Connector/C 6.0**    [MySQL Connector/C 6.0.git](git@192.168.203.211:libmysql.git)


----------


dirs
---------

 - **`$CSS_HOME/bin`**
    all binary file of css4.0.

 - **`$CSS_HOME/deps`**
    all libs of css4.0 dependent.

     > `$CSS_HOME/deps/lib_uv/` *all source files of lib_uv.*

 - **`$CSS_HOME/src`**
    all source file like xxx.c and xxx.h of css4.0.

 - **`$CSS_HOME/test`**
    all unit tests of css4.0.  

 - **`$CSS_HOME/project`**
    windows and linux project files.
    compile,test,release of css4.0.

    >`$CSS_HOME/project/vs` *windows vs procect.Visual Studio 2010.*
    >`$CSS_HOME/project/linux` *linux makefile.*
    >`$CSS_HOME/project/mac_os_x` *mac os x makefile.*
    
    
    


----------


Build Instructions
----------

####1. **linux**
first compile: **`$make get_deps&&$make compile_all`**
other compile:  **`$make compile`** or **`$make compile_test_css`** for test.
all make cmd here:

 - **`$make get_deps`**         get all dependents.
 - **`$make compile_mxml`**     compile mxml independent.
 - **`$make compile_uv`**       compile libuv independent.
 - **`$make compile_css`**      compile css4.0 source files independent without test.
 - **`$make compile_test_css`** compile css4.0 source files independent with test.
 - **`$make compile_all`**      compile all deps and compile css4.0 source file without test.
 - **`$make`**                  like `$make compile_all`
 - **`$make clean`**            clean css4.0 binary file.
 - **`$make clean_all`**        clean all css4.0 binary and dependents's file.
    
####2. **windows**
you should install git,python 2.7 and vs2010,cmake first and add to PATH.
first compile: **`build.bat get_deps && build.bat compile_css`**
default to build for WIN64,you can use x86 to build for WIN32,but mongoc don't have WIN32 version!!

all cmd here:

 - **`build.bat get_deps`**	    get all dependents.
 - **`build.bat compile_mxml`**     compile mxml independent for WIN64.
 - **`build.bat compile_uv`**       compile libuv independent for WIN64
 - **`build.bat compile_css`**      compile css4.0 source files independent for WIN64 without test.
 - **`build.bat compile_test_css`** compile css4.0 source files independent for WIN64 with test.
 - **`build.bat compile_all`**      compile all deps and compile css4.0 source file for WIN64 without test.
 - **`build.bat`**                  like `build.bat compile_all`
 - **`build.bat clean`**            clean css4.0 binary file.
 - **`build.bat clean_all`**        clean all css4.0 binary and dependents's file.
 - **`build.bat compile_mxml x86`** compile mxml independent for WIN32.
 
 
 


Supported Platforms
---------------

Microsoft Windows operating systems since Windows XP SP2. It can be built
with either Visual Studio or MinGW. Consider using
Visual Studio Express 2010 or later if you do not have a full Visual
Studio license.

Linux using the GCC toolchain.

OS X using the GCC or XCode toolchain.

Patches
-------------

 - [node.js](http://nodejs.org/)
 - [GYP](http://code.google.com/p/gyp/)
 - [Python](https://www.python.org/downloads/)
 - [Visual Studio Express 2010]( http://www.microsoft.com/visualstudio/eng/products/visual-studio-2010-express)
 - [mxml](http://www.msweet.org/projects.php?Z3)
 - [mysql](http://www.mysql.com)
 - [uv](https://github.com/joyent/libuv)
 - [cmake](http://www.cmake.org/cmake/resources/software.html)