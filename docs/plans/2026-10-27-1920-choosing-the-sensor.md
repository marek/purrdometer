# Choosing the sensor, cadence or speed

Date: 2026-10-27 19:20 EDT

## Context

The sensor is chosen and checked from the dashboard's settings page, not over
SSH. The page lists the cadence and speed sensors the Pi hears, with their
type, signal and battery; one is chosen, or the first one heard is taken as
now. Speed sensors (wheel revolutions) work as well as cadence sensors (crank
revolutions): either way one revolution is one turn of the cat wheel. The
page shows how to mount the chosen type, with a picture, and a live test
counts rotations while the wheel is turned. The logger stays the only
program that talks Bluetooth and the only writer of the database.

## Tasks

- [x] Intent: the sensor choice, the sensor types, what the page shows and saves, how the logger is asked to scan
- [x] Logger: record every CSC sensor heard (name, signal, type from the CSC Feature), scan on request while connected
- [x] Logger: speed sensors' 32-bit wheel counter as well as cadence sensors' crank counter
- [x] Logger: follow `[sensor] address` from the settings, switching sensor without a restart
- [x] Settings page: the sensors heard and the choice, a scan button, mounting help with a picture per type, a live rotation test
- [x] Tests: wheel accounting and wrap, the sensor choice in the config, the page listing and saving a sensor
- [x] On whiskerwheel: the page lists the cadence sensor as connected with its battery, and choosing it holds without dropping the link
- [ ] The page's live test counts rotations while the wheel turns (the user)
