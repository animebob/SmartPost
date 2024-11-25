#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"

const int LIGHT_SENS = 33;
const int LOADCELL_DOUT_PIN = 15;
const int LOADCELL_SCK_PIN = 13;

HX711 scale;

void setup() {
  pinMode(LIGHT_SENS, INPUT);
  Serial.begin(9600);
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_to_config(RTC_CPU_FREQ_80M, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
            
  scale.set_scale(217.396);   // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();               // reset the scale to 0
}
 
void loop() {
  int threshold = 0;
  int curr_light;
  bool door = false;

  Serial.print("Door Status: ");
  curr_light = analogRead(LIGHT_SENS);
  if (curr_light > threshold) {
    door = true;
    Serial.println("Open");
  } else {
    Serial.println("Closed");
  }

  Serial.print("Current Weight: ");
  Serial.println(scale.get_units(10), 5);
  Serial.println("");

// Load cell calibration
/*if (scale.is_ready()) {
    scale.set_scale();    
    Serial.println("Tare... remove any weights from the scale.");
    delay(5000);
    scale.tare();
    Serial.println("Tare done...");
    Serial.print("Place a known weight on the scale...");
    delay(5000);
    long reading = scale.get_units(10);
    Serial.print("Result: ");
    Serial.println(reading);
  } 
  else {
    Serial.println("HX711 not found.");
  }*/
  

  delay(1000);
}