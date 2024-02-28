#pragma once
#include <Arduino.h>
#include <EEPROM.h>

String eepromRead(u_int8_t startAddress, u_int8_t endAddress);
String eepromWrite(u_int8_t startAddress, String value);