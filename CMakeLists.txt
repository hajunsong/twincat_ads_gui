cmake_minimum_required(VERSION 3.16)
project(twincat_ads_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(BECKHOFF_ADS_ROOT "${CMAKE_SOURCE_DIR}/third_party/Beckhoff.ADS" CACHE PATH
	"Root of Beckhoff/ADS sources (clone offline into third_party)")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(BeckhoffAds)

beckhoff_add_adslib(adslib "${BECKHOFF_ADS_ROOT}")

find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets Network PrintSupport)

add_executable(twincat_ads_gui
	src/main.cpp
	src/mainwindow.cpp
	src/mainwindow.h
	src/mainwindow.ui
	src/graphdisplaywindow.cpp
	src/graphdisplaywindow.h
	src/topologywidget.cpp
	src/topologywidget.h
	src/qcustomplot-source/qcustomplot.cpp
)

target_include_directories(twincat_ads_gui PRIVATE
	"${CMAKE_SOURCE_DIR}/src/qcustomplot-source"
)

target_link_libraries(twincat_ads_gui PRIVATE
	Qt5::Widgets
	Qt5::Network
	Qt5::PrintSupport
	adslib
)

if(WIN32 AND MSVC)
	set_target_properties(twincat_ads_gui PROPERTIES WIN32_EXECUTABLE ON)
endif()
