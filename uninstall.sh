#! /usr/bin/env sh

rm -f /usr/local/bin/SlideController
rm -f /usr/share/applications/net.drinkingtea.sc9k.desktop
rm -f /usr/share/icons/hicolor/16x16/apps/net.drinkingtea.sc9k.png
rm -f /usr/share/icons/hicolor/32x32/apps/net.drinkingtea.sc9k.png
rm -f /usr/share/icons/hicolor/64x64/apps/net.drinkingtea.sc9k.png
rm -f /usr/share/icons/hicolor/128x128/apps/net.drinkingtea.sc9k.png
rm -f /usr/share/icons/hicolor/256x256/apps/net.drinkingtea.sc9k.png
rm -f /usr/share/icons/hicolor/512x512/apps/net.drinkingtea.sc9k.png

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
	gtk-update-icon-cache /usr/share/icons/hicolor
fi
