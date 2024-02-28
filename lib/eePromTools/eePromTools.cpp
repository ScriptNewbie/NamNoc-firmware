#include "eepromTools.h"

String eepromRead(unsigned int startAddress, unsigned int endAddress) {
    EEPROM.begin(512);
    String value = "";
    char current;
    for(unsigned int i = startAddress; i < endAddress; ++i){
        current = EEPROM.read(i);
        if(current == '\0') break;
        value += current;
    }
    EEPROM.end();

    return value;
}

String eepromWrite(unsigned int startAddress, String value) {
    EEPROM.begin(512);
    for(unsigned int i = 0; i < value.length(); ++i){
        EEPROM.write(startAddress + i, value[i]);
    }
    EEPROM.write(startAddress + value.length(), '\0');
    EEPROM.commit();
    EEPROM.end();

    return value;
}
