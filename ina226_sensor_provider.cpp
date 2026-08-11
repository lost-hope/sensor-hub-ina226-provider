#include "wled.h"
#include "sensor_bus.h"
#include <INA226_WE.h>

/*
 * INA226 current/voltage/power sensor provider.
 *
 * Reads a TI INA226 over I2C (address 0x40 by default) and pushes bus
 * voltage, current and power into the Sensor Hub (see
 * ../sensor-hub/usermod_sensor_hub.cpp and ../sensor-hub/sensor_bus.h) as
 * "<prefix>_voltage", "<prefix>_current" and "<prefix>_power". Power is
 * computed here as voltage * current rather than trusted from the
 * library's own getBusPower(), to sidestep any ambiguity about its return
 * unit. This usermod never talks to MQTT, the JSON API or the Info tab
 * itself - the hub takes care of all of that once a sensor is registered
 * here.
 *
 * Wiring: SDA/SCL go to the I2C pins configured on WLED's own Config > LED
 * Preferences page (the shared "i2c_sda"/"i2c_scl" globals). WLED core
 * already calls Wire.begin() with those pins while loading cfg.json at
 * boot (wled00/cfg.cpp), before any usermod's setup() runs - so this
 * usermod only needs to confirm the pins are set, then use the shared Wire
 * bus. It must NOT call Wire.begin() itself.
 *
 * Presence is checked with a plain I2C address probe (rather than relying
 * on the INA226_WE library's own init()/read return values, which don't
 * reliably signal a disconnected sensor) so a missing/lost sensor is
 * detected the same robust way as the other providers in this repo.
 */
class INA226SensorUsermod : public Usermod {
  private:
    INA226_WE ina226 = INA226_WE(0x40);
    SensorHub* hub = nullptr;
    uint8_t voltageHandle = SENSOR_HANDLE_INVALID;
    uint8_t currentHandle = SENSOR_HANDLE_INVALID;
    uint8_t powerHandle = SENSOR_HANDLE_INVALID;

    bool enabled = true;
    bool sensorFound = false;
    bool initDone = false;

    unsigned long lastRead = 0;
    unsigned long lastBeginAttempt = 0;

    // config
    uint8_t i2cAddress = 0x40;
    uint16_t checkIntervalS = 10; // how often to read the sensor
    String namePrefix = "ina226"; // sensor names become "<prefix>_voltage/_current/_power"
    uint8_t precision = 3;        // decimal places published for all three readings

    static const char _name[];
    static const char _enabled[];
    static const char _address[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _precision[];

    bool probePresent() {
      Wire.beginTransmission(i2cAddress);
      return Wire.endTransmission() == 0;
    }

    void registerSensors() {
      if (!hub || voltageHandle != SENSOR_HANDLE_INVALID) return; // already registered
      voltageHandle = hub->registerSensor((namePrefix + "_voltage").c_str(), SensorType::VoltageV, nullptr, nullptr, precision);
      currentHandle = hub->registerSensor((namePrefix + "_current").c_str(), SensorType::Current,  nullptr, nullptr, precision);
      powerHandle   = hub->registerSensor((namePrefix + "_power").c_str(),   SensorType::Power,     nullptr, nullptr, precision);
    }

    void setSensorsAvailable(bool available) {
      if (!hub) return;
      if (voltageHandle != SENSOR_HANDLE_INVALID) hub->setSensorAvailable(voltageHandle, available);
      if (currentHandle != SENSOR_HANDLE_INVALID) hub->setSensorAvailable(currentHandle, available);
      if (powerHandle != SENSOR_HANDLE_INVALID)   hub->setSensorAvailable(powerHandle, available);
    }

  public:
    void setup() override {
      // I2C bus is configured (and Wire.begin() already called) via WLED's
      // own Config > LED Preferences page - nothing to do here if it's unset.
      if (i2c_sda < 0 || i2c_scl < 0) { enabled = false; return; }
      ina226 = INA226_WE(i2cAddress);
      sensorFound = probePresent();
      if (sensorFound) ina226.init();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();

      if (!sensorFound) {
        // sensor missing at boot (or lost) - keep retrying rather than giving up forever
        if (now - lastBeginAttempt < 10000) return;
        lastBeginAttempt = now;
        sensorFound = probePresent();
        if (!sensorFound) return;
        ina226.init();
      }

      if (now - lastRead < (unsigned long)checkIntervalS * 1000UL) return;
      lastRead = now;

      if (!probePresent()) {
        sensorFound = false; // force a fresh init() next loop
        setSensorsAvailable(false);
        return;
      }

      float v = ina226.getBusVoltage_V();
      float mA = ina226.getCurrent_mA();
      float a = mA / 1000.0f;
      float w = v * a;

      setSensorsAvailable(true);
      if (hub) {
        if (voltageHandle != SENSOR_HANDLE_INVALID) hub->updateSensor(voltageHandle, v);
        if (currentHandle != SENSOR_HANDLE_INVALID) hub->updateSensor(currentHandle, a);
        if (powerHandle != SENSOR_HANDLE_INVALID)   hub->updateSensor(powerHandle, w);
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_address)] = i2cAddress;
      top[FPSTR(_checkInterval)] = checkIntervalS;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_precision)] = precision;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_address)], i2cAddress);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalS);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_precision)], precision);
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('INA226Sensor:address',1,'I2C address, default 0x40=64');"));
      settingsScript.print(F("addInfo('INA226Sensor:checkInterval',1,'seconds between sensor reads');"));
      settingsScript.print(F("addInfo('INA226Sensor:namePrefix',1,'sensor names become &lt;prefix&gt;_voltage/_current/_power - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('INA226Sensor:precision',1,'decimal places published for all three readings');"));
    }
};

const char INA226SensorUsermod::_name[]          PROGMEM = "INA226Sensor";
const char INA226SensorUsermod::_enabled[]       PROGMEM = "enabled";
const char INA226SensorUsermod::_address[]       PROGMEM = "address";
const char INA226SensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char INA226SensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char INA226SensorUsermod::_precision[]     PROGMEM = "precision";

static INA226SensorUsermod ina226_sensor;
REGISTER_USERMOD(ina226_sensor);
