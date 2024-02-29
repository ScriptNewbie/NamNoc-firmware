#include <OneWire.h>
#include <DallasTemperature.h>

#include "TemperatureSensor.h"

#define ONE_WIRE_BUS 13

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void TemperatureSensor::init()
{
    sensors.begin();
    sensors.requestTemperatures();
    float currentTemp = sensors.getTempCByIndex(0);

    for(u_int8_t i = 0; i < 3; ++i)
    {
        temps[i] = currentTemp;
    }
}

double TemperatureSensor::getTemperature()
{
    sensors.requestTemperatures();
    temps[tempIndex] = sensors.getTempCByIndex(0);
    tempIndex = (tempIndex + 1) % 3;
    
    double tempSum = 0;
    for (int i = 0; i < 3; ++i)
    {
      tempSum += temps[i];
    }

    return tempSum / 3;
}