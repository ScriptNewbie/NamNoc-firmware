#include "valve.h"
#include <Arduino.h>
#include <Ticker.h>

#define OPEN 5
#define CLOSE 4
#define DETECT 12

Ticker stop;
bool stopb = 0;
void stopcb()
{
    stopb = 1;
}

Valve::Valve()
{
    pinMode(DETECT, INPUT);
    pinMode(CLOSE, OUTPUT);
    digitalWrite(CLOSE, LOW);
    pinMode(OPEN, OUTPUT);
    digitalWrite(OPEN, LOW);
    open();
}

void Valve::open()
{
    digitalWrite(CLOSE, LOW);
    digitalWrite(OPEN, HIGH);
    opened = 1;
    stop.attach(4, stopcb);
    delay(500);
}
void Valve::close()
{
    digitalWrite(CLOSE, HIGH);
    digitalWrite(OPEN, LOW);
    opened = 0;
    stop.attach(5, stopcb);
    delay(500);
}
boolean Valve::isOpened()
{
    return opened;
};

void Valve::handlEvents()
{
    if (!digitalRead(DETECT) || stopb) // Stop the motor if end of rotation is detected or emergency ticker ticked.
    {
        stop.detach();
        digitalWrite(OPEN, LOW);
        digitalWrite(CLOSE, LOW);
        stopb = 0;
    }
}
