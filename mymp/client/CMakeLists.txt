cmake_minimum_required(VERSION 3.20)
project(MyMPClient CXX)

# Alternative build path for Visual Studio / CLion users:
#   cmake -B build && cmake --build build --config Release
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(MyMP SHARED src/net.cpp src/scriptthread.cpp src/client.cpp)
target_include_directories(MyMP PRIVATE src)
target_compile_definitions(MyMP PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(MyMP ws2_32 psapi)
set_target_properties(MyMP PROPERTIES OUTPUT_NAME "MyMP")
# rename to .asi after build, or copy with: copy build\Release\MyMP.dll MyMP.asi
