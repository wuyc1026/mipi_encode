# 指定最低 CMake 版本
cmake_minimum_required(VERSION 3.10) 

#设置SDK路径
set(SDK "/home/tronlong/RK3562/rk3562_linux_sdk_release/buildroot/output/rockchip_rk3562/host/usr/aarch64-buildroot-linux-gnu/sysroot")

# 设置目标输出目录到指定路径
set(TARGET_OUTPUT_DIR "/media/sf_virtual_box_share/output_mipi/bin")

# 确保输出目录存在[3](@ref)
file(MAKE_DIRECTORY ${TARGET_OUTPUT_DIR})

#设置编译器路径
set(CMAKE_C_COMPILER /home/tronlong/RK3562/rk3562_linux_sdk_release/buildroot/output/rockchip_rk3562/host/bin/aarch64-buildroot-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /home/tronlong/RK3562/rk3562_linux_sdk_release/buildroot/output/rockchip_rk3562/host/bin/aarch64-buildroot-linux-gnu-g++)

#设置编译标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O0 -g -std=c11 -DDEBUG")
# 定义项目名称和版本
project(mipi_mpp C) 
    set(PROJECT_VERSION "1.0.1")
    set(PROJECT_DESCRIPTION "瑞芯微MPP的JPEG测试")
set(TARGET "mipi_text")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/mipi_main.c" )
    set(SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/mipi_main.c
                     ${CMAKE_CURRENT_SOURCE_DIR}/lib/camera_init.c)
else()
    message(FATAL_ERROR "mipi_main.c not found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# 添加头文件和库文件搜索路径
include_directories(${SDK}/usr/include
${CMAKE_CURRENT_SOURCE_DIR}/inc)

link_directories(${SDK}/usr/lib)
#设置依赖项
set(LIBS rockchip_mpp pthread)

# 创建一个可执行文件目标
add_executable(${TARGET} ${SOURCE_FILES})  
# 链接库到可执行文件
target_link_libraries(${TARGET} ${LIBS})
# 设置 C++ 标准为 C++11,并将可执行文件输出到指定目录[2,6](@ref)
set_target_properties(${TARGET} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${TARGET_OUTPUT_DIR}
)

message(STATUS "==========================================")
message(STATUS "项目: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "输出目录: ${TARGET_OUTPUT_DIR}")
foreach(file ${SOURCE_FILES})
    message(STATUS "  - ${file}")
endforeach()
message(STATUS "编译器: ${CMAKE_C_COMPILER}")
message(STATUS "==========================================")
