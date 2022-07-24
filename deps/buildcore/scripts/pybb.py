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
import shutil
import subprocess
import sys


def cat(paths: [str]) -> int:
    for path in paths:
        try:
            with open(path) as f:
                data = f.read()
                sys.stdout.write(data)
        except FileNotFoundError:
            sys.stderr.write('cat: {}: no such file or directory\n'.format(path))
            return 1
    sys.stdout.write('\n')
    return 0


def mkdir(path: str) -> int:
    if not os.path.exists(path):
        try:
            os.mkdir(path)
        except:
            return 1
        return 0
    if os.path.isdir(path):
        return 0
    return 1


# this exists because Windows is utterly incapable of providing a proper rm -rf
def rm(path: str) -> int:
    if (os.path.exists(path) or os.path.islink(path)) and not os.path.isdir(path):
        os.remove(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)
    return 0


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


def cmake_build(base_path: str, target: str) -> int:
    if not os.path.isdir(base_path):
        # nothing to build
        return 0
    for d in os.listdir(base_path):
        args = ['cmake', '--build', os.path.join(base_path, d)]
        if target is not None:
            args.extend(['--target', target])
        err = subprocess.run(args).returncode
        if err != 0:
            return err
    return 0


def conan() -> int:
    project_name = sys.argv[2]
    conan_dir = '.conanbuild'
    err = mkdir(conan_dir)
    if err != 0:
        return err
    args = ['conan', 'install', '../', '--build=missing', '-pr', project_name]
    os.chdir(conan_dir)
    err = subprocess.run(args).returncode
    if err != 0:
        return err
    return 0


def main():
    err = 0
    if sys.argv[1] == 'mkdir':
        err = mkdir(sys.argv[2])
    elif sys.argv[1] == 'rm':
        for i in range(2, len(sys.argv)):
            rm(sys.argv[i])
    elif sys.argv[1] == 'conan-install':
        err = conan()
    elif sys.argv[1] == 'ctest-all':
        err = ctest_all()
    elif sys.argv[1] == 'cmake-build':
        err = cmake_build(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    elif sys.argv[1] == 'cat':
        err = cat(sys.argv[2:])
    else:
        sys.stderr.write('Command not found\n')
        err = 1
    return err


if __name__ == '__main__':
    try:
        err = main()
        sys.exit(err)
    except KeyboardInterrupt:
        sys.exit(1)
