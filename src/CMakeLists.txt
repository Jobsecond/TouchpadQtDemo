project(TouchpadQtDemo)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets REQUIRED)

add_executable(${PROJECT_NAME} WIN32
        main.cpp
        TouchpadHelper.cpp
        TouchpadHelper.h
        MainWindow.cpp
        MainWindow.h)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_link_libraries(${PROJECT_NAME} Qt6::Widgets hid)
