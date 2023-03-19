
# NMEATrax Change Log
All notable changes to this project will be documented in this file.
 
The format is based on [Keep a Changelog](http://keepachangelog.com/)
and this project tries to adhere to [Semantic Versioning](http://semver.org/).

## [Unreleased] - yyyy-mm-dd
 
### Coming Soon
- Fix WiFi settings not saving correctly

### In Progress


## [2.0.1] - 2023-03-18

### Fixed
- GPX now records 6 decimal points instead of 2
- Fixed bug where recording with no data would not let file be downloaded


## [2.0.0] - 2023-03-10

### Added
- Auto option to log recorder
- Record GPX
- Added version numbers to options page
- OTA updates (may have some bugs still)

### Changed
- Removed NMEA 0183
- Better gauges
- Stopped options page from continously getting state of recording
- Removed Fuel Pressure
- Moved NMEAloop() and webLoop() to timers
- Change recording interval based on speed or rpm
- Made all SD card functions require a file system to be supplied when called
- Updated SD card LED detection for hardware v2

### Fixed
- Fixed error when trying to download wifi.txt
 

## [1.0.0] - 2022-12-15
 
### Added
- All code from development repo. First publicly available code.
   
### Changed
 
### Fixed
