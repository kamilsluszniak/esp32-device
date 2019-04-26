#include <ESP8266WiFi.h>
#include <Wire.h>
#include <DallasTemperature.h>               //dodaj biblitekę obsługującą DS18B20
#include <urlencode.h>
#include <string.h>
#include <ArduinoJson.h>
#include "credentials.h"
#include <MedianFilter.h>
#include <ESP8266WebServer.h>   // Include the WebServer library


const char* host = "192.168.2.101";
const int httpPort = 3001;
String authentication_token = "";
const char* device = "";
const char* json_buffer = "";
String name = "aquarium_controller";
boolean loggedIn = false;
String water_input_valve_ip = "";
String valveKey = "13e13460f1728c111a68582fd5370a0b";


OneWire oneWire(14);                   //wywołujemy transmisję 1-Wire na pinie 14 (D5)
DallasTemperature sensors(&oneWire);         //informujemy Arduino, ze przy pomocy 1-Wire

//settings
float temperature_set = 0.0;
unsigned long turn_on_time = 0;
unsigned long turn_off_time = 0;
unsigned long current_time = 0;

unsigned long currentMillis = 0;
unsigned long previousReportMillis = 0;

unsigned int red_intensity = 0;
unsigned int green_intensity = 0;
unsigned int blue_intensity = 0;
unsigned int white_intensity = 0;

int ch1Pin = 5;//D1
int ch2Pin = 4;//D2
int ch3Pin = 0;//D3
int ch4Pin = 2;//D4

int CO2Pin = 12;//D6
int heaterPin = 13;//D7

boolean co2valve_on = false;

int trigPin = 15;//D8
int echoPin = 16;//D0
float pulseTime = 0;
float distance = 0;
int maxDistance = 9999;
int distanceTriggerCount = 0;
boolean waterInputRegulationOn = false;
boolean water_input_valve_on = false;
int measuredDistance = 0;

MedianFilter filterObject(81, 11505);


int ledPin = 2; // D4
int ledPin2 = 12; // D6

DeviceAddress sensor1 = { 0x28, 0xC, 0x1, 0x7, 0xA0, 0x46, 0x1, 0xB1 };
DeviceAddress sensor2 = { 0x28, 0x2, 0x0, 0x7, 0x8F, 0x20, 0x1, 0x54 };

float temp0 = 0;
float temp1 = 0;
float temp2 = 0;


ESP8266WebServer server(80);
String header;


boolean makeRequest(String endpoint, String params, boolean auth, String type) {
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return false;
  }
  // We now create a URI for the request
  String url = "/";

  url += endpoint;

  url += "?device[name]=";
  url += urlencode(name);
  url += params;
  Serial.println(url);
  if (auth) {
    client.print(type + " " + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "AUTHORIZATION: " + authentication_token + "\r\n" +
                 "Connection: close\r\n\r\n");
  }
  else {
    client.print(type + " " + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
  }

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return false;
    }
  }
  bool status_ok = false;
  StaticJsonBuffer<700> jsonBuffer;
  while (client.available()) {
    String line = client.readStringUntil('\r');
    if (line.substring(9, 15) == "200 OK") {
      status_ok = true;
    }
    else if (line.substring(9, 25) == "401 Unauthorized") {
      status_ok = false;
      loggedIn = false;
    }
    JsonObject& root = jsonBuffer.parseObject(line);
    // Test if parsing succeeds.
    if (!root.success()) {

    }
    else {
      if (root.containsKey("authentication_token")) {
        String auth_token = root["authentication_token"];
        authentication_token = auth_token;
      }
      if (root.containsKey("settings")) {
        JsonObject& settings = root["settings"];
        if (settings.containsKey("temperature_set")) {
          temperature_set = settings["temperature_set"];
        }
        if (settings.containsKey("intensity")) {
          JsonObject& intensity = settings["intensity"];
          red_intensity = intensity["red"];
          green_intensity = intensity["green"];
          white_intensity = intensity["white"];
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
          JsonObject& connected_devices = settings["connected_devices"];
          if (connected_devices.containsKey("water_input_valve")) {
            water_input_valve_ip = connected_devices["water_input_valve"].as<String>();
          }
        }

      }
      loggedIn = true;
    }
  }
  return status_ok;
}

int measureDistance() {
  int i;
  for (i = 0; i < 81; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    pulseTime = pulseIn(echoPin, HIGH);
    distance = pulseTime * 340 / 200;

    filterObject.in(distance);
  }
  int measuredDistance = filterObject.out();
  return measuredDistance;
}

void shouldTurnWaterInputOn(int distance) {
  if (waterInputRegulationOn) {
    Serial.println("waterInputRegulationOn");
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
}


void reportData() {
  if (millis() >= previousReportMillis + 60000) {

    sensors.requestTemperatures(); // Send the command to get temperatures

    float temperature = sensors.getTempCByIndex(0);
    measuredDistance = measureDistance();

    String endpoint = "reports";
    String params = "";
    params += "&device[reports][temperature]=";
    params += urlencode(String(temperature));
    params += "&device[reports][distance]=";
    params += urlencode(String(measuredDistance));
    boolean requestSucceeded = makeRequest(endpoint, params,  true, "POST");
    if (requestSucceeded) {
      previousReportMillis = millis();
    }
    else {
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
  while (requestSucceeded == false) {
    requestSucceeded = makeRequest(endpoint, params,  false, "GET");
  }
}

void setLightPorts() {
  analogWrite(ch1Pin, red_intensity);
  analogWrite(ch2Pin, green_intensity);
  analogWrite(ch3Pin, white_intensity);
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

void handleUpdateIntensity() {
  String message = "Body received:\n";
  message += server.arg("plain");


  const size_t bufferSize = JSON_OBJECT_SIZE(1) + 10;
  DynamicJsonBuffer jsonBuffer(bufferSize);

  String json = server.arg("plain");

  JsonObject& root = jsonBuffer.parseObject(json);


  server.send(200, "text/plain", "hello from esp8266!");
  Serial.println("message:");
  root.printTo(Serial);
  Serial.println(message);
}

bool updateWaterInputValve(bool isOpen) {
  Serial.println("Water input valve ip: " + water_input_valve_ip);
  if (water_input_valve_ip.length() > 0) {
    WiFiClient client;
    String valveParam = "";
    if (isOpen) {
      valveParam = "open";
    }
    else {
      valveParam = "close";
    }
     Serial.println("Connecting...");
    if (!client.connect(water_input_valve_ip, 80)) {
      Serial.println("updateWaterInputValve - connection failed: " + water_input_valve_ip);
      return false;
    }
    if (client.connected()) {
      Serial.println("Posting valve data...");
      String host="http://" + water_input_valve_ip;
      String PostData = "/valve?valve=" + valveParam;
      PostData += "&key=" + valveKey;
      Serial.println("Posting: " + PostData);
      client.print(String("GET ") + PostData + " HTTP/1.1\r\n" +
               "Host: " + WiFi.localIP() + "\r\n" +
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
  analogWriteFreq(200);
  pinMode(ch1Pin, OUTPUT);
  pinMode(ch2Pin, OUTPUT);
  pinMode(ch3Pin, OUTPUT);
  pinMode(ch4Pin, OUTPUT);
  pinMode(CO2Pin, OUTPUT);
  pinMode(heaterPin, OUTPUT);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  setLightPorts();

  analogWrite(ch1Pin, 0);
  analogWrite(ch2Pin, 0);
  analogWrite(ch3Pin, 0);
  analogWrite(ch4Pin, 0);
  digitalWrite(CO2Pin, LOW);
  digitalWrite(heaterPin, LOW);

  Serial.begin(115200);
  delay(10);

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
}

void loop() {
  if (loggedIn == false) {
    logIn();
  }



  reportData();
  setLightPorts();
  setValve();
  server.handleClient();          //Handle client requests
  if (millis() >= previousReportMillis + 60000) {
    //Serial.println("millis() >= previousReportMillis + 60000");
    //Serial.println("millis: " + String(millis()));
    //Serial.println("previousReportMillis: " + String(previousReportMillis));
    shouldTurnWaterInputOn(measuredDistance);
    updateWaterInputValve(water_input_valve_on);
  }
  else{
    //Serial.println("not  -  - - - millis() >= previousReportMillis + 60000");
    //Serial.println("millis: " + String(millis()));
    //Serial.println("previousReportMillis: " + String(previousReportMillis));
  }
}




