//Copyright 2025-2026 Treevar
//All rights reserved
#ifndef NETWORKING_H
#define NETWORKING_H
#include <WiFi.h>
#include "DataPoint.h"
#include "WebPath.h"
namespace Net{
  const char *HTTP_VER = "1.1";
  const char *HTTP_RES_OK = "200 OK";
  const char *HTTP_RES_NO_CONTENT = "204 No Content";
  const char *HTTP_RES_BAD_REQ = "400 Bad Request";
  const char *HTTP_RES_FORB = "403 Forbidden";
  const char *HTTP_RES_NOT_FOUND = "404 Not Found";
  const char *HTTP_RES_BAD_METH = "405 Method Not Allowed";
  const char *HTTP_RES_URI_LEN = "414 URI Too Long";
  const char *HTTP_RES_INTERN_ERR = "500 Internal Server Error";
  const char *HTTP_RES_NOT_IMPL = "501 Not Implemented";

  struct Config{
    String wifiSSID, wifiPswd;
    String apSSID, apPswd;
    IPAddress ip, gateway, subnet, dns;
  };

  Config config {
    .wifiSSID = "",
    .wifiPswd = "",
    .apSSID = "",
    .apPswd = "",
    .ip = (uint32_t)0,
    .gateway = (uint32_t)0,
    .subnet = (uint32_t)0,
    .dns = (uint32_t)0
  };

  //WiFiServer webServer{WEBSERVER_PORT};
  //bool webServerRunning = 0;
  const uint32_t MAX_REQ_BYTES = 512;

  //Returns the value of the key
  //Returns an empty string of the key wasnt found
  String getKeyValue(String key, const String &str){
    key += '='; //The keys have an = after them
    int keyIdx = str.lastIndexOf(key); //Last index to get the last value it was set to
    if(keyIdx == -1){
      return {};
    }
    int endIdx = str.indexOf('&', keyIdx); //Stop at start of next key
    if(endIdx == -1){
      endIdx = str.length();
    }
    return str.substring(keyIdx + key.length(), endIdx); 
  }

  //Reads from c until delim is found, no bytes are left, or maxSize is surpassed
  //Returns number of bytes read, -1 if maxSize was surpassed
  //If maxSize was surpassed out will not be modified
  int bufReadStringUntil(WiFiClient &c, String &out, char delim, size_t maxSize){
    char *buffer = new char[maxSize+1]; //+1 for null terminator
    buffer[maxSize] = '\0'; //Terminate end of buffer
    size_t bytesRead = c.readBytesUntil(delim, buffer, maxSize+1);
    if(bytesRead > maxSize){ //We surpassed maxSize
      delete[] buffer;
      return -1;
    }
    buffer[bytesRead] = '\0'; //Terminate string
    out = buffer;
    delete[] buffer;
    return bytesRead;
  }

  //Sends resposne header to the client
  //code is the response code
  //contentType is the type of the content (ie "text/html")
  //If allowedMethods is set then the 'Allow' header will be included with it
  void sendHeader(WiFiClient &client, const char *code, const char *contentType, uint8_t allowedMethods = WebPath::NONE){
    client.print("HTTP/");
    client.print(HTTP_VER);
    client.print(' ');
    client.println(code);
    client.print("Content-Type: ");
    client.print(contentType);
    client.println("; charset=utf-8");
    if(allowedMethods != WebPath::NONE){
      client.print("Allow: ");
      if(allowedMethods & WebPath::GET){
        client.print("GET");
        if(allowedMethods & WebPath::POST){
          client.print(", POST");
        }
      }
      else if(allowedMethods & WebPath::POST){ //Dont include the ', ' if post is the only method
        client.print("POST");
      }
      client.println();
    }
    client.println("Connection: close\n");
  }

  void sendHeaderAndBody(WiFiClient &client, const char *code){
    sendHeader(client, code, "text/plain");
  }

  //Returns whether the wifi is connected or not
  bool wifiConnected(){
    if(WiFi.status() != WL_CONNECTED){
      WiFiClient c{};
      //Sometimes the wifi will be connected but the status wont be WL_CONNECTED
      //This checks if we can make a connection
      c.connect(config.dns, 53); //Try to connect to dns server
      if(c.connected()){
        c.stop();
        return true;
      }
      return false;
    }
    return false;
  }

  String getChipID(){
    uint64_t mac = ESP.getEfuseMac();
    // Extract the last 2 bytes (4 hex characters) and format them
    char chipID[5];
    sprintf(chipID, "%04X", (uint16_t)(mac >> 32));
    return String(chipID);
  }

  bool isValidPswd(const String& pswd){
    //Invalid size
    if(pswd.length() < 8 || pswd.length() > 63){ return false; }
    //Invalid char
    char c;
    for(int i = 0; i < pswd.length(); ++i){
      c = pswd.charAt(i);
      if(c < 32 || c > 126){ return false; }
    }

    return true;
  }
  /*//Starts web server if it isnt already running
  void startWebServer(){
    if(webServerRunning){ return; }
    webServer.begin();
    webServerRunning = 1;
  }

  //Stops web server if it is running
  void stopWebServer(){
    if(!webServerRunning){ return; }
    webServer.end();
    webServerRunning = 0;
  }*/

  //Configures wifi based on the config flags
  /*void configWifi(){
    WiFi.setAutoReconnect(1);
    bool hostingNet = GET_FLAGS(controlFlags, ControlFlag::WIFI_HOSTING_NET);
    WiFi.mode(hostingNet ? WIFI_AP : WIFI_STA);
    if(!GET_FLAGS(controlFlags, ControlFlag::WIFI_USE_DHCP) && !hostingNet){ 
      WiFi.config(localIP, gateway, subnet, dns);
    }
  }*/

  //Creates a network with the given vars
  //Returns the creation was succesful
  bool createNetwork(const String &ssid, const String &pswd){
    WiFi.disconnect();
    //SET_FLAGS(controlFlags, WIFI_HOSTING_NET);
    if(!Net::isValidPswd(pswd)){
      print("Invalid wifi password");
    }
    else if(!WiFi.mode(WIFI_MODE_AP)){
      print("Error setting AP mode");
    }
    else if(!WiFi.softAP(ssid, pswd)){
      print("Error creating AP");
    }
    else{
      Serial.print("Created Network (\"");
      Serial.print(ssid);
      Serial.print("\":");
      Serial.print(pswd);
      Serial.println(')');
      Serial.print("Interface IP Address: ");
      Serial.println(WiFi.softAPIP());
      return true;
    }
    Serial.print(" -> Failed to create network");
    return false;
  }

  //Connects to wifi using the supplied creds
  //Returns whether it was able to connect
  bool connectToWifi(const String &ssid, const String &pswd, uint8_t tries){
    if(!WiFi.mode(WIFI_MODE_STA)){ return false; }
    Serial.print("Connecting to wifi");
    WiFi.begin(ssid, pswd);
    for(int i = 0; !wifiConnected() && i < tries; ++i){
      Serial.print('.');
      delay(1000);
    }
    if(wifiConnected()){
      Serial.println("\nConnected at " + WiFi.localIP().toString());
      return true;
    }
    Serial.print("\nUnable to connect to wifi, ");
    Serial.println("verify SSID and password");
    return false;
  }

  void wifiDisconnected(WiFiEvent_t e, WiFiEventInfo_t info){
    Serial.println("WiFi Disconnected");
    //connectToWifi(wifiSSID, wifiPswd, WIFI_CONN_TRIES);
  }

  //Disconnects a client
  void endClient(WiFiClient &client){
    client.flush();
    delay(10);
    client.stop();
  }
};
#endif //NETWORKING_H