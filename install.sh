#! /usr/bin/env sh

dist_dir=.

apt-get install -y libqt6widgets6 libqt6network6 qt6-qpa-plugins

install -Dm 755 $dist_dir/bin/SlideController /usr/local/bin/SlideController
install -Dm 644 $dist_dir/share/applications/net.drinkingtea.sc9k.desktop /usr/share/applications/net.drinkingtea.sc9k.desktop

install -Dm 644 $dist_dir/share/icons/hicolor/16x16/apps/net.drinkingtea.sc9k.png   /usr/share/icons/hicolor/16x16/apps/net.drinkingtea.sc9k.png
install -Dm 644 $dist_dir/share/icons/hicolor/32x32/apps/net.drinkingtea.sc9k.png   /usr/share/icons/hicolor/32x32/apps/net.drinkingtea.sc9k.png
install -Dm 644 $dist_dir/share/icons/hicolor/64x64/apps/net.drinkingtea.sc9k.png   /usr/share/icons/hicolor/64x64/apps/net.drinkingtea.sc9k.png
install -Dm 644 $dist_dir/share/icons/hicolor/128x128/apps/net.drinkingtea.sc9k.png /usr/share/icons/hicolor/128x128/apps/net.drinkingtea.sc9k.png
install -Dm 644 $dist_dir/share/icons/hicolor/256x256/apps/net.drinkingtea.sc9k.png /usr/share/icons/hicolor/256x256/apps/net.drinkingtea.sc9k.png
install -Dm 644 $dist_dir/share/icons/hicolor/512x512/apps/net.drinkingtea.sc9k.png /usr/share/icons/hicolor/512x512/apps/net.drinkingtea.sc9k.png

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache /usr/share/icons/hicolor
fi
