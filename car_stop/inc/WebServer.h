//Copyright 2025-2026 Treevar
//All rights reserved
#ifndef WEBSERVER_H
#define WEBSERVER_H
#include "Networking.h"
class WebServer{
  public:
    static const uint MAX_PATHS{8};
    WebServer(uint16_t port, const String& host = String()):
      _port(port),
      _server(port),
      _host(host)
    {}
    void begin(){
      _server.begin();
    }
    bool addPath(const WebPath &path){
      if(_pathExists(path.path) || _pathCount == MAX_PATHS){ return false; }
      _paths[_pathCount++] = path;
      return true;
    }
    void processReq(){
      if(!_server.hasClient()){ return; }
      WiFiClient client{_server.accept()};
      if(!client.connected() || !client.available()){
        Net::endClient(client);
        return;
      }
      print("New connection: ");
      print(client.remoteIP().toString());
      print(':');
      print(client.remotePort());
      print('\n');
      uint8_t clientMethod = WebPath::NONE;
      //Read to the end of the uri
      //Doesn't use body to make hanlding variables simpler
      String req{};
      if(Net::bufReadStringUntil(client, req, '\n', Net::MAX_REQ_BYTES) == 0){ //URI was oversize
        Net::sendHeader(client, Net::HTTP_RES_URI_LEN, "text/plain");
        client.println(Net::HTTP_RES_URI_LEN);
        println("oversize");
      }
      else{
        //Find method
        if(req.indexOf("GET") == 0){
          clientMethod = WebPath::GET;
        }
        else if(req.indexOf("POST") == 0){
          clientMethod = WebPath::POST;
        }
        //Get path and vars
        int pathIdx = req.indexOf('/');
        int varIdx = req.indexOf('?', pathIdx);
        varIdx = varIdx < 0 ? req.indexOf(' ', pathIdx) : varIdx; //If there arent any vars set to the end of the uri
        String path = req.substring(pathIdx, varIdx);
        String vars {};
        if(-1 < varIdx){
          vars = req.substring(varIdx+1, req.indexOf(' ', varIdx));
        }
        String host {_getHost(client)};
        print("Method: ");
        println(clientMethod == WebPath::GET ? "GET" : "POST");
        print("Path: ");
        println(path);
        print("Host: ");
        print(host);
        if(!vars.isEmpty()){
          print("\nVars: ");
          print(vars);
        }
        println();
        //Serve path's page
        _servePage(client, clientMethod, path, vars, host);
      }
      Net::endClient(client);
    }
    uint8_t pathCount() const { return _pathCount; }
  private:
    WiFiServer _server;
    WebPath _paths[MAX_PATHS];
    String _host;
    uint8_t _pathCount;
    uint16_t _port;
    const char* _favicon;
    const char* _notFoundPage =
    #include "data/notFound.string"
    ;
    bool _pathExists(const String &path){
      for(int i = 0; i < _pathCount; ++i){
        if(_paths[i].path == path){ return true; }
      }
      return false;
    }
    String _getHost(WiFiClient &client){
      String line{};
      int res = false;
      bool maxReached = false;
      do{
        res = Net::bufReadStringUntil(client, line, '\n', Net::MAX_REQ_BYTES);
        if(res == -1){ maxReached = true; }
        else{ maxReached = false; }
        if(!maxReached && line.indexOf("Host: ") == 0){ //Host line
          line = line.substring(6);
          line.trim();
          return line;
        }
      } while(res > 0);
      return String();
    }
    void _servePage(WiFiClient &c, uint8_t method, const String &path, const String &vars, const String &host){
      uint8_t i = 0;
      if(host.length() > 0){
        if(host != _host && host != WiFi.softAPIP().toString()){
          i = _pathCount; //Skip looking through web paths as this is not intended for the regular webserver
        }
      }
      //Find the webpath
      for(; i < _pathCount; ++i){
        if(_paths[i].path == path){ //Found it
          if(_paths[i].methods & method == 0){ //Method not allowed
            Net::sendHeader(c, Net::HTTP_RES_BAD_METH, "text/plain", _paths[i].methods);
            c.println(Net::HTTP_RES_BAD_METH);
            return;
          }
          //Execute the webpage's callback function
          if(_paths[i].callback == nullptr){
            Net::sendHeader(c, Net::HTTP_RES_NOT_IMPL, "text/plain");
            c.println(Net::HTTP_RES_NOT_IMPL);
            return;
          }
          _paths[i].callback(c, method, vars);
          break;
        }
      }
      if(i >= _pathCount){ //Path not found
        //Trick devices into thinking they have internet
        if(path == "/generate_204"){ //Google
          Net::sendHeader(c, Net::HTTP_RES_NO_CONTENT, "text/plain");
        }
        else if(path == "/hotspot-detect.html" || path == "/test/success.html"){ //Apple
          Net::sendHeader(c, Net::HTTP_RES_OK, "text/html");
          c.print("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        }
        else if(path == "/connecttest.txt"){ //Microsoft
          Net::sendHeader(c, Net::HTTP_RES_OK, "text/plain");
          c.print("Microsoft Connect Test");
        }
        else if(path == "/success.txt"){
          Net::sendHeader(c, Net::HTTP_RES_OK, "text/plain");
          c.println("success");
        }
        else if(path == "/canonical.html"){
          Net::sendHeader(c, Net::HTTP_RES_OK, "text/html");
          c.print(R"(<meta http-equiv="refresh" content="0;url=https://support.mozilla.org/kb/captive-portal"/>)");
        }
        else{
          Net::sendHeader(c, Net::HTTP_RES_OK, "text/html");
          c.println(_notFoundPage);
        }
      }
    }
};
#endif //WEBSERVER_H