cmake_minimum_required(VERSION 3.5)

project(NetAndSerialPort
	LANGUAGES CXX
	VERSION 1.13.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(liblsl REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost 1.67 REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(${PROJECT_NAME}
	main.cpp
	mainwindow.cpp
	mainwindow.h
	mainwindow.ui
	reader.cpp
	reader.hpp
)
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Qt5::Widgets
	Threads::Threads
	LSL::lsl
	Boost::boost
)

if(Boost_VERSION LESS 106900)
	find_package(Boost COMPONENTS system REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_auto_type cxx_lambda_init_captures)
target_compile_definitions(${PROJECT_NAME} PRIVATE BOOST BOOST_DATE_TIME_NO_LIB BOOST_REGEX_NO_LIB)
installLSLApp(${PROJECT_NAME})
installLSLAuxFiles(${PROJECT_NAME}
	${PROJECT_NAME}.cfg
)

LSLGenerateCPackConfig()
