
/*
 =================================== 
 == High Temp Thermostat by VariTrak
 == *fw: v1.0
 == *2022.07.27.
 ===================================
 board: Arduino Nano m328
 build: Arduino 1.8.13
 used external libraries:
   Adafruit SSD1306
   MAX6675 library
   RunningAverege
   Bounce2
*/

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>
#include <RunningAverage.h>
#include <Bounce2.h>
#include <EEPROM.h>

const uint8_t buttonSet = A1;
const uint8_t buttonMinus = A0;
const uint8_t buttonPlus = A2;
const uint8_t thermoCS = 2;
const uint8_t thermoCLK = 3;
const uint8_t thermoDO = 4;
const uint8_t relayPin = 8;
const bool relayActiveLow = true;

Adafruit_SSD1306 display(128, 32, &Wire, -1);
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
RunningAverage temp_meter(10);
Button btn_set = Button();
Button btn_plus = Button();
Button btn_minus = Button();
const uint16_t btn_press_time = 3000;
const uint16_t mode_timeout = 5000;
const uint16_t default_temp_setpoint = 90;
const uint8_t default_hysteresis_value = 5;

uint8_t mode = 0;
uint16_t last_press_ms = 0;
uint16_t temp_value = 0;

const uint16_t temp_setpoint_min = 0;
const uint16_t temp_setpoint_max = 1000;
const uint8_t hysteresis_value_min = 1;
const uint8_t hysteresis_value_max = 250;

uint16_t temp_setpoint = 90;
uint8_t hysteresis_value = 5;

bool hyster(uint16_t value, uint16_t setpoint, uint8_t hysteresis) {
  static bool _thermostat_result = false;
  if (_thermostat_result) {
    if ((int16_t)value < (int16_t)setpoint - hysteresis) {
      _thermostat_result = false; // ki
    }
  } else {
    if (value >= setpoint) {
      _thermostat_result = true; // be
    }
  }
  return _thermostat_result;
}

void tempRead() {
  static uint16_t _last_ms = 0;
  uint16_t current_ms = (uint16_t)millis();
  if (abs(current_ms - _last_ms) > 300){
    temp_meter.addValue(thermocouple.readCelsius());
    _last_ms = (uint16_t)millis();
  }
  temp_value = (uint16_t)temp_meter.getAverage();
}

void controlLogic() {
  tempRead();
  
  if (hyster(temp_value, temp_setpoint, hysteresis_value-1)) {
    // Relay Activate
    if (relayActiveLow) digitalWrite(relayPin, LOW);
    else digitalWrite(relayPin, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    if (mode == 0) display.invertDisplay(true);
  } else {
    // Relay Deactivate
    if (relayActiveLow) digitalWrite(relayPin, HIGH);
    else digitalWrite(relayPin, LOW);
    digitalWrite(LED_BUILTIN, LOW);
    if (mode == 0) display.invertDisplay(false);
  }
}

void modeTimer() {
  uint16_t current_ms = (uint16_t)millis();
  if (abs(current_ms - last_press_ms) > mode_timeout) {
    mode = 0;
  }
}

void incSetpoint(uint8_t value) {
  if (temp_setpoint + value < temp_setpoint_max) {
    temp_setpoint += value;
  } else if (temp_setpoint < temp_setpoint_max) {
    temp_setpoint++;
  } else {
    temp_setpoint = temp_setpoint_max;
  }
}

void decSetpoint(uint8_t value) {
  if ((int16_t)temp_setpoint - value > (int16_t)temp_setpoint_min) {
    temp_setpoint -= value;
  } else if (temp_setpoint > temp_setpoint_min) {
    temp_setpoint--;
  } else {
    temp_setpoint = temp_setpoint_min;
  }
}

void incHysteresis(uint8_t value) {
  if (hysteresis_value + value < hysteresis_value_max) {
    hysteresis_value += value;
  } else if (hysteresis_value < hysteresis_value_max) {
    hysteresis_value++;
  } else {
    hysteresis_value = hysteresis_value_max;
  }
}

void decHysteresis(uint8_t value) {
  if (hysteresis_value - value > hysteresis_value_min) {
    hysteresis_value -= value;
  } else if (hysteresis_value > hysteresis_value_min) {
    hysteresis_value--;
  } else {
    hysteresis_value = hysteresis_value_min;
  }
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relayPin, OUTPUT);
  if (relayActiveLow) digitalWrite(relayPin, HIGH);
  else digitalWrite(relayPin, LOW);

  btn_set.attach( buttonSet, INPUT );
  btn_set.interval(5);
  btn_set.setPressedState(LOW);
  btn_plus.attach( buttonPlus, INPUT );
  btn_plus.interval(5);
  btn_plus.setPressedState(LOW);
  btn_minus.attach( buttonMinus, INPUT );
  btn_minus.interval(5);
  btn_minus.setPressedState(LOW);
  
  // display init
  delay(2000);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[Error] no display attached!"));
  }
  display.clearDisplay();
  display.display();

  EEPROM.get( 0, temp_setpoint );
  EEPROM.get( 2, hysteresis_value );
  if (temp_setpoint == 0xffff) temp_setpoint = default_temp_setpoint;
  if (hysteresis_value == 0xff) hysteresis_value = default_hysteresis_value;

  bootLogo();
}

void loop() {

  btn_set.update();
  btn_plus.update();
  btn_minus.update();

  if (mode == 0) {
    modeScroll();
  }
  else if (mode == 1) {
    modeTemperature();
  }
  else if (mode == 2) {
    modeHysteresis();
  }
  else if(mode == 3) {
    modeReset();
  }

  modeTimer();
  
  controlLogic();
}

void bootLogo() {
  display.clearDisplay();
  for(int16_t i=max(display.width(),display.height())/2; i>0; i-=3) {
    display.fillCircle(display.width() / 2, display.height() / 2, i, SSD1306_INVERSE);
    display.display();
    delay(1);
  }
  delay(1000);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("High Temp Thermostat"));
  display.println(F(" fw: v1.0"));
  display.println(F(" date: 2022.07.27"));
  display.println(F("by VariTrak"));
  display.display();
  delay(4000);
}

void modeNext() {
  if (mode < 3) mode++;
  else mode = 0;
}

void modeScroll() {
  if (btn_set.pressed() || btn_plus.pressed() || btn_minus.pressed()) {
    modeNext();
    display.invertDisplay(false);
    last_press_ms = (uint16_t)millis();
  }
  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.print(temp_value);
  display.println(F(" C"));
  display.display();
}

void modeTemperature() {
  static bool _modified = false;
  
  if ( btn_set.pressed() ) {
    if (_modified) {
      EEPROM.put(0, temp_setpoint);
      _modified = false;
    }
    modeNext();
    last_press_ms = (uint16_t)millis();
  }
  if ( btn_plus.pressed() ) {
    incSetpoint(1);
    _modified = true;
    last_press_ms = (uint16_t)millis();
  }
  if ( btn_minus.pressed() ) {
    decSetpoint(1);
    _modified = true;
    last_press_ms = (uint16_t)millis();
  }
  if (btn_plus.isPressed()) {
    if (_modified && abs((uint16_t)millis() - last_press_ms) > 200 ) {
      last_press_ms = (uint16_t)millis();
      if (btn_plus.duration() > btn_press_time && temp_setpoint%10==0) {
        incSetpoint(10);
      } else {
        incSetpoint(1);
      }
    }
  }
  if (btn_minus.isPressed()) {
    if (_modified && abs((uint16_t)millis() - last_press_ms) > 200 ) {
      last_press_ms = (uint16_t)millis();
      if (btn_minus.duration() > btn_press_time && temp_setpoint%10==0) {
        decSetpoint(10);
      } else {
        decSetpoint(1);
      }
    }
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Setpoint"));
  display.print(F("   "));
  display.print(temp_setpoint);
  display.println(F(" C"));
  display.display();
}

void modeHysteresis() {
  static bool _modified = false;
  if ( btn_set.pressed() ) {
    if (_modified) {
      EEPROM.put(2, hysteresis_value);
      _modified = false;
    }
    modeNext();
    last_press_ms = (uint16_t)millis();
  }
  if ( btn_plus.pressed() ) {
    incHysteresis(1);
    _modified = true;
    last_press_ms = (uint16_t)millis();
  }
  if ( btn_minus.pressed() ) {
    decHysteresis(1);
    _modified = true;
    last_press_ms = (uint16_t)millis();
  }

  if (btn_plus.isPressed()) {
    if (_modified && abs((uint16_t)millis() - last_press_ms) > 200 ) {
      last_press_ms = (uint16_t)millis();
      if (btn_plus.duration() > btn_press_time && hysteresis_value%10==0) {
        incHysteresis(10);
      } else {
        incHysteresis(1);
      }
    }
  }
  if (btn_minus.isPressed()) {
    if (_modified && abs((uint16_t)millis() - last_press_ms) > 200 ) {
      last_press_ms = (uint16_t)millis();
      if (btn_minus.duration() > btn_press_time && hysteresis_value%10==0) {
        decHysteresis(10);
      } else {
        decHysteresis(1);
      }
    }
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Hysteresis"));
  display.print(F("   "));
  display.print(hysteresis_value);
  display.println(F(" C"));
  display.display();
}

void modeReset() {
  if ( btn_set.pressed() ) {
    modeNext();
    last_press_ms = (uint16_t)millis();
  }
  if ( btn_plus.pressed() || btn_minus.pressed() ) {
    hysteresis_value = default_hysteresis_value;
    temp_setpoint = default_temp_setpoint;
    EEPROM.put(0, temp_setpoint);
    EEPROM.put(2, hysteresis_value);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Reset"));
    display.println(F("Succesful"));
    display.display();
    delay(1000);
    modeNext();
    last_press_ms = (uint16_t)millis();
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Reset?"));
  display.println(F("Yes No Yes"));
  display.display();
}
