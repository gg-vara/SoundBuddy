# MSYS2 installation : MSYS2_ROOT environment variable, else its default location
if(DEFINED ENV{MSYS2_ROOT})
    file(TO_CMAKE_PATH "$ENV{MSYS2_ROOT}" MSYS2_ROOT)
else()
    SET(MSYS2_ROOT "C:/msys64")
endif()

# its CLANG64 environment (LLVM Clang, libc++), with the toolchain (see prerequisites/msys2/build.txt)
SET(MSYS2_CLANG64_ROOT "${MSYS2_ROOT}/clang64")

if(NOT IS_DIRECTORY "${MSYS2_CLANG64_ROOT}")
    message(FATAL_ERROR "No MSYS2 CLANG64 environment in ${MSYS2_ROOT} : set the MSYS2_ROOT environment variable to the MSYS2 installation (Ex: C:\\msys64)")
endif()

foreach(tool clang++ ld.lld llvm-ar llvm-rc llvm-objcopy)
    if(NOT EXISTS "${MSYS2_CLANG64_ROOT}/bin/${tool}.exe")
        message(FATAL_ERROR "${tool} not found in ${MSYS2_CLANG64_ROOT}/bin : install the build prerequisites (see prerequisites/msys2/build.txt)")
    endif()
endforeach()

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)

# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

list(APPEND CMAKE_PREFIX_PATH
    ${MSYS2_CLANG64_ROOT}
)

SET (CMAKE_CXX_COMPILER         "clang++")

SET (CMAKE_AR                   "llvm-ar")
SET (CMAKE_RANLIB               "llvm-ranlib")
SET (CMAKE_NM                   "llvm-nm")
SET (CMAKE_RC_COMPILER          "llvm-rc")
SET (CMAKE_OBJCOPY              "llvm-objcopy")

SET (CMAKE_CXX_FLAGS            "-fuse-ld=lld")
SET (CMAKE_CXX_FLAGS_DEBUG      "-O0 -g")