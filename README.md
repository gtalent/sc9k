# Slide Controller 9000

## Build Prerequisites

* Install GCC, Clang, or Visual Studio with C++20 support
* Install Python 3
* Install Ninja, Make, and CMake
* [Qt6](https://www.qt.io/download-qt-installer-oss) (Network and Widgets)
* Consider also installing ccache for faster subsequent build times

## Build

Build options: release, debug, asan

	make purge configure-{release,debug,asan} install

## Run

	make run
