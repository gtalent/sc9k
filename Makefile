PROJECT_NAME=SlideController
BUILDCORE_PATH=deps/buildcore
VCPKG_PKGS=
include ${BUILDCORE_PATH}/base.mk

ifeq ($(OS),darwin)
	PROJECT_EXECUTABLE=./dist/${CURRENT_BUILD}/${PROJECT_NAME}.app/Contents/MacOS/SlideController
else
	PROJECT_EXECUTABLE=./dist/${CURRENT_BUILD}/bin/SlideController
endif

.PHONY: run
run: install
	${ENV_RUN} ${PROJECT_EXECUTABLE}
.PHONY: debug
debug: install
	${ENV_RUN} gdb --args ${PROJECT_EXECUTABLE}
