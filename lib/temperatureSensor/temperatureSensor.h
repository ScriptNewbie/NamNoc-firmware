#include <Arduino.h>

class TemperatureSensor
{
public:
    void init();
    double getTemperature();

private:
    u_int8_t tempIndex = 0;
    double temps[3] = {0, 0, 0};
};
