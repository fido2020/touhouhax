project(dllinject)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

if (NOT WIN32)
    message(FATAL_ERROR "Requires windows compiler")
endif()

# Ensure we are building for 32-bit x86
if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Requires 32-bit x86 build environment")
endif()

add_subdirectory(fmt)

add_library(hax SHARED hax.cpp)
target_link_libraries(hax fmt)
target_compile_options(hax PRIVATE -fno-exceptions -D_WIN32_WINNT=0x0501)
target_link_options(hax PRIVATE -static)

add_executable(dllinject dllinject.cpp)
target_link_libraries(dllinject psapi)
target_compile_options(dllinject PRIVATE -fno-exceptions -D_WIN32_WINNT=0x0501)

target_link_libraries(dllinject fmt)
target_link_options(dllinject PRIVATE -static)
