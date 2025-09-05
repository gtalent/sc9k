#! /usr/bin/env bash

set -eu

dist_dir=.

install -Dm 755 $dist_dir/bin/SlideController /usr/local/bin/SlideController
install -Dm 644 $dist_dir/share/applications/net.drinkingtea.sc9k.desktop /usr/share/applications/net.drinkingtea.sc9k.desktop
cp -r $dist_dir/share/ /usr/

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache /usr/share/icons/hicolor
fi
