
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 COMPONENTS Network Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Network Widgets REQUIRED)

add_executable(
	SlideController MACOSX_BUNDLE WIN32
		cameraclient.cpp
		main.cpp
		mainwindow.cpp
		obsclient.cpp
		openlpclient.cpp
		settingsdata.cpp
		settingsdialog.cpp
		slideview.cpp
		sc9k.rc
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

install(
	DIRECTORY
		icons
	DESTINATION
		share
)

install(
	FILES
		net.drinkingtea.sc9k.desktop
	DESTINATION
		share/applications
)

if(UNIX)
	qt_add_resources(
		SlideController "app_images"
			PREFIX "/"
			FILES
				icons/hicolor/16x16/apps/net.drinkingtea.sc9k.png
				icons/hicolor/32x32/apps/net.drinkingtea.sc9k.png
				icons/hicolor/64x64/apps/net.drinkingtea.sc9k.png
				icons/hicolor/128x128/apps/net.drinkingtea.sc9k.png
				icons/hicolor/256x256/apps/net.drinkingtea.sc9k.png
				icons/hicolor/512x512/apps/net.drinkingtea.sc9k.png
	)
endif()
