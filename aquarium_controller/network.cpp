
#include "network.h"

Network::Network(IPAddress address, String name, String token, int port)
  : deviceName(name), authToken(token), host(address), networkPort(port)
{
}

bool Network::setHostAddr(String addr) {
  parseSuccess = host.fromString("192.168.2.101");
  return parseSuccess;
}
void Network::setDeviceName(String name) {
  deviceName = name;
}

String Network::getDeviceName() {
  return deviceName;
}

void Network::setAuthToken(String token) {
  authToken = token;
}

String Network::getAuthToken() {
  return authToken;
}

void Network::setPort(int port){
  networkPort = port;
}

int Network::getPort(){
  return networkPort;
}

DynamicJsonDocument Network::makeRequest(String endpoint, String params, boolean auth, String type) {
  WiFiClient client;
  DynamicJsonDocument jsonDocument(1024);
  //JsonObject response = jsonDocument.createObject();
  if (!client.connect(host, networkPort)) {
    jsonDocument["error"] = "connection failed";
    return jsonDocument;
  }
  // We now create a URI for the request
  String url = "/";

  url += endpoint;

  url += "?device[name]=";
  url += urlencode(deviceName);
  url += params;
  Serial.println(url);
  Serial.println(authToken);
  if (auth) {
    client.print(type + " " + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "AUTHORIZATION: " + authToken + "\r\n" +
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
      client.stop();
      jsonDocument["error"] = "Client Timeout !";
      return jsonDocument;
    }
  }
  bool status_ok = false;
  while (client.available()) {
    String line = client.readStringUntil('\r');
    if (line.substring(9, 15) == "200 OK") {
      status_ok = true;
    }
    else if (line.substring(9, 25) == "401 Unauthorized") {
      status_ok = false;
      jsonDocument["error"] = "401 Unauthorized";
      return jsonDocument;
    }
    //JsonObject root = jsonBuffer.parseObject(line);
    DeserializationError error = deserializeJson(jsonDocument, line);
    // Test if parsing succeeds.
    if (!error && line.toInt() == 0) {
      return jsonDocument;
    }
  }
  Serial.println("error");
  jsonDocument["error"] = "No JSON data";
  return jsonDocument;
}
