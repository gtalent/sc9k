#! /usr/bin/env python3

#
#  Copyright 2016 - 2021 gary@drinkingtea.net
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

import argparse
import os
import platform
import shutil
import subprocess
import sys

from pybb import mkdir, rm

os_name = os.uname().sysname.lower()

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', help='Platform target',
                        default='{:s}-{:s}'.format(os_name, platform.machine()))
    parser.add_argument('--build_type', help='Build type (asan,debug,release)', default='release')
    parser.add_argument('--build_tool', help='Build tool (default,xcode)', default='')
    parser.add_argument('--build_root', help='Path to the root of build directories (must be in project dir)', default='build')
    parser.add_argument('--toolchain', help='Path to CMake toolchain file', default='')
    parser.add_argument('--current_build', help='Indicates whether or not to make this the active build', default=1)
    args = parser.parse_args()

    if args.build_type == 'asan':
        build_type_arg = 'Debug'
        sanitizer_status = 'ON'
    elif args.build_type == 'debug':
        build_type_arg = 'Debug'
        sanitizer_status = 'OFF'
    elif args.build_type == 'release':
        build_type_arg = 'Release'
        sanitizer_status = 'OFF'
    else:
        print('Error: Invalid build tool')
        return 1

    if args.build_tool == 'xcode':
        build_config = '{:s}-{:s}'.format(args.target, args.build_tool)
    else:
        build_config = '{:s}-{:s}'.format(args.target, args.build_type)

    if 'QTDIR' in os.environ:
        qt_path = '-DQTDIR={:s}'.format(os.environ['QTDIR'])
    else:
        qt_path = ''

    if args.build_tool == '' or args.build_tool == 'default':
        if shutil.which('ninja') is None:
            build_tool = ''
        else:
            build_tool = '-GNinja'
    elif args.build_tool == 'xcode':
        build_tool = '-GXcode'
    else:
        print('Error: Invalid build tool')
        return 1

    project_dir = os.getcwd()
    build_dir = '{:s}/{:s}/{:s}'.format(project_dir, args.build_root, build_config)
    rm(build_dir)
    cmake_cmd = [
        'cmake', '-S', project_dir, '-B', build_dir, build_tool,
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCMAKE_TOOLCHAIN_FILE={:s}'.format(args.toolchain),
        '-DCMAKE_BUILD_TYPE={:s}'.format(build_type_arg),
        '-DUSE_ASAN={:s}'.format(sanitizer_status),
        '-DBUILDCORE_BUILD_CONFIG={:s}'.format(build_config),
        '-DBUILDCORE_TARGET={:s}'.format(args.target),
    ]
    if qt_path != '':
        cmake_cmd.append(qt_path)
    if platform.system() == 'Windows':
        cmake_cmd.append('-A x64')

    subprocess.run(cmake_cmd)

    mkdir('dist')
    if int(args.current_build) != 0:
        cb = open('.current_build', 'w')
        cb.write(args.build_type)
        cb.close()

    rm('compile_commands.json')
    if platform.system() != 'Windows':
        os.symlink('{:s}/compile_commands.json'.format(build_dir), 'compile_commands.json')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(1)
