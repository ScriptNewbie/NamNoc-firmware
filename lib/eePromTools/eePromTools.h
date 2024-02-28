#pragma once
#include <Arduino.h>
#include <EEPROM.h>

String eepromRead(unsigned int startAddress, unsigned int endAddress);
String eepromWrite(unsigned int startAddress, String value);