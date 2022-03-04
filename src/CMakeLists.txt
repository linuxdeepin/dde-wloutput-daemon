
project(dde-wloutput-daemon)

# 增加安全编译参数
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
set(CMAKE_EXE_LINKER_FLAGS  "-z relro -z now -z noexecstack -pie")

set(TARGET_NAME dde-wloutput-daemon)
FILE(GLOB SC_FILES "*.cpp" "*.h")

add_executable(${TARGET_NAME} ${SC_FILES})

find_package(KF5Wayland REQUIRED)
# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt5 COMPONENTS
    Core
    Gui
    DBus
REQUIRED)

target_link_libraries(${TARGET_NAME}  Qt5::Core Qt5::DBus)
target_link_libraries(${TARGET_NAME} KF5::WaylandClient KF5::WaylandServer)
