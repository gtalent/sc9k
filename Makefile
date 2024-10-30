PROJECT_NAME=SlideController
BUILDCORE_PATH=deps/buildcore
VCPKG_PKGS=
include ${BUILDCORE_PATH}/base.mk

ifeq ($(OS),darwin)
	PROJECT_EXECUTABLE=./build/${BC_VAR_CURRENT_BUILD}/${PROJECT_NAME}.app/Contents/MacOS/SlideController
else
	PROJECT_EXECUTABLE=./build/${BC_VAR_CURRENT_BUILD}/bin/SlideController
endif

.PHONY: run
run:
	${ENV_RUN} ${PROJECT_EXECUTABLE}
.PHONY: debug
debug:
	${DEBUGGER} ${PROJECT_EXECUTABLE}
