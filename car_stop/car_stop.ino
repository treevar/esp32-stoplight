//Copyright 2025-2026 Treevar
//All Rights Reserved
#define ENABLE_SERIAL true
#define HAS_COLOR true
#include "inc/Util.h"
#include "inc/Time.h"
#include "inc/TOFSensor.h"
#include "inc/NeoPixel.h"
#include "inc/LightRelay.h"
#include "inc/WebServer.h"
#include "inc/DataPointManager.h"
#include <limits.h>
#include <Preferences.h>
#include <DNSServer.h>

#define LED_PIN 16
#define LED_COUNT 26
#define LED_COLOR NeoPixel::RED
#define TIMING_BUDGET 100 //ms to read
#define LOOP_DELAY 50
#define READING_COUNT 5

#define RELAY_PIN 4

enum class Mode : uint8_t{
  REGULAR = 0,
  SELECT = 1,
  LIGHT = 2
};
const uint8_t MAX_MODE = 2;

//Scoped castable enum
namespace Feature{
  enum Feature{
    NONE = 0,
    COLOR = 1, //Whether the light has color (a neopixel strip)
    TOUCH = 2, //Whether there is a touch sensor to switch light
    WIFI = 4
  };
};

namespace Error{
  enum Error{
    NONE = 0,
    TOF_INIT_ERR = 1,
    LIGHT_INIT_ERR = 2,
    PREFERENCES_ERR = 4,
    DNS_ERR = 8
  };
};

//Preferences keys
const char *wifiPswdKey = "wifiPswd";
const char *thresholdKey = "threshold";

//Features of this build
uint32_t features = (HAS_COLOR ? Feature::COLOR : Feature::NONE) | Feature::WIFI;
uint32_t errors = Error::NONE;

NeoPixel lightStrip{LED_PIN, LED_COUNT, LED_COLOR};
LightRelay relay{RELAY_PIN};

TOFSensor tofSensor{TOFSensor::DistanceMode::Medium, READING_COUNT, TIMING_BUDGET, TOFSensor::Coord(4, 16)};

const Time LIGHT_ON_TIME {Time::second(5)};
const Time BLINK_DELAY {Time::second(1)};
const Time WIFI_PSWD_CHANGE_TIMEOUT {Time::minute(1)};

TOFSensor::Zone triggerZone{Convert::ftToMm(3), Convert::ftToMm(7)}; //Zone to trigger light
//Zone for detecting when an object left the zone
//Can't just invert becasue data less than the min is considered invalid
TOFSensor::Zone leaveZone{triggerZone.upper+1, UINT16_MAX}; 

bool waitForLeave {false}; //Wait for object to leave trigger zone before enabling light
Time wifiPswdChangeTime {Time::NULL_TIME};

#if HAS_COLOR
Light &light = lightStrip;
#else
Light &light = relay;
#endif

DNSServer dnsServer{};
WebServer server{80, "stop.light"};
const char* MAIN_HTML_DATA = 
#include "data/main.string"
;
const char* favicon = 
#include "data/favicon.string"
;

const String wifiSSID = "stoplight_" + Net::getChipID();
String wifiPswd = "lightpass";

TOFSensor::distance_t curDistance;
Time curTime;
Mode curMode = Mode::REGULAR;

DataPointManager dataPoints{};

Preferences prefs;

bool setWifiPswd(const String& pswd){
  if(WiFi.getMode() == WIFI_MODE_AP){
    if(Net::createNetwork(wifiSSID, pswd)){
      wifiPswd = pswd;
      wifiPswdChangeTime = Time::now(false);
      return true;
    }
    return false;
  }
  if(!Net::isValidPswd(pswd)){ return false; }
  wifiPswd = pswd;
  return true;
}

DataPoint::status_t setWifiPswdCallback(DataPoint::data_t data, DataPoint::Type type){
  if(type != DataPoint::STR){ return DataPoint::BAD; }
  String pswd = *static_cast<String*>(data);
  if(!setWifiPswd(pswd)){ return DataPoint::BAD; }
  return DataPoint::SET | DataPoint::OK;
}

bool setThreshold(TOFSensor::distance_t distance){
  const TOFSensor::distance_t minDist = Convert::ftToMm(1);
  const TOFSensor::distance_t rangingZone = Convert::ftToMm(2);
  if(distance < minDist || distance == UINT16_MAX){ return false; }
  triggerZone.upper = distance;
  triggerZone.lower = distance < rangingZone ? 0 : (distance - rangingZone);
  leaveZone.lower = distance + 1;
  prefs.putUShort(thresholdKey, triggerZone.upper);
  return true;
}

DataPoint::status_t setThresholdCallback(DataPoint::data_t data, DataPoint::Type type){
  if(type != DataPoint::UINT || data == nullptr){ return DataPoint::BAD; }
  uint32_t newThresh = *static_cast<uint32_t*>(data);
  bool threshSet = setThreshold(newThresh);
  DataPoint::status_t status = threshSet ? (DataPoint::OK | DataPoint::SET) : DataPoint::BAD;
  return status;
}

void setMode(Mode newMode){
  if(curMode == newMode){ return; }
  switch(newMode){
    case(Mode::REGULAR):
      light.off();
      break;
    case(Mode::LIGHT):
      lightStrip.show(0xFFFFFF);
      break;
    case(Mode::SELECT):
      light.on();
      break;
  }
  curMode = newMode;
}

DataPoint::status_t setModeCallback(DataPoint::data_t data, DataPoint::Type type){
  if(type != DataPoint::UINT8 || data == nullptr){ return DataPoint::BAD; }
  uint8_t newModeInt = *static_cast<uint8_t*>(data);
  if(newModeInt > MAX_MODE){ return DataPoint::BAD; }

  DataPoint::status_t status = DataPoint::OK;
  Mode newMode = static_cast<Mode>(newModeInt);
  setMode(newMode);
  status |= DataPoint::SET;
  return status;
}

void mainPageCallback(WiFiClient& client, WebPath::method_t method, const String& vars){
  Net::sendHeader(client, Net::HTTP_RES_OK, "text/html");
  client.print(MAIN_HTML_DATA);
}

//Body will contain new value if set
void setPageCallback(WiFiClient& client, WebPath::method_t method, const String& vars){
  String dataPointName {Net::getKeyValue("dp", vars)};
  String value {Net::getKeyValue("val", vars)};
  if(dataPointName.isEmpty() || value.isEmpty()){
    Net::sendHeaderAndBody(client, Net::HTTP_RES_BAD_REQ);
    return;
  }

  if(dataPoints.set(dataPointName, value) == DataPoint::BAD){ //Invalid value
    Net::sendHeaderAndBody(client, Net::HTTP_RES_BAD_REQ); 
    return;
  }

  Net::sendHeader(client, Net::HTTP_RES_OK, "text/plain");
  client.println(dataPoints.get(dataPointName).toString(true));
}

//Body contains only teh value of teh datapoint
void getPageCallback(WiFiClient& client, WebPath::method_t method, const String& vars){
  String dataPointName {Net::getKeyValue("dp", vars)};
  if(dataPointName.isEmpty()){
    Net::sendHeaderAndBody(client, Net::HTTP_RES_BAD_REQ);
    return;
  }
  const DataPoint& dp {dataPoints.get(dataPointName)};
  if(dp == DataPoint::NULL_DATAPOINT){
    Net::sendHeaderAndBody(client, Net::HTTP_RES_BAD_REQ);
    return;
  }
  Net::sendHeader(client, Net::HTTP_RES_OK, "text/plain");
  client.println(dp.toString(true));
}

//Json array of all datapoints
void getAllPageCallback(WiFiClient& client, WebPath::method_t method, const String& vars){
  Net::sendHeader(client, Net::HTTP_RES_OK, "application/json");
  client.println('{');
  uint8_t dpCount = dataPoints.count();
  for(uint8_t i = 0; i < dpCount - 1; ++i){
    client.print(dataPoints.get(i).toJSON(true));
    client.println(',');
  }
  client.println(dataPoints.get(dpCount-1).toJSON(true));
  client.println('}');
}

void tryColorCallback(WiFiClient& client, WebPath::method_t method, const String& vars){
  String value {Net::getKeyValue("val", vars)};
  if(value.isEmpty() || !isInt(value, false)){
    Net::sendHeaderAndBody(client, Net::HTTP_RES_BAD_REQ);
    return;
  }
  uint32_t newColor = toInt(value);
  lightStrip.show(newColor);
  Net::sendHeader(client, Net::HTTP_RES_OK, "text/plain");
  client.println(newColor);
}

void sendFavicon(WiFiClient &client, uint8_t method, const String &vars){
  if(favicon == nullptr || favicon[0] == '\0'){
    Net::sendHeaderAndBody(client, Net::HTTP_RES_NOT_FOUND);
    return;
  }
  Net::sendHeader(client, Net::HTTP_RES_OK, "image/svg+xml");
  client.print(favicon);
}

void handleRegular(){
  if(!tofSensor.initErr()){ //Init good
    //distance_t curRange = tofSensor.updateReadings();
    if(tofSensor.withinZone(triggerZone)){ //Object within the zone
      if(light.state() == Light::OFF && !waitForLeave){
        light.on();
        waitForLeave = true;
      }
    }
    else if(tofSensor.withinZone(leaveZone)){ //No object within zone
      waitForLeave = false;
    }
    print("mm: ");
    print(curDistance);
    print(" ft: ");
    println(Convert::mmToFt(curDistance));

    //Light on and has been on for the light on time
    if(light.state() == Light::ON && Time::timeSince(light.lastSetTime()) >= LIGHT_ON_TIME){
      light.off();
    }
  }
  else{ //Init error
    if(Time::timeSince(light.lastSetTime()) >= BLINK_DELAY){
      light.flip();
    }
  }
}

void handleSelect(){

}

void handleLight(){

}

void handleNet(){
  server.processReq();
  if(wifiPswdChangeTime != Time::NULL_TIME){
    if(Time::timeSince(wifiPswdChangeTime) > WIFI_PSWD_CHANGE_TIMEOUT){
      wifiPswdChangeTime = Time::NULL_TIME;
      wifiPswd = prefs.getString(wifiPswdKey);
    }
  }
}

void onWifiEvent(WiFiEvent_t event){
  if(event == ARDUINO_EVENT_WIFI_AP_STACONNECTED){
    if(wifiPswdChangeTime != Time::NULL_TIME){
      wifiPswdChangeTime = Time::NULL_TIME;
      prefs.putString(wifiPswdKey, wifiPswd);
    }
  }
}

void loadPreferences(){
  //Wifi password
  if(prefs.isKey(wifiPswdKey)){
    String pswd = prefs.getString(wifiPswdKey);
    if(Net::isValidPswd(pswd)){ wifiPswd = pswd; }
    else{ prefs.putString(wifiPswdKey, wifiPswd); }
  }
  else{
    prefs.putString(wifiPswdKey, wifiPswd);
  }
  //Threshold
  if(prefs.isKey(thresholdKey)){
    uint32_t thresh = prefs.getUShort(thresholdKey);
    if(!setThreshold(thresh)){ prefs.putUShort(thresholdKey, triggerZone.upper); }
  }
  else{
    prefs.putUShort(thresholdKey, triggerZone.upper);
  }
}

void setup() {
  //Serial
  #if ENABLE_SERIAL
  Serial.begin(9600);
  println("Init Serial");
  #endif
  //Preferences
  if(!prefs.begin("stoplight", false)){
    errors |= Error::PREFERENCES_ERR;
    println("Error initializing preferences");
  }
  else{ loadPreferences(); }
  //Light
  if(!light.init()){
    errors |= Error::LIGHT_INIT_ERR;
    println("Error initializing light");
  }
  //TOF
  if(!tofSensor.init()){
    errors |= Error::TOF_INIT_ERR;
    println("Error initializing sensor");
  }
  else{
    tofSensor.start(TIMING_BUDGET);
    tofSensor.fillReadings();
    if(tofSensor.withinZone(triggerZone)){
      waitForLeave = true;
    }
  }
  //Data Points
  //              Name              Variable            Type              Settable  Verify/Set Func
  dataPoints.add({"distance",       &curDistance,       DataPoint::UINT,  false                             });
  dataPoints.add({"uptime",         &curTime,           DataPoint::TIME,  false                             });
  dataPoints.add({"features",       &features,          DataPoint::UINT,  false                             });
  dataPoints.add({"errors",         &errors,            DataPoint::UINT,  false                             });
  dataPoints.add({"mode",           &curMode,           DataPoint::UINT8, true,     setModeCallback         });
  dataPoints.add({"threshold",      &triggerZone.upper, DataPoint::UINT,  true,     setThresholdCallback    });
  dataPoints.add({"color",          &lightStrip.color,  DataPoint::UINT,  true                              });
  dataPoints.add({"wifiPswd",       &wifiPswd,          DataPoint::STR,   true,     setWifiPswdCallback     });
  //Net
  if(features & Feature::WIFI){
    WiFi.begin();
    WiFi.onEvent(onWifiEvent);
    if(Net::createNetwork(wifiSSID, wifiPswd)){
      server.begin();
      //              Path            Callback            Allowed Methods
      server.addPath({"/",            mainPageCallback,   WebPath::GET});
      server.addPath({"/favicon.ico", sendFavicon,        WebPath::GET});
      server.addPath({"/get",         getPageCallback,    WebPath::GET});
      server.addPath({"/getall",      getAllPageCallback, WebPath::GET});
      server.addPath({"/set",         setPageCallback,    WebPath::POST});
      server.addPath({"/trycolor",    tryColorCallback,   WebPath::POST});
      if(!dnsServer.start()){
        errors |= Error::DNS_ERR;
        println("Error creating DNS server");
      }
    }
    else{
      println("Error creating network");
    }
  }
  //Let user know how init went
  light.alertInitState(tofSensor.initErr(), waitForLeave);
}

void loop() {
  Time::updateTime();
  curTime = Time::now(false);
  if(tofSensor.dataReady()){ curDistance = tofSensor.read(false); }
  handleNet();
  switch(curMode){
    case(Mode::REGULAR):
      handleRegular();
      break;
    case(Mode::SELECT):
      handleSelect();
      break;
    case(Mode::LIGHT):
      handleLight();
      break;
  }
  delay(LOOP_DELAY);
}