#ifndef FIRMWARE_OUTLINE_H
#define FIRMWARE_OUTLINE_H

#define NUM_SENSORS 16
#define TEMP_THRESHOLD 60.0f

void initialize_peripherals(void);
void select_channel(int channel);
int read_adc(void);
float adc_to_voltage(int reading);
float voltage_to_temperature(float voltage);
void scan_temperatures(float temperatures[NUM_SENSORS]);
float max_temperature(const float temperatures[NUM_SENSORS]);
void update_fault_led(float maximum);
void report_i2c(const float temperatures[NUM_SENSORS], float maximum);

#endif
