//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef TOF_SENSOR_H
#define TOF_SENSOR_H
#include <VL53L1X.h>
#include <SPIMemory.h>
//Wrapper for VL53L1X
class TOFSensor{
  public:
    using DistanceMode = VL53L1X::DistanceMode;
    using distance_t = uint16_t;
    //0, 0 is top left
    struct Coord{
      uint8_t x, y;
      Coord(uint8_t x, uint8_t y) : x(x), y(y){}
    };
    struct Config{
      DistanceMode distMode;
      uint32_t timingBudget;
      Coord roiSize, roiCenter;
      Config(DistanceMode distMode, uint32_t timingBudget, Coord roiSize, Coord roiCenter):
        distMode(distMode),
        timingBudget(timingBudget),
        roiSize(roiSize),
        roiCenter(roiCenter)
      {}
      void loadFromFlash(SPIFlash &flash, uint32_t addr){
        flash.readAnything(addr, *this);
      }
      void saveToFlash(SPIFlash &flash, uint32_t addr){
        flash.eraseSector(addr);
        flash.writeAnything(addr, *this);
      }
    };
    TOFSensor(DistanceMode distMode, uint32_t timingBudget = 0, Coord roiSize = Coord(16, 16), Coord roiCenter = Coord(8, 8)): 
      _sensor(), 
      _config(distMode, timingBudget, roiSize, roiCenter),
      _reading(0),
      _initErr(true)
    {}
    
    ~TOFSensor(){
      _sensor.stopContinuous();
    }

    struct Zone{
      distance_t lower, upper;
      Zone(distance_t l, distance_t u) : lower(l), upper(u){}
      bool within(distance_t distance) const {
        if(distance < lower || distance > upper){
          return false;
        }
        return true;
      }
    };

    bool init(){
      _initErr = true;
      if(!Wire.begin()){ return false; }
      if(!_sensor.init(true)){ return false; } //Start in 2.8v mode
      if(!_sensor.setDistanceMode(_config.distMode)){ return false; };
      if(_config.timingBudget > 0){ 
        if(!_sensor.setMeasurementTimingBudget(_config.timingBudget)){ return false; }
      }
      uint8_t spadNum = _config.roiCenter.y * 16 + _config.roiCenter.x;
      _sensor.setROICenter(spadNum); 
      _sensor.setROISize(_config.roiSize.x, _config.roiSize.y);
      _initErr = false;
      return true;
    }

    //Tell sensor to start reading
    void start(uint32_t period){
      _sensor.startContinuous(period);
    }
    //Tell sensor to stop reading
    void stop(){
      _sensor.stopContinuous();
    }

    //Take a reading from the sensor
    distance_t read(bool blocking = true){
      distance_t reading = _sensor.read(blocking);
      _reading = reading;
      return reading;
    }

    bool dataReady() { return _sensor.dataReady(); }

    distance_t reading() const { return _reading; }
    bool initErr() const { return _initErr; }

    //Returns whether the sensor is measuring an object within the zone
    bool withinZone(const Zone &zone, bool useStoredReading = false){
      if(!useStoredReading){ read(); }
      if(_reading < zone.lower || _reading > zone.upper){
        return false;
      }
      return true;
    }

    //Returns whether all readings are not within the zone
    bool invertedWithinZone(const Zone &zone, bool useStoredReading = false){
      if(!useStoredReading){ read(); }
      if(_reading >= zone.lower && _reading <= zone.upper){
        return false;
      }
      return true;
    }
  private:
    VL53L1X _sensor;
    Config _config;
    distance_t _reading;
    bool _initErr;
};
#endif //TOF_SENSOR_H