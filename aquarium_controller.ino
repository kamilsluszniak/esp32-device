#include  <ESP8266WiFi.h>
#include <urlencode.h>
#include <string.h>
#include <ArduinoJson.h>
#include "credentials.h"
 
const char* host = "192.168.1.110";
const char* authentication_token = "";
const char* device = "";
const char* json_buffer = "";
String name = "aquarium_controller";
boolean loggedIn = false;


//settings
float temperature_set = 0.0;
unsigned long turn_on_time = 0;
unsigned long turn_off_time = 0;
unsigned long current_time = 0;
unsigned int red_intensity = 0;
unsigned int green_intensity = 0;
unsigned int blue_intensity = 0;
unsigned int white_intensity = 0;


int ledPin = 2; // D4
int ledPin2 = 12; // D6

boolean makeRequest(String endpoint, String params, boolean auth, String type){
  WiFiClient client;
  const int httpPort = 3001;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return false;
  }
  // We now create a URI for the request
  String url = "/";

  url += endpoint;

  url += "?device[name]=";
  url += urlencode(name);
  if (auth) {
    url += "&device[authentication_token]=";
    url += urlencode(authentication_token);
  }
  url += params;
  Serial.println(url);
  client.print(type + " " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
    }
  }
  bool status_ok = false;
  StaticJsonBuffer<500> jsonBuffer;
  while (client.available()) {
    String line = client.readStringUntil('\r');
    if (line.substring(9, 15) == "200 OK"){ status_ok = true;}
    else if (line.substring(9, 25) == "401 Unauthorized"){
      status_ok = false;
      loggedIn = false;
      }
    JsonObject& root = jsonBuffer.parseObject(line);
    // Test if parsing succeeds.
    if (!root.success()) {
      //root.printTo(Serial);

    }
    else{
      if (root.containsKey("authentication_token")){
        authentication_token = root["authentication_token"];
      }
      if (root.containsKey("settings")){
        JsonObject& settings = root["settings"];
        if (settings.containsKey("temperature_set")){
          temperature_set = settings["temperature_set"];
        }
        if (settings.containsKey("intensity")){
          JsonObject& intensity = settings["intensity"];
          red_intensity = intensity["red"];
          green_intensity = intensity["green"];
          blue_intensity = intensity["blue"];
          white_intensity = intensity["white"];
        }
        
      }
      if (root.containsKey("turn_on_time")){
        turn_on_time = root["turn_on_time"];
      }
      if (root.containsKey("turn_off_time")){
        turn_off_time = root["turn_off_time"];
      }

      loggedIn = true;
      Serial.println("root");
      root.printTo(Serial);
    }
  }
  return status_ok;
}


void reportTemperature(){
  String endpoint = "reports";
  String params = "";
  params += "&device[reports][temperature]=";
  params += urlencode(String(random(20, 30)));
  boolean requestSucceeded = makeRequest( endpoint, params,  true, "POST");
  Serial.println("intensity:");
  Serial.println(red_intensity);
  Serial.println(green_intensity);
  Serial.println(blue_intensity);
  Serial.println(white_intensity);
}

void logIn(){
  String endpoint = "new_session";
  String params = "";
  params += "&device[password]=";
  params += urlencode(device_password);
  boolean requestSucceeded = false;
  while (requestSucceeded == false){
    requestSucceeded = makeRequest( endpoint, params,  false, "GET");
    
  }

}

void setLightPorts(){
  analogWrite(D1, red_intensity);
  analogWrite(D2, green_intensity);
  analogWrite(D2, blue_intensity);
  analogWrite(D4, white_intensity);
}


void setup() {
  Serial.begin(115200);
  delay(10);
  
   
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, LOW);
   
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.println("dupa");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //WiFi.mode(WIFI_STA); // <<< Station
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
  delay(2000);
  reportTemperature();
  if (loggedIn == false){
     logIn();
  }
  setLightPorts();
  
}




