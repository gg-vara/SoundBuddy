# Windows toolchain : MSYS2's CLANG64 environment (LLVM Clang, libc++, lld), whose bin directory the presets put in the PATH

# MSYS2 installation : MSYS2_ROOT environment variable, else its default location
if(DEFINED ENV{MSYS2_ROOT})
    file(TO_CMAKE_PATH "$ENV{MSYS2_ROOT}" MSYS2_ROOT)
else()
    set(MSYS2_ROOT "C:/msys64")
endif()

# its CLANG64 environment, with the toolchain (see prerequisites/msys2/build.txt)
set(MSYS2_CLANG64_ROOT "${MSYS2_ROOT}/clang64")

if(NOT IS_DIRECTORY "${MSYS2_CLANG64_ROOT}")
    message(FATAL_ERROR "No MSYS2 CLANG64 environment in ${MSYS2_ROOT} : set the MSYS2_ROOT environment variable to the MSYS2 installation (Ex: C:\\msys64)")
endif()

foreach(tool clang++ ld.lld llvm-ar llvm-rc llvm-objcopy)
    if(NOT EXISTS "${MSYS2_CLANG64_ROOT}/bin/${tool}.exe")
        message(FATAL_ERROR "${tool} not found in ${MSYS2_CLANG64_ROOT}/bin : install the build prerequisites (see prerequisites/msys2/build.txt)")
    endif()
endforeach()

# libraries and packages of the environment
list(APPEND CMAKE_PREFIX_PATH ${MSYS2_CLANG64_ROOT})

set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_AR "llvm-ar")
set(CMAKE_RANLIB "llvm-ranlib")
set(CMAKE_NM "llvm-nm")
set(CMAKE_RC_COMPILER "llvm-rc")
set(CMAKE_OBJCOPY "llvm-objcopy")

# LLVM's linker
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-O0")  # added to CMake's -g
