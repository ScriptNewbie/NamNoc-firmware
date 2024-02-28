#include "eepromTools.h"

String eepromRead(u_int8_t startAddress, u_int8_t endAddress) {
    EEPROM.begin(512);
    String value = "";
    char current;
    for(int i = startAddress; i < endAddress; ++i){
        current = EEPROM.read(i);
        if(current == '\0') break;
        value += current;
    }
    EEPROM.end();

    return value;
}

String eepromWrite(u_int8_t startAddress, String value) {
    EEPROM.begin(512);
    for(int i = 0; i < value.length(); ++i){
        EEPROM.write(startAddress + i, value[i]);
    }
    EEPROM.write(startAddress + value.length(), '\0');
    EEPROM.commit();
    EEPROM.end();

    return value;
}
