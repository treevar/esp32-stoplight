//Copyright 2025-2026 Treevar
//All rights reserved
#ifndef WEB_PATH_H
#define WEB_PATH_H
#include <WiFi.h>
#include <stdint.h>
//Represents a path for the web server
struct WebPath{
  using method_t = uint8_t;
  //static void dummyCallback(WiFiClient&, method_t, const String&){}
  static const method_t NONE = 0;
  static const method_t GET = 1;
  static const method_t POST = 2;
  String path;
  //The func thats called when a req is made ot the path
  //Takes the client, method, and the variable string (everything after '?')
  void (*callback)(WiFiClient&, method_t, const String&);
  method_t methods; //Supported methods
  WebPath(String path, void (*callback)(WiFiClient&, method_t, const String&), method_t methods): 
    path(path), 
    callback(callback), 
    methods(methods)
  {}
  WebPath(): 
    path(), 
    callback(nullptr), 
    methods(NONE)
  {}
};
#endif //WEB_PATH_H