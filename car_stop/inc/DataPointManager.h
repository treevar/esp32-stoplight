//Copyright 2026 Treevar
//All rights reserved
#ifndef DATAPOINT_MANAGER_H
#define DATAPOINT_MANAGER_H
#include "DataPoint.h"
class DataPointManager{
  public:
    static const int MAX = 16;
    DataPointManager():
      _count(0)
    {
      for(int i = 0; i < MAX; ++i){
        _dataPoints[i] = nullptr;
      }
    }
    ~DataPointManager(){
      for(int i = 0; i < _count; ++i){
        delete _dataPoints[i];
      }
    }
    bool exists(const String &name) const {
      return _getIdx(name) < 0 ? false : true;
    }
    bool add(const DataPoint &dataPoint){
      if(_count >= MAX || exists(dataPoint.name) || dataPoint.name == DataPoint::NULL_DATAPOINT.name){ return false; }
      _dataPoints[_count++] = new DataPoint(dataPoint);
      return true;
    }
    DataPoint::status_t set(const String &name, const String &val){
      int idx = _getIdx(name);
      if(idx < 0){ return DataPoint::BAD; }
      return _dataPoints[idx]->setValueStr(val);
    }
    DataPoint::status_t set(const String &name, DataPoint::data_t val){
      int idx = _getIdx(name);
      if(idx < 0){ return DataPoint::BAD; }
      return _dataPoints[idx]->setVal(val);
    }
    const DataPoint& get(const String &name){
      int idx = _getIdx(name);
      if(idx < 0 || _dataPoints[idx] == nullptr){ return DataPoint::NULL_DATAPOINT; }
      return *_dataPoints[idx];
    }
    const DataPoint& get(uint8_t idx){
      if(idx < 0 || idx >= _count){ return DataPoint::NULL_DATAPOINT; }
      return *_dataPoints[idx];
    }
    uint8_t count() const { return _count; }
  private:
    DataPoint* _dataPoints[MAX];
    uint8_t _count;
    int _getIdx(const String &name) const {
      for(int i = 0; i < _count; ++i){
        if(_dataPoints[i]->name == name){ return i; }
      }
      return -1;
    }
};
#endif //DATAPOINT_MANAGER_H