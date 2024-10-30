#
#  Copyright 2016 - 2021 gary@drinkingtea.net
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

import os
import platform
import shutil


def mkdir_p(path: str):
    if not os.path.exists(path):
        os.mkdir(path)


# this exists because Windows is utterly incapable of providing a proper rm -rf
def rm(path: str):
    file_exists = os.path.exists(path)
    is_link = os.path.islink(path)
    is_dir = os.path.isdir(path)
    if (file_exists or is_link) and not is_dir:
        os.remove(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)


def get_os() -> str:
    return platform.system().lower()


def get_arch() -> str:
    arch = platform.machine().lower()
    if arch == 'amd64':
        arch = 'x86_64'
    return arch
