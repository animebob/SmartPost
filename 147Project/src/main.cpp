#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <az_core.h>
#include <esp32_azureiotkit_sensors.h>
#include "iot_configs.h"
#include <WiFi.h>
#include <Esp32MQTTClient.h>
#include <WiFiClientSecure.h>

//Azure IOT code below obtained from https://github.com/critchards/ESP32-Azure-Iot-Central/blob/main/src/main.cpp with some changes to fit our project
#define INTERVAL 10000        //time between messages sent to Azure IoT Hub
#define MESSAGE_MAX_LEN 256   //changes the maximum size of the message that can be sent


//Credentials taken from configs.h
const char* ssid     = IOT_CONFIG_WIFI_SSID;
const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* connectionString = DEVICE_CONNECTION_STRING;

const char *messageData = "{\"messageId\":%d, \"Weight\":%f, \"OpenStatus:\":%d}";  //Structure of the message sent to Azure IoT Hub
static bool hasIoTHub = false;
static bool hasWifi = false;
int messageCount = 1;
static bool messageSending = true;
static uint64_t send_interval_ms;



//this function will run when Azure IoT confirms it has recieved a message from the device
static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    Serial.println("Send Confirmation Callback finished.");
  }
}

//this function will run when Azure confirms it has sent a message to the device
static void MessageCallback(const char* payLoad, int size)
{
  Serial.println("Message callback:");
  Serial.println(payLoad);
}

//will run when device twin activity performed
static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  // Display Twin message.
  Serial.println("Device twin callback active");
  Serial.println(temp);
  free(temp);
}

//will run when a message is recieved on the device from Azure IoT Hub. This is where we can make our device react to input from the Cloud.
static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);                           //methodName is the Name* of the command sent from IoT Central.
  const char *responseMessage = "\"Successfully invoke device method\"";    //must be a properlly formatted JSON message. Azure IoT used this to confirm message received
  int result = 200;                                                         //200 is good, 400 is bad. https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-mqtt-support

  //we can check the methodName for known commands and react when they match
  
  if (strcmp(methodName, "start") == 0)     //looking for a message coming in under the command named start
  {
    LogInfo("Start sending data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)   //looking for a message coming in under the command named stop
  {
    LogInfo("Stop sending data");
    messageSending = false;
  }
  else if (strcmp(methodName, "echo") == 0)   //looking for a message coming in under the command named echo
  {
    Serial.println("echo command detected");
    Serial.print("Executed direct method payload: ");
    Serial.println((const char *)payload);

  }

  else
  {
    LogInfo("No method %s found", methodName);    //if a message comes in from an unrecognized command, go here
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage);       //originally had a +1 on it, messed up the JSON, made Azure IoT angry
  *response = (unsigned char *)strdup(responseMessage);

  return result;                                  //return the status code
}


const int LIGHT_SENS = 33;
const int LOADCELL_DOUT_PIN = 15;
const int LOADCELL_SCK_PIN = 13;

HX711 scale;

void setup() {
  Serial.begin(9600);

  Serial.println(" > WiFi");
  Serial.println("Starting connecting WiFi.");

  //initialize the wifi connection using the credentials fron iot_config.h
  delay(10);
  WiFi.mode(WIFI_AP);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    hasWifi = false;
  }
  hasWifi = true;
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(" > IoT Hub");
  
  //connect to IoT Hub using the connection string from iot_config.h
  if (!Esp32MQTTClient_Init((const uint8_t*)connectionString, true))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }
  hasIoTHub = true;

  //set up all the callback functions, all the subscriptions are handled in ESP32MQTTCLIENT
  Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  Esp32MQTTClient_SetMessageCallback(MessageCallback);
  Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
  Serial.println("Start sending events.");
  send_interval_ms = millis();              //state machine timer for sending telemetry

  pinMode(LIGHT_SENS, INPUT);
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
  int weight = 0;

  Serial.print("Door Status: ");
  curr_light = analogRead(LIGHT_SENS);
  if (curr_light > threshold) {
    door = true;
    Serial.println("Open");
  } else {
    Serial.println("Closed");
  }

  Serial.print("Current Weight: ");
  weight = scale.get_units(10);
  Serial.println(weight, 5);
  Serial.println("");
  
 if (hasWifi && hasIoTHub)
  {
    if (messageSending && (int)(millis() - send_interval_ms) >= INTERVAL)
    {  
      char messagePayload[MESSAGE_MAX_LEN];         //create an array of characters to hold the message that will be sent to Azure IoT Hub
      snprintf(messagePayload, MESSAGE_MAX_LEN, messageData, messageCount++, weight, door); //build the message from the function data and the measurements defined at the top (temp, humidity, led)
      Serial.println(messagePayload);                                                                     //write the message to the serial monitor for debugging
      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);                  //get ready to send a message to the MQTT broker
      Esp32MQTTClient_SendEventInstance(message);                                                         //send the message
      send_interval_ms = millis();                                                                        //update the state machine timer
    }
    else
    {
      Esp32MQTTClient_Check();                                                                            //keep the connection to Auzre IoT Hub alive even when not sending messages
    }
  }
  delay(1000);
}