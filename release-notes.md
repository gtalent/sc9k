# 1.2.0

* Add support for an app defined camera preset system with more natural transitions
* Add manual focus option to camera presets
* Add camera calibration, which measures the camera's pan and tilt speeds so preset transitions move in a straight line
* Replace connection status text in status bar with colored indicators
* Add Power On, Power Off, and Reboot options to Camera menu
* Only enable OK and Apply buttons in settings dialog upon changes made
* Change OBS integration to use the builtin WebSocket API instead of custom script
* Change camera client to use VISCA
* Add Qt install to Linux install script
* Add uninstall script
* Fix issue with duplicate song names always showing the first one selected
* Fix settings saved with Apply not taking effect until restart if the settings dialog was then closed with Cancel
* Fix icon on Wayland

# 1.1.0

* Add Linux app icon, desktop file, and install script
* Rename MusicScene SlidesScene and SpeakerScene to NoSlidesScene
