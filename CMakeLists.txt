cmake_minimum_required(VERSION 3.16)

project(RealCtrl VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets SerialPort)

set(PROJECT_SOURCES
        main.cpp
        ui/mainwindow.cpp
        ui/mainwindow.h
        ui/mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(RealCtrl
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        protocol/dcsprotocoltypes.h protocol/dcsprotocoltypes.cpp
        protocol/ctrlmode.h protocol/ctrlmode.cpp
        protocol/sysform.h protocol/sysform.cpp
        protocol/framecodec.h protocol/framecodec.cpp
        protocol/tlvcodec.h protocol/tlvcodec.cpp
        device/DeviceInterface.h
        device/serialdevice.h device/serialdevice.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET RealCtrl APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(RealCtrl SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(RealCtrl
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(RealCtrl PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

target_include_directories(RealCtrl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/device
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.RealCtrl)
endif()
set_target_properties(RealCtrl PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RealCtrl
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RealCtrl)
endif()
