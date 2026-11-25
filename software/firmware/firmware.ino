#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "firmware_outline.h"

const uint8_t muxPins[] = {2, 3, 4};
const uint8_t ledPin = 6;
const float adcReference = 4.84f; // measure voltage of ADC reference with multimeter and set here

const int8_t TEMP_INVALID = -128;           // 0x80, means "no valid reading"
volatile uint8_t reply[] = {0, 0xFF, 0x80}; // {fault, hottest channel, temp}

const float SENSOR_TEMPERATURES_C[] = {
    -40.0f, -35.0f, -30.0f, -25.0f, -20.0f, -15.0f, -10.0f, -5.0f,
    0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f,
    40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f, 70.0f, 75.0f,
    80.0f, 85.0f, 90.0f, 95.0f, 100.0f, 105.0f, 110.0f, 115.0f,
    120.0f};

const float SENSOR_VOLTAGES_V[] = {
    2.44f, 2.42f, 2.40f, 2.38f, 2.35f, 2.32f, 2.27f, 2.23f,
    2.17f, 2.11f, 2.05f, 1.99f, 1.92f, 1.86f, 1.80f, 1.74f,
    1.68f, 1.63f, 1.59f, 1.55f, 1.51f, 1.48f, 1.45f, 1.43f,
    1.40f, 1.38f, 1.37f, 1.35f, 1.34f, 1.33f, 1.32f, 1.31f,
    1.30f};

const uint8_t SENSOR_TABLE_SIZE =
    sizeof(SENSOR_TEMPERATURES_C) / sizeof(SENSOR_TEMPERATURES_C[0]);

float voltage_to_temperature(float voltage)
{
  if (!isfinite(voltage) ||
      voltage < SENSOR_VOLTAGES_V[SENSOR_TABLE_SIZE - 1] ||
      voltage > SENSOR_VOLTAGES_V[0])
  {
    return NAN;
  }

  for (uint8_t index = 0; index < SENSOR_TABLE_SIZE - 1; ++index)
  {
    const float high_voltage = SENSOR_VOLTAGES_V[index];
    const float low_voltage = SENSOR_VOLTAGES_V[index + 1];
    if (voltage <= high_voltage && voltage >= low_voltage)
    {
      // linear interpolation eq.
      float v1 = SENSOR_VOLTAGES_V[index];
      float v2 = SENSOR_VOLTAGES_V[index + 1];
      float t1 = SENSOR_TEMPERATURES_C[index];
      float t2 = SENSOR_TEMPERATURES_C[index + 1];
      return t1 + (voltage - v1) * (t2 - t1) / (v2 - v1);
    }
  }

  return NAN;
}

void sendReply()
{
  const uint8_t packet[] = {reply[0], reply[1], reply[2]};
  Wire.write(packet, sizeof(packet));
}

void setup()
{
  for (uint8_t pin : muxPins)
  {
    pinMode(pin, OUTPUT);
  }
  pinMode(ledPin, OUTPUT);
  analogReference(DEFAULT);
  Serial.begin(115200);
  Wire.begin(0x08); // I2C slave address: 0x08 (lowest)
  Wire.onRequest(sendReply);
}

void loop()
{
  float highest = NAN;
  uint8_t hottestChannel = 0xFF;

  for (uint8_t channel = 0; channel < NUM_SENSORS; ++channel)
  {
    // set select pins to select channel
    for (uint8_t bit = 0; bit < 3; ++bit)
    {
      digitalWrite(muxPins[bit], (channel >> bit) & 1);
    }
    delay(1500); // settling time

    // map 0-1023 ADC value to voltage @ pin A0
    const float a0 = analogRead(A0) * adcReference / 1023.0f;

    // formula to convert voltage at pin A0 to voltage of diode (see design report hand calcs)
    const float diodeVoltage = 0.43824f * adcReference - 0.28735f * a0;
    const float temperature = voltage_to_temperature(diodeVoltage);

    Serial.print(channel);
    Serial.print(F(": "));
    Serial.println(temperature);

    if (isfinite(temperature) && (!isfinite(highest) || temperature > highest))
    {
      highest = temperature;
      hottestChannel = channel;
    }
  }

  const bool overtemperature = highest > TEMP_THRESHOLD;

  // 1 C per LSB, signed 8-bit (two's complement), clamped to -127..127
  int8_t encoded = TEMP_INVALID;
  if (isfinite(highest))
  {
    long rounded = lroundf(highest);
    if (rounded > 127)
      rounded = 127;
    if (rounded < -127)
      rounded = -127;
    encoded = static_cast<int8_t>(rounded);
  }

  // update all bytes together so an I2C request can't read a half-updated packet
  noInterrupts();
  reply[0] = overtemperature;
  reply[1] = overtemperature ? hottestChannel : 0xFF;
  reply[2] = static_cast<uint8_t>(encoded);
  interrupts();

  // I2C Response format (3 bytes):
  //    Byte 0: fault status (1 = over threshold, 0 = OK)
  //    Byte 1: index of the hottest channel (0xFF if fault status = 0)
  //    Byte 2: hottest temperature, signed int8, 1 C/LSB (0x80 = no valid reading)

  digitalWrite(ledPin, overtemperature && (millis() / 250 % 2));
}