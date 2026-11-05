#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(__AVR__)
#include <avr/interrupt.h>
#endif

#include "firmware_outline.h"

const uint8_t MUX_SELECT_PINS[3] = {2, 3, 4};
const uint8_t ADC_INPUT_PIN = A0;
const uint8_t FAULT_LED_PIN = 6;
const uint8_t I2C_ADDRESS = 0x08;

const float ADC_REFERENCE_VOLTAGE = 5.0f;
const float DIFFERENTIAL_REFERENCE_VOLTAGE = 2.5f;
const float CONDITIONING_GAIN = 3.9f;
const unsigned long MUX_SETTLING_DELAY_US = 2000UL;
const unsigned long LED_BLINK_INTERVAL_MS = 250UL;
const unsigned long SERIAL_REPORT_INTERVAL_MS = 500UL;
const unsigned long SERIAL_BAUD_RATE = 115200UL;

const uint8_t PACKET_SIZE = 24;
const uint8_t PROTOCOL_VERSION = 1;
const int16_t UNAVAILABLE_TEMPERATURE = INT16_MIN;

const float SENSOR_TEMPERATURES_C[] = {
  -40.0f, -35.0f, -30.0f, -25.0f, -20.0f, -15.0f, -10.0f, -5.0f,
  0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f,
  40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f, 70.0f, 75.0f,
  80.0f, 85.0f, 90.0f, 95.0f, 100.0f, 105.0f, 110.0f, 115.0f,
  120.0f
};

const float SENSOR_VOLTAGES_V[] = {
  2.44f, 2.42f, 2.40f, 2.38f, 2.35f, 2.32f, 2.27f, 2.23f,
  2.17f, 2.11f, 2.00f, 1.99f, 1.92f, 1.86f, 1.80f, 1.74f,
  1.68f, 1.63f, 1.59f, 1.55f, 1.51f, 1.48f, 1.45f, 1.43f,
  1.40f, 1.38f, 1.37f, 1.35f, 1.34f, 1.33f, 1.32f, 1.31f,
  1.30f
};

const uint8_t SENSOR_TABLE_SIZE =
    sizeof(SENSOR_TEMPERATURES_C) / sizeof(SENSOR_TEMPERATURES_C[0]);

int raw_adc[NUM_SENSORS];
float channel_temperatures[NUM_SENSORS];
float published_maximum = NAN;
int published_hottest_channel = -1;
bool scan_ready = false;
bool invalid_channel_present = false;
uint16_t scan_sequence = 0;
unsigned long last_led_toggle_ms = 0;
unsigned long last_serial_report_ms = 0;
bool led_state = false;
volatile uint8_t published_packet[PACKET_SIZE] = {};

bool valid_channel(int channel) {
  return channel >= 0 && channel < NUM_SENSORS;
}

void select_channel(int channel) {
  if (!valid_channel(channel)) {
    return;
  }

  for (uint8_t bit = 0; bit < 3; ++bit) {
    digitalWrite(MUX_SELECT_PINS[bit], (channel >> bit) & 0x01);
  }
}

int read_adc(void) {
  return analogRead(ADC_INPUT_PIN);
}

float adc_to_voltage(int reading) {
  if (reading < 0 || reading > 1023) {
    return NAN;
  }
  return (static_cast<float>(reading) * ADC_REFERENCE_VOLTAGE) / 1023.0f;
}

float voltage_to_temperature(float voltage) {
  if (!isfinite(voltage) ||
      voltage < SENSOR_VOLTAGES_V[SENSOR_TABLE_SIZE - 1] ||
      voltage > SENSOR_VOLTAGES_V[0]) {
    return NAN;
  }

  for (uint8_t index = 0; index < SENSOR_TABLE_SIZE - 1; ++index) {
    const float high_voltage = SENSOR_VOLTAGES_V[index];
    const float low_voltage = SENSOR_VOLTAGES_V[index + 1];
    if (voltage <= high_voltage && voltage >= low_voltage) {
      const float fraction =
          (high_voltage - voltage) / (high_voltage - low_voltage);
      return SENSOR_TEMPERATURES_C[index] +
             fraction *
                 (SENSOR_TEMPERATURES_C[index + 1] -
                  SENSOR_TEMPERATURES_C[index]);
    }
  }

  return NAN;
}

void scan_temperatures(float temperatures[NUM_SENSORS]) {
  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    select_channel(channel);
    delayMicroseconds(MUX_SETTLING_DELAY_US);
    raw_adc[channel] = read_adc();

    temperatures[channel] = voltage_to_temperature(
        DIFFERENTIAL_REFERENCE_VOLTAGE -
        adc_to_voltage(raw_adc[channel]) / CONDITIONING_GAIN);
  }
}

float max_temperature(const float temperatures[NUM_SENSORS]) {
  float maximum = NAN;
  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    if (isfinite(temperatures[channel]) &&
        (!isfinite(maximum) || temperatures[channel] > maximum)) {
      maximum = temperatures[channel];
    }
  }
  return maximum;
}

int16_t encode_temperature(float temperature) {
  if (!isfinite(temperature)) {
    return UNAVAILABLE_TEMPERATURE;
  }
  const long rounded = lroundf(temperature * 100.0f);
  if (rounded < INT16_MIN + 1L || rounded > INT16_MAX) {
    return UNAVAILABLE_TEMPERATURE;
  }
  return static_cast<int16_t>(rounded);
}

void write_uint16_le(uint8_t *destination, uint16_t encoded) {
  destination[0] = static_cast<uint8_t>(encoded & 0xFF);
  destination[1] = static_cast<uint8_t>(encoded >> 8);
}

void build_packet(uint8_t packet[PACKET_SIZE]) {
  memset(packet, 0, PACKET_SIZE);
  packet[0] = PROTOCOL_VERSION;

  if (scan_ready) {
    packet[1] = 1;
    if (isfinite(published_maximum) && published_maximum > TEMP_THRESHOLD) {
      packet[1] |= 1 << 1;
    }
    if (invalid_channel_present) {
      packet[1] |= 1 << 2;
    }
    if (!isfinite(published_maximum)) {
      packet[1] |= 1 << 3;
    }
    for (int channel = 0; channel < NUM_SENSORS; ++channel) {
      if (!isfinite(channel_temperatures[channel])) {
        packet[2] |= 1 << channel;
      }
    }
  }

  packet[3] = published_hottest_channel < 0
                  ? 0xFF
                  : static_cast<uint8_t>(published_hottest_channel);
  write_uint16_le(packet + 4, scan_sequence);
  write_uint16_le(packet + 6, static_cast<uint16_t>(
                                   encode_temperature(published_maximum)));
  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    const int16_t temperature =
        scan_ready ? encode_temperature(channel_temperatures[channel])
                   : UNAVAILABLE_TEMPERATURE;
    write_uint16_le(packet + 8 + channel * 2,
                   static_cast<uint16_t>(temperature));
  }
}

void report_i2c(const float temperatures[NUM_SENSORS], float maximum) {
  (void)temperatures;
  (void)maximum;
  uint8_t packet[PACKET_SIZE];
  build_packet(packet);
#if defined(__AVR__)
  const uint8_t interrupt_state = SREG;
  cli();
  memcpy(const_cast<uint8_t *>(published_packet), packet, PACKET_SIZE);
  SREG = interrupt_state;
#else
  noInterrupts();
  memcpy(const_cast<uint8_t *>(published_packet), packet, PACKET_SIZE);
  interrupts();
#endif
}

void on_i2c_request(void) {
  Wire.write(const_cast<const uint8_t *>(published_packet), PACKET_SIZE);
}

void update_fault_led(float maximum) {
  const bool fault = scan_ready &&
                     (invalid_channel_present ||
                      (isfinite(maximum) && maximum > TEMP_THRESHOLD));
  if (!fault) {
    led_state = false;
    digitalWrite(FAULT_LED_PIN, LOW);
    return;
  }

  const unsigned long now = millis();
  if (now - last_led_toggle_ms >= LED_BLINK_INTERVAL_MS) {
    last_led_toggle_ms = now;
    led_state = !led_state;
    digitalWrite(FAULT_LED_PIN, led_state ? HIGH : LOW);
  }
}

void print_scan(void) {
  const bool overtemperature =
      isfinite(published_maximum) && published_maximum > TEMP_THRESHOLD;
  Serial.print(F("scan="));
  Serial.print(scan_sequence);
  Serial.print(F(" status="));
  if (!scan_ready) {
    Serial.print(F("NOT_READY"));
  } else {
    if (invalid_channel_present) Serial.print(F("INVALID "));
    if (overtemperature) Serial.print(F("OVERTEMP "));
    if (!invalid_channel_present && !overtemperature) Serial.print(F("OK"));
  }
  Serial.print(F(" max="));
  if (isfinite(published_maximum)) {
    Serial.print(published_maximum, 2);
    Serial.print(F("C ch="));
    Serial.print(published_hottest_channel);
  } else {
    Serial.print(F("INVALID"));
  }
  Serial.println();

  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    Serial.print(F("  ch"));
    Serial.print(channel);
    Serial.print(F(" adc="));
    Serial.print(raw_adc[channel]);
    Serial.print(F(" temp="));
    if (isfinite(channel_temperatures[channel])) {
      Serial.print(channel_temperatures[channel], 2);
      Serial.println(F("C"));
    } else {
      Serial.println(F("INVALID"));
    }
  }
}

void initialize_peripherals(void) {
  for (uint8_t bit = 0; bit < 3; ++bit) {
    pinMode(MUX_SELECT_PINS[bit], OUTPUT);
    digitalWrite(MUX_SELECT_PINS[bit], LOW);
  }
  pinMode(FAULT_LED_PIN, OUTPUT);
  digitalWrite(FAULT_LED_PIN, LOW);
  analogReference(DEFAULT);
  Serial.begin(SERIAL_BAUD_RATE);
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(on_i2c_request);

  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    raw_adc[channel] = 0;
    channel_temperatures[channel] = NAN;
  }
  Serial.println(F("RFR 8-channel scanner"));
  Serial.println(F("Provisional pins: mux S0/S1/S2=D2/D3/D4, ADC=A0, LED=D6"));
  Serial.println(F("Provisional model: Vsensor=2.5-(Vconditioned/3.9), Vref=5.0V"));
  Serial.println(F("Settling=2000us, I2C=0x08, packet=24 bytes"));
}

void setup(void) {
  initialize_peripherals();
}

void loop(void) {
  scan_temperatures(channel_temperatures);
  published_maximum = NAN;
  published_hottest_channel = -1;
  invalid_channel_present = false;
  for (int channel = 0; channel < NUM_SENSORS; ++channel) {
    if (!isfinite(channel_temperatures[channel])) {
      invalid_channel_present = true;
    } else if (!isfinite(published_maximum) ||
               channel_temperatures[channel] > published_maximum) {
      published_maximum = channel_temperatures[channel];
      published_hottest_channel = channel;
    }
  }
  scan_ready = true;
  ++scan_sequence;
  report_i2c(channel_temperatures, published_maximum);

  update_fault_led(published_maximum);
  const unsigned long now = millis();
  if (now - last_serial_report_ms >= SERIAL_REPORT_INTERVAL_MS) {
    last_serial_report_ms = now;
    print_scan();
  }
}