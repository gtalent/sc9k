
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Network Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Network Widgets REQUIRED)

add_executable(
	SlideController MACOSX_BUNDLE
		main.cpp
		mainwindow.cpp
		obsclient.cpp
		openlpclient.cpp
		slideview.cpp
)

target_link_libraries(
	SlideController
		Qt${QT_VERSION_MAJOR}::Network
		Qt${QT_VERSION_MAJOR}::Widgets
)

install(
	TARGETS
		SlideController
	DESTINATION
		bin
	BUNDLE DESTINATION .
)

