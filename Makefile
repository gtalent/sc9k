PROJECT_NAME=SlideController
BUILDCORE_PATH=deps/buildcore
VCPKG_PKGS=
include ${BUILDCORE_PATH}/base.mk

ifeq ($(BC_VAR_OS),darwin)
	PROJECT_EXECUTABLE=./build/${BC_VAR_CURRENT_BUILD}/bin/${PROJECT_NAME}.app/Contents/MacOS/SlideController
else
	PROJECT_EXECUTABLE=./build/${BC_VAR_CURRENT_BUILD}/bin/${PROJECT_NAME}
endif

.PHONY: run
run: build
	${ENV_RUN} ${PROJECT_EXECUTABLE}
.PHONY: debug
debug: build
	${DEBUGGER} ${PROJECT_EXECUTABLE}
