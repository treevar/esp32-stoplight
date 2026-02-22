//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef TOF_SENSOR_H
#define TOF_SENSOR_H
#include <VL53L1X.h>
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
    TOFSensor(DistanceMode distMode, uint8_t readingCount, uint32_t timingBudget = 0, Coord roiSize = Coord(16, 16), Coord roiCenter = Coord(8, 8)): 
      _sensor(), 
      _distMode(distMode),
      _timingBudget(timingBudget * 1000), //Needs to be in us
      _roiSize(roiSize),
      _roiCenter(roiCenter),
      _readingCount(readingCount),
      _readingIdx(0),
      _initErr(true)
    {
      _readings = new distance_t[_readingCount];
      memset(_readings, 0, sizeof(_readings) / sizeof(_readings[0]));
      _readingIdx = 0;
    }
    
    ~TOFSensor(){
      _sensor.stopContinuous();
      delete[] _readings;
    }

    struct Zone{
      distance_t lower, upper;
      Zone(distance_t l, distance_t u) : lower(l), upper(u){}
    };

    bool init(){
      _initErr = true;
      if(!Wire.begin()){ return false; }
      if(!_sensor.init(true)){ return false; } //Start in 2.8v mode
      if(!_sensor.setDistanceMode(_distMode)){ return false; };
      if(_timingBudget > 0){ 
        if(!_sensor.setMeasurementTimingBudget(_timingBudget)){ return false; }
      }
      uint8_t spadNum = _roiCenter.y * 16 + _roiCenter.x;
      _sensor.setROICenter(spadNum); 
      _sensor.setROISize(_roiSize.x, _roiSize.y);
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
      _readings[_readingIdx] = reading;
      _readingIdx = (_readingIdx + 1) % _readingCount;
      return reading;
    }

    bool dataReady() { return _sensor.dataReady(); }

    bool initErr() const { return _initErr; }

    //Returns whether the sensor is measuring an object within the zone
    bool withinZone(const Zone &zone){
      for(int i = 0; i < _readingCount; ++i){
        if(_readings[i] < zone.lower || _readings[i] > zone.upper){
          return false;
        }
      }
      return true;
    }

    //Returns whether all readings are not within the zone
    bool invertedWithinZone(const Zone &zone, bool useStoredReading = false){
      for(int i = 0; i < _readingCount; ++i){
        if(_readings[i] >= zone.lower && _readings[i] <= zone.upper){
          return false;
        }
      }
      return true;
    }
    void fillReadings(){
      for(int i = 0; i < _readingCount; ++i){
        read();
      }
    }
  private:
    VL53L1X _sensor;
    DistanceMode _distMode;
    uint32_t _timingBudget;
    Coord _roiSize, _roiCenter;
    bool _initErr;
    distance_t* _readings;
    uint8_t _readingIdx;
    const uint8_t _readingCount;
};
#endif //TOF_SENSOR_H