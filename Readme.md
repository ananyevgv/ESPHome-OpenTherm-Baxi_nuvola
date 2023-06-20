# ESPHome OpenTherm

[![build](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml/badge.svg)](https://github.com/rsciriano/ESPHome-OpenTherm/actions/workflows/build.yml)

This is an example of a integration with a OpenTherm boiler Baxi Nuvola-3 b40 using + decoding of boiler error codes [ESPHome](https://esphome.io/) 

## Installation
- Copy the content of this repository to your ESPHome folder
- Make sure the pin numbers are right, check the file opentherm_component.h in the esphome-opentherm folder.
- Edit the opentherm.yaml file:
    - Make sure the board and device settings are correct for your device
    - Set the sensor entity_id with the external temperature sensor's name from Home Assistant. (The ESPHome sensor name is temperature_sensor).
- Flash the ESP and configure in Home Assistant. It should be auto-discovered by the ESPHome Integration.

