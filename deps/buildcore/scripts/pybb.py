#! /usr/bin/env python3

#
#  Copyright 2016 - 2021 gary@drinkingtea.net
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

# "Python Busy Box" - adds cross-platform equivalents to Unix commands that
#                     don't translate well to that other operating system

import os
import platform
import shutil
import subprocess
import sys
from typing import List, Optional

import util


def mkdir(path: str) -> int:
    try:
        util.mkdir_p(path)
    except Exception:
        return 1
    return 0


def rm_multi(paths: List[str]):
    for path in paths:
        util.rm(path)


def ctest_all() -> int:
    base_path = sys.argv[2]
    if not os.path.isdir(base_path):
        # no generated projects
        return 0
    args = ['ctest'] + sys.argv[3:]
    orig_dir = os.getcwd()
    for d in os.listdir(base_path):
        os.chdir(os.path.join(orig_dir, base_path, d))
        err = subprocess.run(args).returncode
        if err != 0:
            return err
    return 0


def cmake_build(base_path: str, target: Optional[str]) -> int:
    if not os.path.isdir(base_path):
        # nothing to build
        return 0
    for d in os.listdir(base_path):
        path = os.path.join(base_path, d)
        if not os.path.isdir(path):
            continue
        args = ['cmake', '--build', path]
        if target is not None:
            args.extend(['--target', target])
        err = subprocess.run(args).returncode
        if err != 0:
            return err
    return 0


def conan() -> int:
    project_name = sys.argv[2]
    conan_dir = '.conanbuild'
    err = 0
    try:
        mkdir(conan_dir)
    except Exception:
        return 1
    if err != 0:
        return err
    args = ['conan', 'install', '../', '--build=missing', '-pr', project_name]
    os.chdir(conan_dir)
    return subprocess.run(args).returncode


def cat(paths: List[str]) -> int:
    for path in paths:
        try:
            with open(path) as f:
                data = f.read()
                print(data)
        except FileNotFoundError:
            sys.stderr.write(f'cat: {path}: no such file or directory\n')
            return 1
    return 0


def debug(paths: List[str]) -> int:
    if shutil.which('gdb') is not None:
        args = ['gdb', '--args']
    elif shutil.which('lldb') is not None:
        args = ['lldb', '--']
    else:
        sys.stderr.write('debug: could not find a supported debugger\n')
        return 1
    args.extend(paths)
    return subprocess.run(args).returncode


def get_env(var_name: str) -> int:
    if var_name not in os.environ:
        return 1
    print(os.environ[var_name])
    return 0


def hostname() -> int:
    print(platform.node())
    return 0


def host_env() -> int:
    os_name = platform.system().lower()
    arch = util.get_arch()
    print(f'{os_name}-{arch}')
    return 0


def clarg(idx: int) -> Optional[str]:
    return sys.argv[idx] if len(sys.argv) > idx else None


def main() -> int:
    err = 0
    if sys.argv[1] == 'mkdir':
        err = mkdir(sys.argv[2])
    elif sys.argv[1] == 'rm':
        rm_multi(sys.argv[2:])
    elif sys.argv[1] == 'conan-install':
        err = conan()
    elif sys.argv[1] == 'ctest-all':
        err = ctest_all()
    elif sys.argv[1] == 'cmake-build':
        err = cmake_build(sys.argv[2], clarg(3))
    elif sys.argv[1] == 'cat':
        err = cat(sys.argv[2:])
    elif sys.argv[1] == 'debug':
        err = debug(sys.argv[2:])
    elif sys.argv[1] == 'getenv':
        err = get_env(sys.argv[2])
    elif sys.argv[1] == 'hostname':
        err = hostname()
    elif sys.argv[1] == 'hostenv':
        err = host_env()
    else:
        sys.stderr.write('Command not found\n')
        err = 1
    return err


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(1)
