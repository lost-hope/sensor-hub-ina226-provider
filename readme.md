# INA226 Sensor Provider

A [Sensor Hub](../sensor-hub/readme.md) provider usermod for the TI
INA226 current/voltage/power monitor - registers `ina226_voltage` (V),
`ina226_current` (A) and `ina226_power` (W) with the hub by default, which
then handles MQTT, Home Assistant discovery, the JSON API and the Info
tab. Power is computed as voltage × current rather than trusting the
library's own `getBusPower()`, to sidestep any ambiguity about its return
unit.

## Hardware

Wire SDA/SCL to the I2C pins configured on WLED's own **Config > LED
Preferences** page (shared across all I2C usermods, address configurable
in this usermod's settings, default `0x40`). This usermod does not call
`Wire.begin()` itself. Presence is checked with a plain I2C address probe
(rather than relying on the library's own return values, which don't
reliably flag a disconnected sensor); retries every 10s if not found.

## Usage

Self-contained out-of-tree usermod (see `library.json` for its
`wollewald/INA226_WE` dependency). Add it to `custom_usermods` next to the
[Sensor Hub](../sensor-hub/readme.md) itself.

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch (also auto-disabled if I2C pins aren't configured) |
| Address | 0x40 | I2C address (set via the INA226's ADDR pins) |
| Check interval | 10s | How often the sensor is read |
| Name prefix | `ina226` | Sensor names become `<prefix>_voltage/_current/_power` - must be unique across every provider registered with the hub |
| Precision | 3 | Decimal places published for all three readings |
