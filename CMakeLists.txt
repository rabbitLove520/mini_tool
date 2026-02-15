cmake_minimum_required(VERSION 3.25)

SET(CMAKE_C_COMPILER gcc)
SET(CMAKE_CXX_COMPILER g++)

project(mini_tool)

aux_source_directory(src SOURCE)

set(wxWidgets_ROOT_DIR $ENV{WXWIN})
set(wxWidgets_LIB_DIR $ENV{WXWIN}/lib/gcc_lib)
set(wxWidgets_CONFIGURATION mswu)

set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
set(CMAKE_CXX_FLAGS_RELEASE, "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG") # 优化编译

set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets COMPONENTS core base adv)

# ========== 资源文件（Windows）==========
set(RESOURCE_FILES
    resources/app.rc
    resources/app.ico
    resources/app.manifest  # 确保 manifest 被编译进资源
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCE} ${RESOURCE_FILES})

# ========== 链接库 ==========
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${wxWidgets_LIBRARIES}
    advapi32
    iphlpapi
    psapi
    -static          # 静态链接 MinGW 运行时
    -static-libgcc   # 静态链接 GCC 运行时
    -static-libstdc++ # 静态链接 C++ 标准库
)

# ========== 编译定义 ==========
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601  # Windows 7+
    wxUSE_DPI_AWARE_MANIFEST=1 # ✅ 启用 wxWidgets DPI 支持
)

# ========== 包含目录 ==========
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/resources
    ${wxWidgets_INCLUDE_DIRS}  # ✅ 正确获取 wxWidgets 头文件路径
)

# ========== 资源编译器配置（关键修复）==========
if(MINGW)
    # ✅ 正确方式：为 RC 文件添加 wxWidgets 头文件路径
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I\"${wxWidgets_INCLUDE_DIRS}\"")
    
    # GCC 10+ UTF-8 支持（可选）
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

