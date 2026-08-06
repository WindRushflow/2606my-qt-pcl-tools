cmake_minimum_required(VERSION 3.21)
project(myQtPclTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================
# Qt 自动编译配置
# ==============================
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
list(APPEND CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui")

# ==============================
# 输出目录
# ==============================
set(OUTPUT_ROOT "${CMAKE_SOURCE_DIR}/output")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_ROOT}/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_ROOT}/Release")

# ==============================
# 自动收集文件
# ==============================
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB INC_FILES "${CMAKE_SOURCE_DIR}/include/*.h")
file(GLOB UI_FILES "${CMAKE_SOURCE_DIR}/ui/*.ui")

# ==============================
# VS 目录分组
# ==============================
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${INC_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${SRC_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES ${UI_FILES})

# ==============================
# 依赖
# ==============================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 优先找到带 Qt 支持的 VTK（含 QVTKOpenGLNativeWidget）
list(PREPEND CMAKE_PREFIX_PATH "D:/VTK/VTK-9.4.2-Qt")
find_package(VTK REQUIRED COMPONENTS GUISupportQt)

find_package(PCL REQUIRED)

# ==============================
# 生成 exe
# ==============================
add_executable(${PROJECT_NAME}
    ${SRC_FILES}
    ${INC_FILES}
    ${UI_FILES}
)

# ==============================
# 头文件路径
# ==============================
target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        ${PCL_INCLUDE_DIRS}
        "${CMAKE_BINARY_DIR}"
        E:/sdmcfjwrjsscj/practice/VSCode-C++/PointCloud/2606myPclLib/include
)

add_definitions(${PCL_DEFINITIONS})
link_directories(${PCL_LIBRARY_DIRS})

# ==============================
# 链接：Qt + PCL + 静态库
# ==============================
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
        ${PCL_LIBRARIES}
        ${VTK_LIBRARIES}
        VTK::GUISupportQt
        debug   E:/sdmcfjwrjsscj/practice/VSCode-C++/PointCloud/2606myPclLib/output/Debug/mypcllib.lib
)