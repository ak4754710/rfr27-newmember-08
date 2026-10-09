#ifndef FIRMWARE_OUTLINE_H
#define FIRMWARE_OUTLINE_H

#define SENSOR_COUNT 16
#define FAULT_THRESHOLD_C 60.0f

void initialize_peripherals(void);
void select_channel(int channel);
int read_adc(void);
float adc_to_voltage(int reading);
float voltage_to_temperature(float voltage);
void scan_temperatures(float temperatures[SENSOR_COUNT]);
float max_temperature(const float temperatures[SENSOR_COUNT]);
void update_fault_led(float maximum);
void report_i2c(const float temperatures[SENSOR_COUNT], float maximum);

#endif
