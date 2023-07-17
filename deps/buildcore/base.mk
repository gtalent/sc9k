#
#  Copyright 2016 - 2023 gary@drinkingtea.net
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

ifeq (${OS},Windows_NT)
	SHELL := powershell.exe
	.SHELLFLAGS := -NoProfile -Command
	OS=windows
	HOST_ENV=${OS}
else
	OS=$(shell uname | tr [:upper:] [:lower:])
	HOST_ENV=${OS}-$(shell uname -m)
endif

DEVENV=devenv$(shell pwd | sed 's/\//-/g')
DEVENV_IMAGE=${PROJECT_NAME}-devenv
ifneq ($(shell which docker 2> /dev/null),)
	ifeq ($(shell docker inspect --format="{{.State.Status}}" ${DEVENV} 2>&1),running)
		ENV_RUN=docker exec -i -t --user $(shell id -u ${USER}) ${DEVENV}
	endif
endif

ifneq ($(shell ${ENV_RUN} which python3 2> /dev/null),)
	PYTHON3=python3
else
	ifeq ($(shell ${ENV_RUN} python -c 'import sys; print(sys.version_info[0])'),3)
		PYTHON3=python
	endif
endif

SCRIPTS=${BUILDCORE_PATH}/scripts
SETUP_BUILD=${PYTHON3} ${SCRIPTS}/setup-build.py
PYBB=${PYTHON3} ${SCRIPTS}/pybb.py
CMAKE_BUILD=${PYBB} cmake-build
GET_ENV=${PYBB} getenv
CTEST=${PYBB} ctest-all
RM_RF=${PYBB} rm
HOST=$(shell ${PYBB} hostname)
BUILDCORE_HOST_SPECIFIC_BUILDPATH=$(shell ${GET_ENV} BUILDCORE_HOST_SPECIFIC_BUILDPATH)
ifneq (${BUILDCORE_HOST_SPECIFIC_BUILDPATH},)
BUILD_PATH=build/${HOST}
else
BUILD_PATH=build
endif
ifdef USE_VCPKG
	ifndef VCPKG_DIR_BASE
		VCPKG_DIR_BASE=.vcpkg
	endif
	ifndef VCPKG_VERSION
		VCPKG_VERSION=2020.06
	endif
	VCPKG_TOOLCHAIN=--toolchain=${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake
endif
ifeq ($(OS),darwin)
	DEBUGGER=lldb --
else
	DEBUGGER=gdb --args
endif

VCPKG_DIR=$(VCPKG_DIR_BASE)/$(VCPKG_VERSION)-$(HOST_ENV)
CURRENT_BUILD=$(HOST_ENV)-$(shell ${ENV_RUN} ${PYBB} cat .current_build)

.PHONY: build
build:
	${ENV_RUN} ${CMAKE_BUILD} ${BUILD_PATH}
.PHONY: install
install:
	${ENV_RUN} ${CMAKE_BUILD} ${BUILD_PATH} install
.PHONY: clean
clean:
	${ENV_RUN} ${CMAKE_BUILD} ${BUILD_PATH} clean
.PHONY: purge
purge:
	${ENV_RUN} ${RM_RF} .current_build
	${ENV_RUN} ${RM_RF} ${BUILD_PATH}
	${ENV_RUN} ${RM_RF} dist
.PHONY: test
test: build
	${ENV_RUN} mypy ${SCRIPTS}
	${ENV_RUN} ${CMAKE_BUILD} ${BUILD_PATH} test
.PHONY: test-verbose
test-verbose: build
	${ENV_RUN} ${CTEST} ${BUILD_PATH} --output-on-failure
.PHONY: test-rerun-verbose
test-rerun-verbose: build
	${ENV_RUN} ${CTEST} ${BUILD_PATH} --rerun-failed --output-on-failure

.PHONY: devenv-image
devenv-image:
	docker build . -t ${DEVENV_IMAGE}
.PHONY: devenv-create
devenv-create:
	docker run -d \
		-e LOCAL_USER_ID=$(shell id -u ${USER}) \
		-e DISPLAY=$(DISPLAY) \
		-e QT_AUTO_SCREEN_SCALE_FACTOR=1 \
		-v /tmp/.X11-unix:/tmp/.X11-unix \
		-v /run/dbus/:/run/dbus/ \
		-v $(shell pwd):/usr/src/project \
		-v /dev/shm:/dev/shm \
		--restart=always \
		--name ${DEVENV} \
		-t ${DEVENV_IMAGE} bash
.PHONY: devenv-destroy
devenv-destroy:
	docker rm -f ${DEVENV}
ifdef ENV_RUN
.PHONY: devenv-shell
devenv-shell:
	${ENV_RUN} bash
endif

ifdef USE_VCPKG

.PHONY: vcpkg
vcpkg: ${VCPKG_DIR} vcpkg-install

${VCPKG_DIR}:
	${ENV_RUN} ${RM_RF} ${VCPKG_DIR}
	${ENV_RUN} mkdir -p ${VCPKG_DIR_BASE}
	${ENV_RUN} git clone -b release --depth 1 --branch ${VCPKG_VERSION} https://github.com/microsoft/vcpkg.git ${VCPKG_DIR}
ifneq (${OS},windows)
	${ENV_RUN} ${VCPKG_DIR}/bootstrap-vcpkg.sh
else
	${ENV_RUN} ${VCPKG_DIR}/bootstrap-vcpkg.bat
endif

.PHONY: vcpkg-install
vcpkg-install:
ifneq (${OS},windows)
	${VCPKG_DIR}/vcpkg install ${VCPKG_PKGS}
else
	${VCPKG_DIR}/vcpkg install --triplet x64-windows ${VCPKG_PKGS}
endif

else ifdef USE_CONAN # USE_VCPKG ################################################

.PHONY: setup-conan
conan-config:
	${ENV_RUN} conan profile new ${PROJECT_NAME} --detect --force
ifeq ($(OS),linux)
	${ENV_RUN} conan profile update settings.compiler.libcxx=libstdc++11 ${PROJECT_NAME}
else
	${ENV_RUN} conan profile update settings.compiler.cppstd=20 ${PROJECT_NAME}
ifeq ($(OS),windows)
	${ENV_RUN} conan profile update settings.compiler.runtime=static ${PROJECT_NAME}
endif
endif
.PHONY: conan
conan:
	${ENV_RUN} ${PYBB} conan-install ${PROJECT_NAME}
endif # USE_VCPKG ###############################################

ifeq (${OS},darwin)
.PHONY: configure-xcode
configure-xcode:
	${ENV_RUN} ${SETUP_BUILD} ${VCPKG_TOOLCHAIN} --build_tool=xcode --current_build=0 --build_root=${BUILD_PATH}
endif

.PHONY: configure-release
configure-release:
	${ENV_RUN} ${SETUP_BUILD} ${VCPKG_TOOLCHAIN} --build_type=release --build_root=${BUILD_PATH}

.PHONY: configure-debug
configure-debug:
	${ENV_RUN} ${SETUP_BUILD} ${VCPKG_TOOLCHAIN} --build_type=debug --build_root=${BUILD_PATH}

.PHONY: configure-asan
configure-asan:
	${ENV_RUN} ${SETUP_BUILD} ${VCPKG_TOOLCHAIN} --build_type=asan --build_root=${BUILD_PATH}

