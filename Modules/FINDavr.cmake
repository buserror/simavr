# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FINDavr
# -------
#
# Finds the avr tools and compilers required to build simavr 
#
#This will define the following variables::
#
#   avr_FOUND    - True if the system has the Foo library
#   avr_VERSION  - The version of the Foo library which was found
#   avr_VERSION_MAJOR  - The major version of the avr libc library found
#   avr_VERSION_MINOR  - The minor version of the avr libc library found
#   avr_VERSION_PATCH  - The patch number of the avr libc library found
#   avr_VERSION_POSTFIX  - Any additional postfix added to the version number of the avr libc library found
#   avr_AVR_ROOT - The root directory of the avr tool chain
#   avr_AVR_INC - The directory where libsimavr library is located
#   avr_AVR_TOOL_PREFIX - The tools prefix
#   avr_LDFLAGS - avr installation specific LDFLAGS
#   avr_CFLAGS - avr installation specific CFLAGS
#   avr_CPPFLAGS - the cpp flags required to build simavr
#   avr_INCLUDE_PATHS - the include paths specific to avr

if (CMAKE_HOST_SYSTEM_NAME EQUAL "Linux")
    find_path(avr_AVR_ROOT 
       NAMES "include/avr/version.h" "avr" "avr/bin/ranlib" "avr/bin/ld"
       PATHS "/usr/" "/opt/" "/opt/cross" "/opt/cross" "/usr/local"
       PATH_SUFFIXES "lib" "avr" "avr/lib" "lib/avr" "lib32" "lib32/avr" "lib64" "lib64/avr" "libx32" "libx32/avr"
       DOC "AVR_ROOT directory containing avr specific includes and libraries"
    )
    if( avr_AVR_ROOT_NOTFOUND )
        message(FATAL_ERROR "Please install avr-gcc avr-libc and related packages")
    endif
    set(avr_AVR_INC 
        "${avr_AVR_ROOT}"
    )
    set(avr_AVR_TOOL_PREFIX 
       "avr-"
       CACHE STRING "prefix prepended to any avr specific tool"
    )
    set( avr_CFLAGS 
        ""
    ) 
    set(avr_LDFLAGS
        ""
    )
    set(avr_INCLUDE_PATHS
        ""
    )
elseif (CMAKE_HOST_SYSTEM_NAME EQUAL "Windows")
    message(FATAL_ERROR "avr on windows not yet supported")
elseif (CMAKE_HOST_SYSTEM_NAME EQUAL "Darwin")
    find_path(avr_AVR_ROOT
        NAMES "avr" "avr-gcc" "bin/avr-gcc" "avr-libc"
        PATHS "/Applications/Arduino.app/Contents/Resources/Java/hardware/tools/avr/" "/usr/local" "/usr/" "/opt/" "/opt/cross" "/opt/cross" "/usr/local"
        PATH_SUFFIXES "Cellar" 
    )
    if( avr_AVR_ROOT_NOTFOUND )
        message(FATAL_ERROR "Please install avr-gcc avr-libc and related packages")
    endif
    file(GLOB avr_AVR_ROOT_in_avr_libc 
        LIST_DIRECTORIES True
        "${avr_AVR_ROOT}/avr-libc/*/"
    )
    list(LENGTH avr_AVR_ROOT_in_avr_libc avr_AVR_ROOT_in_avr_libc_length)
    if ( avr_AVR_ROOT_in_avr_libc_length GREATER 0 )
        set(avr_AVR_ROOT 
            list(GET avr_AVR_ROOT_in_avr_libc 0) 
            CACHE PATH "avr root directory containing avr specific includes and libraries"
        )
        set(avr_AVR_INC 
            "${avr_AVR_ROOT}/avr"
        )
        set(avr_AVR_TOOL_PREFIX 
            "avr-"
        )
        string(REGEX REPLACE "/+" ";" avr_AVR_ROOT_DIRS "${avr_AVR_ROOT}")
        list(FIND avr_AVR_ROOT_DIRS "Cellar" avr_AVR_ROOT_CELLAR)
        if ( avr_AVR_ROOT_CELLAR LESS 0 )
            message(FATAL_ERROR "Please install avr-gcc avr-libc and related packages or install avr-gcc: brew tap osx-cross/homebrew-avr ; brew install avr-libc")
        endif()
        list(LENGTH avr_AVR_ROOT_DIRS avr_AVR_ROOT_DIRS_COUNT)
        while(avr_AVR_ROOT_DIRS_COUNT GREATER avr_AVR_ROOT_CELLAR)
            list(REMOVE_AT avr_AVR_ROOT_DIRS avr_AVR_ROOT_CELLAR)
            list(LENGTH avr_AVR_ROOT_DIRS avr_AVR_ROOT_DIRS_COUNT)
        endwhile()
        string(REPLACE ";" "/" avr_HOME_BREW "${avr_AVR_ROOT_DIRS}")
        set(avr_AVR_TOOL_PREFIX 
            "${avr_AVR_HOME_BREW}/bin/avr-"
        )
        set(avr_LDFLAGS
            "-L${avr_AVR_HOME_BREW}/lib"
        )
        set( avr_CFLAGS 
             "-I${avr_AVR_HOME_BREW}/include -I${avr_AVR_HOME_BREW}/include/libelf"
        ) 
        set(avr_INCLUDE_PATHS
            "${avr_AVR_HOME_BREW}/include" "${avr_AVR_HOME_BREW}/include/libelf"
        )
    else()
        find_path(avr_OPT_LOCAL_PATH
            NAMES "local"
            PATHS "/opt/"
        )
        if ( avr_OPT_LOCAL_PATH_FOUND )
            find_path(avr_OPT_LOCAL_AVR_PATH
                NAMES "avr"
                PATHS "${avr_OPT_LOCAL_PATH}"
            )
            if ( avr_OPT_LOCAL_AVR_PATH_NOTFOUND )
                message(FATAL_ERROR "Please install avr-gcc avr-libc and related packages or brew tap osx-cross/homebrew-avr ; brew install avr-libc")
            endif()
            find_path( avr_OPT_LOCAL_LIBELF 
                NAMES "libelf" 
                PATHS "${avr_OPT_LOCAL_PATH}/include"
            )
            if ( avr_OPT_LOCAL_LIBELF_NOTFOUND )
                message(FATAL_ERROR "Please install libelf: port install libelf or brew install libelf"
            endif()
            set(avr_AVR_INC
                "${avr_OPT_LOCAL_AVR_PATH}/avr"
            )
            set(avr_AVR_TOOL_PREFIX 
                "${avr_OPT_LOCAL_PATH}/bin/avr-"
            )
            set(avr_LDFLAGS
                "-L${avr_OPT_LOCAL}/lib/"
            )
            set(avr_INCLUDE_PATHS
                "/opt/local/include" "/opt/local/include/libelf"
            )
        else()
            set(avr_AVR_INC 
                "${avr_AVR_ROOT}/avr"
            )
            set(avr_AVR_TOOL_PREFIX 
                "${avr_AVR_ROOT}/bin/avr-"
            )
            set(avr_LDFLAGS
                ""
            )
            set(avr_INCLUDE_PATHS
                ""
            )
        endif()
    endif() 
    find_file(avr_CMAKE_C_COMPILER
        NAMES "clang"
        PATHS $ENV{"PATH"}
    )
    if ( avr_CMAKE_C_COMPILER_NOTFOUND)
        message(FATAL_ERROR "install clang c compiler")
    endif()
    set(CMAKE_C_COMPILER ${avr_CMAKE_C_COMPILER})
else()
    string(REGEX MATCH "^(([fF]ree|[dD]ragon[fF]ly|[oO]pen|[Nn]et)\s*)[Bb][sS][dD]\s*$" simavr_BSD_OS_BUILD "${CMAKE_HOST_SYSTEM_NAME}")
    if ( simavr_BSD_OS_BUOLD LESS 0 )
        message(FATAL_ERROR "Host system \"${CMAKE_HOST_SYSTEM_NAME}\" not uspported. (Linux, Windows, MacOS: Darwin and BSD derivatives)")
    endif()
    find_path(avr_AVR_ROOT 
       NAMES "avr/bin/ranlib" "avr/bin/ld"
       PATHS "/usr/" "/opt/" "/opt/cross" "/opt/cross" "/usr/local"
       PATH_SUFFIXES "lib" "avr" "avr/lib" "lib/avr" "lib32" "lib32/avr" "lib64" "lib64/avr" "libx32" "libx32/avr"
    )
    if( avr_AVR_ROOT_NOTFOUND )
        message(FATAL_ERROR "Please install avr-gcc avr-libc and related packages")
    endif
    set(avr_AVR_ROOT
        "${avr_AVR_ROOT}/avr"
    )
    set(avr_AVR_INC 
        "${avr_AVR_ROOT}/avr"
    )
    set(avr_AVR_TOOL_PREFIX 
        "${avr_AVR_ROOT}/bin/avr-"
    )
    set( avr_CFLAGS 
        ""
    ) 
    set(avr_LDFLAGS
        ""
    )
endif()
file(STRINGS 
    "${avr_AVR_INC}/include/avr/version.h" 
    avr_AVR_LIBC_VERSION_MACRO 
    REGEX "^\s*#define\s+__AVR_LIBC_VERSION_STRING__\s+\"[^\"]+\s*$"
)
list(LENGTH avr_AVR_LIBC_VERSION_MACRO avr_AVR_LIBC_VERSION_MACROS_COUNT)
if ( avr_AVR_LIBC_VERSION_MACROS_COUNT less 1 )
	message(FATAL_ERROR "Avr LIBC version not found fix broken avr-libc pachage")
endif()
string(REGEX REPLACE 
    "^\s*#define\s+__AVR_LIBC_VERSION_STRING__\s+\"(\d+)\.(\d+)\.(\d+)(.*)\"\s*$|^\s*#define\s+__AVR_LIBC_VERSION_STRING__\s+\"(\d+)\.(\d+)()(.*)\"\s*$" 
    "\1;\2;\3;\4" 
    avr_AVR_LIBC_VERSION_NUMBERS 
    list(GET avr_AVR_LIBC_VERSION_MACROS 0))
list(LENGTH avr_AVR_LIBC_VERSION_NUMBERS avr_AVR_LIBC_VERSION_NUMBERS_COUNT)
if ( avr_AVR_LIBC_VERSION_NUMBERS less 2 )
    message(FATAL_ERROR "Cant read avr LIBC version fix broken avr-libc pachage")
endif()
string(REGEX REPKACE ";+" "." avr_AVR_VERSION "${avr_AVR_LIBC_VERSION_NUMBERS}")
set(avr_AVR_VERSION_MAJOR list(GET avr_AVR_LIBC_VERSION_NUMBERS 0 ))
set(avr_AVR_VERSION_MINOR list(GET avr_AVR_LIBC_VERSION_NUMBERS 1 ))
set(avr_AVR_VERSION_PATCH list(GET avr_AVR_LIBC_VERSION_NUMBERS 2 ))
set(avr_AVR_VERSION_POSTFIX list(GET avr_AVR_LIBC_VERSION_NUMBERS 3 ))
set(avr_AVR_FOUND True)
