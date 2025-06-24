cmake_minimum_required(VERSION 3.14)
project(disk_benchmark)

set(CMAKE_CXX_STANDARD 17)

# Define Python paths manually (Intel macOS with Homebrew Python)
set(Python3_EXECUTABLE "/usr/local/bin/python3")

# Get Python include directory
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import sysconfig; print(sysconfig.get_path('include'))"
    OUTPUT_VARIABLE PYTHON_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get NumPy include directory
execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import numpy; print(numpy.get_include())"
    OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get Python library path (libpython)
set(PYTHON_LIBRARY "/usr/local/opt/python@3.13/Frameworks/Python.framework/Versions/3.13/lib/libpython3.13.dylib")

# Include dirs
include_directories(${PYTHON_INCLUDE_DIR})
include_directories(${NUMPY_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/external)  # for matplotlibcpp.h

add_executable(disk_benchmark disk_benchmark.cpp)

# Add compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(disk_benchmark PRIVATE -Wno-deprecated-declarations)
endif()

# Link against Python shared library
target_link_libraries(disk_benchmark PRIVATE ${PYTHON_LIBRARY})
