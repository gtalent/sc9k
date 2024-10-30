#! /usr/bin/env python3

#
#  Copyright 2016 - 2022 gary@drinkingtea.net
#
#  This Source Code Form is subject to the terms of the Mozilla Public
#  License, v. 2.0. If a copy of the MPL was not distributed with this
#  file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

import argparse
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-cpp', help='path to output cpp file')
    parser.add_argument('--out-hpp', help='path to output hpp file')
    args = parser.parse_args()
    return 0


if __name__ == '__main__':
    try:
        err = main()
        sys.exit(err)
    except KeyboardInterrupt:
        sys.exit(1)
