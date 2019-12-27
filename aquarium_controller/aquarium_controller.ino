#include <NewPing.h>
#include <WiFi.h>
#include <Wire.h>
//#include <VL53L0X.h>
#include <DallasTemperature.h>               //dodaj biblitekę obsługującą DS18B20
#include "credentials.h"
#include <MedianFilter.h>
#include "network.h"


IPAddress host;
int echoPin = 35;
int trigPin = 5;
NewPing sonar(trigPin, echoPin, 50);
float pulseTime = 0;
float distance = 0;
int maxDistance = 9999;
int distanceTriggerCount = 0;
boolean waterInputRegulationOn = false;
boolean water_input_valve_on = false;
int measuredDistance = 0;

bool lidarNewData = false;
bool parseSuccess = host.fromString("192.168.2.101");
//const char * host = "192.168.2.101";
const int httpPort = 3001;
String authentication_token = "";
const char* device = "";
const char* json_buffer = "";
String name = "aquarium_controller_v2";
boolean loggedIn = false;
IPAddress water_input_valve_ip;
String valveKey = "13e13460f1728c111a68582fd5370a0b";


OneWire oneWire(23);
DallasTemperature sensors(&oneWire);         //informujemy Arduino, ze przy pomocy 1-Wire
Network* connection = new Network(host, name, authentication_token, httpPort);

//settings
float temperature_set = 0.0;
unsigned long turn_on_time = 0;
unsigned long turn_off_time = 0;
unsigned long current_time = 0;

unsigned long currentMillis = 0;
unsigned long previousReportMillis = 0;

unsigned int red_intensity = 0;
unsigned int green_intensity = 0;
unsigned int uv_intensity = 0;
unsigned int ww_intensity = 0;
unsigned int cw_intensity = 0;
unsigned int full_spectrum_intensity = 0;

const int ledFreq = 200;
const int ledResolution = 10;

int ch1Pin = 13;
int ch2Pin = 15;
int ch3Pin = 14;
int ch4Pin = 27;
int ch5Pin = 26;
int ch6Pin = 4;//25;
int ch7Pin = 33;
int ch8Pin = 32;


int wwChannel = 0;
int cwChannel = 1;
int greenChannel = 2;
int redChannel = 3;
int uvChannel = 4;
int fullSpectrumChannel = 5;

int adcPin = 36;

int CO2Pin = 19;
int heaterPin = 34;

boolean co2valve_on = false;


MedianFilter filterObject(41, 270);


DeviceAddress sensor1 = { 0x28, 0xC, 0x1, 0x7, 0xA0, 0x46, 0x1, 0xB1 };
DeviceAddress sensor2 = { 0x28, 0x2, 0x0, 0x7, 0x8F, 0x20, 0x1, 0x54 };

float temp0 = 0;
float temp1 = 0;
float temp2 = 0;

WiFiServer server(80);
String header;

void shouldTurnWaterInputOn(int distance) {
  if (waterInputRegulationOn) {
    Serial.println("waterInputRegulationOn");
    Serial.print("distance: ");
    Serial.println(distance);
    Serial.print("max distance: ");
    Serial.println(maxDistance);
    
    if (distance > maxDistance) {
      Serial.println("distance: " + String(distance) + " > " + String(maxDistance));
      distanceTriggerCount++;
      Serial.println("distance trigger count: " + String(distanceTriggerCount));
      if (distanceTriggerCount > 2) {
        Serial.println("valve input on:");
        water_input_valve_on = true;
      }
    }
    else {
      Serial.println("valve input off:");
      water_input_valve_on = false;
      distanceTriggerCount = 0;
    }
  }
  else {
    Serial.println("valve input off 2:");
    water_input_valve_on = false;
    distanceTriggerCount = 0;
  }
}

void decodeJsonObjectSettings(DynamicJsonDocument root){
  if (root.containsKey("settings")) {
    JsonObject settings = root["settings"];
    //settings.printTo(Serial);
    if (settings.containsKey("temperature_set")) {
      temperature_set = settings["temperature_set"];
    }
    if (settings.containsKey("intensity")) {
      JsonObject intensity = settings["intensity"];
      red_intensity = intensity["red"];
      green_intensity = intensity["green"];
      ww_intensity = intensity["ww"];
      cw_intensity = intensity["cw"];
      uv_intensity = intensity["uv"];
      full_spectrum_intensity = intensity["full_spectrum"];
      //intensity.printTo(Serial);
    }
    if (settings.containsKey("co2valve_on")) {
      co2valve_on = settings["co2valve_on"];
    }
    if (settings.containsKey("water_input_valve_on")) {
      waterInputRegulationOn = settings["water_input_valve_on"];
    }
    if (settings.containsKey("distance")) {
      maxDistance = settings["distance"];
    }
    if (settings.containsKey("connected_devices")) {
      JsonObject connected_devices = settings["connected_devices"];
      if (connected_devices.containsKey("water_input_valve")) {
        water_input_valve_ip.fromString(connected_devices["water_input_valve"].as<String>());
        Serial.println(water_input_valve_ip.toString());
      }
    }

  }
}

void reportData() {
  if (millis() >= previousReportMillis + 60000) {
    Serial.println("1");
    //sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println("2");
    //float temperature = sensors.getTempCByIndex(0);
    measuredDistance = sonar.ping_median(15);
    Serial.println("median: ");
    Serial.println(measuredDistance);

    Serial.println("3");
    String endpoint = "reports";
    String params = "";
    params += "&device[reports][temperature]=";
    params += urlencode(String(1));
    params += "&device[reports][distance]=";
    params += urlencode(String(measuredDistance));
    Serial.println("4");
    DynamicJsonDocument root = connection->makeRequest(endpoint, params,  true, "POST");

    decodeJsonObjectSettings(root);
    Serial.println("5");
    JsonVariant error = root["error"];
    if (!error) {
      previousReportMillis = millis();
    }
    else {
      loggedIn = false;
      Serial.println("Reporting failed!!!!!!!!!!!!!!!!!!");
    }

  }
}

void logIn() {
  String endpoint = "new_session";
  String params = "";
  params += "&device[password]=";
  params += urlencode(device_password);
  boolean requestSucceeded = false;
  
  DynamicJsonDocument root = connection->makeRequest(endpoint, params,  false, "GET");
  //root.printTo(Serial);
  JsonVariant error = root["error"];
  if (error) {
    Serial.println("login error");
    loggedIn = false;
  }
  JsonVariant authentication_token_variant = root["authentication_token"];
  String token = root["authentication_token"];

  if (!authentication_token_variant.isNull()) {
    String auth_token = root["authentication_token"];
    authentication_token = auth_token;
    connection->setAuthToken(authentication_token);
    loggedIn = true;
  }
  
}

void setLightPorts() {
  ledcWrite(wwChannel, ww_intensity);
  ledcWrite(cwChannel, cw_intensity);
  ledcWrite(greenChannel, green_intensity);
  ledcWrite(redChannel, red_intensity);
  ledcWrite(uvChannel, uv_intensity);
  ledcWrite(fullSpectrumChannel, full_spectrum_intensity);
}

void setValve() {
  if (co2valve_on == true) {
    digitalWrite(CO2Pin, HIGH);
  }
  else {
    digitalWrite(CO2Pin, LOW);
  }
}

//     This rutine is exicuted when you open its IP in browser
//===============================================================

//void handleUpdateIntensity() {
//  String message = "Body received:\n";
//  message += server.arg("plain");
//
//
//  const size_t bufferSize = JSON_OBJECT_SIZE(1) + 10;
//  DynamicJsonBuffer jsonBuffer(bufferSize);
//
//  String json = server.arg("plain");
//
//  JsonObject& root = jsonBuffer.parseObject(json);
//
//
//  server.send(200, "text/plain", "hello from esp8266!");
//  Serial.println("message:");
//  root.printTo(Serial);
//  Serial.println(message);
//}

bool updateWaterInputValve(bool isOpen) {
  const char* valve_ip = water_input_valve_ip.toString().c_str();
  if (strlen(valve_ip) > 0) {
    WiFiClient client;
    String valveParam = "";
    if (isOpen) {
      valveParam = "open";
    }
    else {
      valveParam = "close";
    }
    Serial.println("Connecting...");
    if (!client.connect(valve_ip, 80)) {
      Serial.println("updateWaterInputValve - connection failed: " + water_input_valve_ip.toString());
      return false;
    }
    if (client.connected()) {
      Serial.println("Posting valve data...");
      String host="http://" + water_input_valve_ip.toString();
      String PostData = "/valve?valve=" + valveParam;
      PostData += "&key=" + valveKey;
      Serial.println("Posting: " + PostData);
      client.print(String("GET ") + PostData + " HTTP/1.1\r\n" +
               "Host: " + String(WiFi.localIP()) + "\r\n" +
               "Connection: close\r\n\r\n");
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println(">>> Client Timeout !");
          client.stop();
          return false;
        }
      }

      long interval = 2000;
      unsigned long currentMillis = millis(), previousMillis = millis();
      
      while(!client.available()){
      
        if( (currentMillis - previousMillis) > interval ){   
          Serial.println("Timeout");
          client.stop();     
          return false;
        }
        currentMillis = millis();
      }
      
      while (client.available())
      {
        String line = client.readStringUntil('\r');
        Serial.println("Message from valve:");
        Serial.println(line);
      }
    }
  }
  return true;
}

void setup() {
  pinMode(ch1Pin, OUTPUT);
  pinMode(ch2Pin, OUTPUT);
  pinMode(ch3Pin, OUTPUT);
  pinMode(ch4Pin, OUTPUT);
  pinMode(ch5Pin, OUTPUT);
  pinMode(ch6Pin, OUTPUT);

  ledcSetup(wwChannel, ledFreq, ledResolution);
  ledcSetup(cwChannel, ledFreq, ledResolution);
  ledcSetup(greenChannel, ledFreq, ledResolution);
  ledcSetup(redChannel, ledFreq, ledResolution);
  ledcSetup(uvChannel, ledFreq, ledResolution);
  ledcSetup(fullSpectrumChannel, ledFreq, ledResolution);
  
  ledcAttachPin(ch1Pin, wwChannel);
  ledcAttachPin(ch2Pin, cwChannel);
  ledcAttachPin(ch3Pin, greenChannel);
  ledcAttachPin(ch4Pin, redChannel);
  ledcAttachPin(ch5Pin, uvChannel);
  ledcAttachPin(ch6Pin, fullSpectrumChannel);

  pinMode(CO2Pin, OUTPUT);
  pinMode(heaterPin, OUTPUT);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  setLightPorts();

  Serial.begin(9600);
  delay(10);
  Wire.begin(21, 22, 400000); // SDA (21), SCL (22) on ESP32, 400 kHz rate
  delay(500);
//  lidar.init();
//  lidar.setTimeout(500);
//   //increase timing budget to 200 ms
//  lidar.setMeasurementTimingBudget(20000);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.println("dupa");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA); // <<< Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  logIn();

  Serial.println(authentication_token);


  Serial.println();
  Serial.println("closing connection");
//  attachInterrupt(lidarIntPin, lidarIntHandler, FALLING);  // define interrupt for GPI01 pin output of VL53L0X
//  lidar.startContinuous(1000);
}

void loop() {
  if (loggedIn == false) {
    logIn();
  }
 
//  if (lidarNewData) {
//    measuredDistance = lidar.readRangeContinuousMillimeters();
//    if (lidar.timeoutOccurred()) { Serial.print("LIDAR TIMEOUT"); }
//    Serial.print("Distance: ");
//    Serial.println(measuredDistance);
//    lidarNewData = false;
//  }

  reportData();
  setLightPorts();

  setValve();
//  //server.handleClient();          //Handle client requests
  if (millis() >= previousReportMillis + 60000) {
    //Serial.println("millis() >= previousReportMillis + 60000");
    //Serial.println("millis: " + String(millis()));
    //Serial.println("previousReportMillis: " + String(previousReportMillis));
    shouldTurnWaterInputOn(measuredDistance);
    updateWaterInputValve(water_input_valve_on);
  }
//  else{
//    //Serial.println("not  -  - - - millis() >= previousReportMillis + 60000");
//    //Serial.println("millis: " + String(millis()));
//    //Serial.println("previousReportMillis: " + String(previousReportMillis));
//  }
}
