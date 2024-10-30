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
	BC_VAR_OS=windows
	BC_CMD_HOST_PY3=python
else
	BC_VAR_OS=$(shell uname | tr [:upper:] [:lower:])
	ifneq ($(shell which python3 2> /dev/null),)
		BC_CMD_HOST_PY3=python3
	else
		ifeq ($(shell python -c 'import sys; print(sys.version_info[0])'),3)
			BC_CMD_HOST_PY3=python
		else
			echo 'Please install Python3 on host'
			exit 1
		endif
	endif
endif


ifdef BC_VAR_USE_DOCKER_DEVENV
	ifneq ($(shell which docker 2> /dev/null),)
		BC_VAR_DEVENV=devenv$(shell pwd | sed 's/\//-/g')
		BC_VAR_DEVENV_IMAGE=${BC_VAR_PROJECT_NAME}-devenv
		ifeq ($(shell docker inspect --format="{{.State.Status}}" ${BC_VAR_DEVENV} 2>&1),running)
			BC_CMD_ENVRUN=docker exec -i -t --user $(shell id -u ${USER}) ${BC_VAR_DEVENV}
		endif
	endif
	ifneq ($(shell ${BC_CMD_ENVRUN} which python3 2> /dev/null),)
		BC_CMD_PY3=${BC_CMD_ENVRUN} python3
	else
		ifeq ($(shell ${BC_CMD_ENVRUN} python -c 'import sys; print(sys.version_info[0])'),3)
			BC_CMD_PY3=${BC_CMD_ENVRUN} python
		else
			echo 'Please install Python3 in devenv'
			exit 1
		endif
	endif
	ifndef BC_VAR_DEVENV_ROOT
		BC_VAR_DEVENV_ROOT="."
	endif
else
	BC_CMD_PY3=${BC_CMD_HOST_PY3}
endif

BC_VAR_SCRIPTS=${BUILDCORE_PATH}/scripts
BC_CMD_SETUP_BUILD=${BC_CMD_PY3} ${BC_VAR_SCRIPTS}/setup-build.py
BC_CMD_PYBB=${BC_CMD_PY3} ${BC_VAR_SCRIPTS}/pybb.py
BC_CMD_HOST_PYBB=${BC_CMD_HOST_PY3} ${BC_VAR_SCRIPTS}/pybb.py
BC_CMD_CMAKE_BUILD=${BC_CMD_PYBB} cmake-build
BC_CMD_GETENV=${BC_CMD_PYBB} getenv
BC_CMD_CTEST=${BC_CMD_PYBB} ctest-all
BC_CMD_RM_RF=${BC_CMD_PYBB} rm
BC_CMD_MKDIR_P=${BC_CMD_PYBB} mkdir
BC_CMD_CAT=${BC_CMD_PYBB} cat
BC_CMD_DEBUGGER=${BC_CMD_PYBB} debug
BC_CMD_HOST_DEBUGGER=${BC_CMD_HOST_PYBB} debug
BC_VAR_HOSTENV=$(shell ${BC_CMD_ENVRUN} ${BC_CMD_PYBB} hostenv)
BC_VAR_BUILD_PATH=build
BC_VAR_CURRENT_BUILD=$(BC_VAR_HOSTENV)-$(shell ${BC_CMD_ENVRUN} ${BC_CMD_CAT} .current_build)

ifdef BC_VAR_USE_VCPKG
ifndef BC_VAR_VCPKG_DIR_BASE
	BC_VAR_VCPKG_DIR_BASE=.vcpkg
endif
ifndef BC_VAR_VCPKG_VERSION
	BC_VAR_VCPKG_VERSION=2023.08.09
endif
endif

.PHONY: build
build:
	${BC_CMD_CMAKE_BUILD} ${BC_VAR_BUILD_PATH}
.PHONY: install
install:
	${BC_CMD_CMAKE_BUILD} ${BC_VAR_BUILD_PATH} install
.PHONY: clean
clean:
	${BC_CMD_CMAKE_BUILD} ${BC_VAR_BUILD_PATH} clean
.PHONY: purge
purge:
	${BC_CMD_RM_RF} .current_build
	${BC_CMD_RM_RF} ${BC_VAR_BUILD_PATH}
	${BC_CMD_RM_RF} dist
	${BC_CMD_RM_RF} compile_commands.json
.PHONY: test
test: build
	${BC_CMD_ENVRUN} mypy ${BC_VAR_SCRIPTS}
	${BC_CMD_CMAKE_BUILD} ${BC_VAR_BUILD_PATH} test
.PHONY: test-verbose
test-verbose: build
	${BC_CMD_CTEST} ${BC_VAR_BUILD_PATH} --output-on-failure
.PHONY: test-rerun-verbose
test-rerun-verbose: build
	${BC_CMD_CTEST} ${BC_VAR_BUILD_PATH} --rerun-failed --output-on-failure

ifdef BC_VAR_USE_DOCKER_DEVENV
.PHONY: devenv-image
devenv-image:
	docker build ${BC_VAR_DEVENV_ROOT} -t ${BC_VAR_DEVENV_IMAGE}
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
		--name ${BC_VAR_DEVENV} \
		-t ${BC_VAR_DEVENV_IMAGE} bash
.PHONY: devenv-destroy
devenv-destroy:
	docker rm -f ${BC_VAR_DEVENV}
ifdef BC_CMD_ENVRUN
.PHONY: devenv-shell
devenv-shell:
	${BC_CMD_ENVRUN} bash
endif
endif

ifdef BC_VAR_USE_VCPKG

BC_VAR_VCPKG_TOOLCHAIN=--toolchain=${BC_VAR_VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake
BC_VAR_VCPKG_DIR=$(BC_VAR_VCPKG_DIR_BASE)/$(BC_VAR_VCPKG_VERSION)-$(BC_VAR_HOSTENV)

.PHONY: vcpkg
vcpkg: ${BC_VAR_VCPKG_DIR} vcpkg-install

${BC_VAR_VCPKG_DIR}:
	${BC_CMD_RM_RF} ${BC_VAR_VCPKG_DIR}
	${BC_CMD_PYBB} mkdir ${BC_VAR_VCPKG_DIR_BASE}
	${BC_CMD_ENVRUN} git clone -b release --depth 1 --branch ${BC_VAR_VCPKG_VERSION} https://github.com/microsoft/vcpkg.git ${BC_VAR_VCPKG_DIR}
ifneq (${BC_VAR_OS},windows)
	${BC_CMD_ENVRUN} ${BC_VAR_VCPKG_DIR}/bootstrap-vcpkg.sh
else
	${BC_CMD_ENVRUN} ${BC_VAR_VCPKG_DIR}/bootstrap-vcpkg.bat
endif

.PHONY: vcpkg-install
vcpkg-install:
ifneq (${BC_VAR_OS},windows)
	${BC_CMD_ENVRUN} ${BC_VAR_VCPKG_DIR}/vcpkg install ${BC_VAR_VCPKG_PKGS}
else
	${BC_CMD_ENVRUN} ${BC_VAR_VCPKG_DIR}/vcpkg install --triplet x64-windows ${BC_VAR_VCPKG_PKGS}
endif

else ifdef USE_CONAN # USE_VCPKG / USE_CONAN ####################################
.PHONY: setup-conan
conan-config:
	${BC_CMD_ENVRUN} conan profile new ${BC_VAR_PROJECT_NAME} --detect --force
ifeq ($(BC_VAR_OS),linux)
	${BC_CMD_ENVRUN} conan profile update settings.compiler.libcxx=libstdc++11 ${BC_VAR_PROJECT_NAME}
else
	${BC_CMD_ENVRUN} conan profile update settings.compiler.cppstd=20 ${BC_VAR_PROJECT_NAME}
ifeq ($(BC_VAR_OS),windows)
	${BC_CMD_ENVRUN} conan profile update settings.compiler.runtime=static ${BC_VAR_PROJECT_NAME}
endif
endif

.PHONY: conan
conan:
	${BC_CMD_PYBB} conan-install ${BC_VAR_PROJECT_NAME}
endif # USE_CONAN ###############################################

ifeq (${BC_VAR_OS},darwin)
.PHONY: configure-xcode
configure-xcode:
	${BC_CMD_SETUP_BUILD} ${BC_VAR_VCPKG_TOOLCHAIN} --build_tool=xcode --current_build=0 --build_root=${BC_VAR_BUILD_PATH}
endif

.PHONY: configure-release
configure-release:
	${BC_CMD_SETUP_BUILD} ${BC_VAR_VCPKG_TOOLCHAIN} --build_type=release --build_root=${BC_VAR_BUILD_PATH}

.PHONY: configure-debug
configure-debug:
	${BC_CMD_SETUP_BUILD} ${BC_VAR_VCPKG_TOOLCHAIN} --build_type=debug --build_root=${BC_VAR_BUILD_PATH}

.PHONY: configure-asan
configure-asan:
	${BC_CMD_SETUP_BUILD} ${BC_VAR_VCPKG_TOOLCHAIN} --build_type=asan --build_root=${BC_VAR_BUILD_PATH}

