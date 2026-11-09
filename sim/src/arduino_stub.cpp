#include "ArduinoStub.h"

#include "SPI.h"
#include "WString.h"
#include "Wire.h"

// Arduino's Stream::readStringUntil returns String. Nothing feeds the emulator
// a serial console, so both overloads return an empty one.
String Stream::readStringUntil(char) { return String(); }
String SerialStub::readStringUntil(char) { return String(); }

SerialStub Serial;
SerialStub Serial0;
SPIClass SPI;
TwoWire Wire;
