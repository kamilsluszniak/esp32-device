#ifndef NETWORK_H
#define NETWORK_H

#include "Arduino.h"
#include <urlencode.h>
#include <string.h>
#include <ArduinoJson.h>
#include "network.h"
#include <WiFi.h>

class Network
{
private:
  String deviceName;
  String authToken;
  IPAddress host;
  bool parseSuccess;
  int networkPort;
public:
  Network(IPAddress address, String name, String token, int port);
  bool setHostAddr(String addr);
  void setDeviceName(String name);
  String getDeviceName();
  void setAuthToken(String token);
  String getAuthToken();
  void setPort(int port);
  int getPort();
  DynamicJsonDocument makeRequest(String endpoint, String params, boolean auth, String type);

};

#endif
