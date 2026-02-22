//Copyright 2025-2026 Treevar
//All rights reserved
#ifndef DATA_POINT_H
#define DATA_POINT_H
#include <Arduino.h>
#include <stdint.h>
#include "Time.h"
#include "Util.h"
//Data point used for viewing and modifying variabled
//Stores a void pointer to the data and its type
struct DataPoint{
  using status_t = uint8_t;
  enum Type{
    //Castable to ints
    UINT8 = 0,//Unsigned
    UINT,
    UINT64,
    TIME,
    BOOL,
    INT8 = 50,//Signed
    INT,
    VOID = 100,//Not castable to ints
    STR,
    IP
  };
  //Used for filtering data points based on their attributes
  struct Filter{
    enum class Type{
      NONE, //No filter
      TYPE, //Filter based on type
      SETTABLE //Filter based on whether it's settable
    } type;
    static const uint8_t TYPE_COUNT {2};

    uint8_t value; //Value to filter on (TYPE = type value, SETTABLE = whether settable)
    bool inverse; //Whether to invert the filter

    Filter(Type t, uint8_t val, bool invert = 0) : type(t), value(val), inverse(invert){}
    Filter() : type(Type::NONE), value(0), inverse(0){}
    Filter operator=(const Filter &rhs){
      type = rhs.type;
      value = rhs.value;
    }

    //Parses a string a returns the filter it represents
    //Filters are the following
    //  s[v] - Settable, v should be either 0 (no settable), or 1 (settable)
    //  t[v] - Type, v should be a valid type value, see DataPoint::Type enum above for valid values 
    //  filters can have a '!' before v to invert the filter
    //If the string is bad then the type will be set to NONE
    static Filter parseString(const String &str){
      uint8_t valStartIdx = 1; //Index of where the value for the filter starts
      Filter filter {};
      if(str[1] == '!'){ //Filter should be inverted
        ++valStartIdx; //Skip '!'
        filter.inverse = 1;
      }
      switch(str[0]){
        case 's': //Settable
          if(isDigit(str[valStartIdx])){
            filter.type = Type::SETTABLE;
            filter.value = str[valStartIdx] - '0'; //Convert the char to value it represents
          }
          break;
        case 't':{ //Type
          String val {str.substring(valStartIdx)};
          if(isInt(val, 0)){
            uint32_t v = toInt(val);
            switch(v){
              case DataPoint::Type::BOOL:
              case DataPoint::Type::INT8:
              case DataPoint::Type::INT:
              case DataPoint::Type::STR:
              case DataPoint::Type::TIME:
              case DataPoint::Type::UINT:
              case DataPoint::Type::UINT64:
              case DataPoint::Type::UINT8:
              case DataPoint::Type::VOID:
              case DataPoint::Type::IP: 
                //Valid type
                filter.type = Type::TYPE;
                filter.value = v;
            }
          }
          break;
        }
      };
      return filter;
    }

    //Parses a string with (possibly) multiple filters and populates the filterBuf with the filters found
    //Returns the number of filters found
    static uint8_t parseMultiString(const String &str, Filter *filterBuf, uint8_t bufSize){
      if(filterBuf == nullptr || bufSize == 0){
        return 0;
      }
      uint8_t filterCount = 0;
      int idx = 0, endIdx = 0;
      do{
        endIdx = str.indexOf(',', idx); //Get index of the next comma (will be -1 if there isnt one)
        String filter = str.substring(idx, endIdx); //Get the filter (sunstring uses uints so -1 will be interpretted as UINT_MAX)
        Filter newFilter = parseString(filter);
        if(newFilter.type == Filter::Type::NONE){ //Bad formatting
          return filterCount; //Stop parsing since we dont know where the next filter starts
        }
        filterBuf[filterCount++] = newFilter; //Add filter to buffer
        idx = endIdx + 1; //Get to start of next filter (if there is one. will be 0 if there isnt another filter)
      }while(idx > 0 && filterCount < bufSize); //Theres another filter and the buffer has room for it
      return filterCount;
    }
  };
  enum VerifyStatus : uint8_t{
    SET = 1,
    OK = 2,
    BAD = 4
  };
  using data_t = void *;
  //Used when no set function is set
  static status_t dummyFn(data_t, Type){
    return OK;
  }
  String name; //Name of the datapoint
  const Type type;
  data_t data;
  const bool settable; //Whether the data point is settable
  bool isInteger() const {
    return type < Type::VOID;
  }
  //Set function (not used directly)
  //Returns whether the var should be changed
  status_t (&set)(data_t, Type); 
  //Used to set the value
  //Returns whether the value was set
  status_t setVal(data_t d){ 
    if(!settable || d == nullptr || data == nullptr){
      return BAD;
    }
    status_t r = set(d, type);
    if(r == OK){
      switch(type){
        case BOOL:
        case UINT8:
          *static_cast<uint8_t*>(data) = *static_cast<uint8_t*>(d);
          break;
        case INT8:
          *static_cast<int8_t*>(data) = *static_cast<int8_t*>(d);
          break;
        case INT:
          *static_cast<int32_t*>(data) = *static_cast<int32_t*>(d);
          break;
        case UINT:
          *static_cast<uint32_t*>(data) = *static_cast<uint32_t*>(d);
        case UINT64:
          *static_cast<uint64_t*>(data) = *static_cast<uint64_t*>(d);
          break;
        case TIME:
          *static_cast<Time::Time_t*>(data) = *static_cast<Time::Time_t*>(d);
          break;
        case VOID:
          data = d;
          break;
        case STR:
          *static_cast<String*>(data) = *static_cast<String*>(d);
          break;
        case IP:
          *static_cast<IPAddress*>(data) = *static_cast<IPAddress*>(d);
          break;
      };
    }
    return r;
  }
  //Returns whether the datapoint is positive
  static bool isPositive(DataPoint::data_t d, DataPoint::Type t){
    if(t < DataPoint::Type::INT8){//Unsigned type
      return true;
    }
    else if(t < DataPoint::Type::VOID){//Signed type
      if(t == DataPoint::Type::INT8){
        return *static_cast<int8_t*>(d) >= 0;
      }
      else if(t == DataPoint::Type::INT){
        return *static_cast<int32_t*>(d) >= 0;
      }
    }
    else if(t == DataPoint::Type::IP){
      return true;
    }
    return false;
  }

  static bool isPosNonZero(DataPoint::data_t d, DataPoint::Type t){
    if(t < DataPoint::Type::VOID && t >= DataPoint::Type::INT8){ //Signed type
      if(t == DataPoint::Type::INT8){
        return *static_cast<int8_t*>(d) > 0;
      }
      else if(t == DataPoint::Type::INT){
        return *static_cast<int32_t*>(d) > 0;
      }
    }
    return isPositive(d, t);
  }

  //Returns whether the string is an IPv4 address
  static bool isIPAddress(const String &s){
    /*if(s.length() > 15){ //Max length of ipv4
      return 0;
    }
    int idx = 0, endIdx = 0;
    uint8_t octetCount = 0;
    do{
      if(octetCount > 4){
        return 0;
      }
      endIdx = s.indexOf('.', idx);
      String octet = s.substring(idx, endIdx);
      if(!isInt(octet, 0)){
        return 0;
      }
      int64_t val = toInt(octet);
      if(val > 255){
        return 0;
      }
      idx = endIdx + 1;
      ++octetCount;
    }while(idx > 0);
    if(octetCount != 4){
      return 0;
    }
    return 1;*/
    IPAddress ip {};
    if(!ip.fromString(s)){
      return false;
    }
    return ip.type() == IPType::IPv4;
  }

  //Returns whether the string contains valid data for the data point type
  static bool validateData(const String &val, Type type){
    switch(type){
      case Type::BOOL:
        if(val != "0" || val != "1"){
          return false;
        }
        return 1;
      case Type::UINT8:{
        if(!isInt(val, 0)){
          return false;
        }
        int64_t v = toInt(val);
        return v >= 0 && v <= UINT8_MAX;
      }
      case Type::INT8:{
        if(!isInt(val)){
          return false;
        }
        int64_t v = toInt(val);
        return v >= INT8_MIN && v <= INT8_MAX;
      }
      case Type::INT:{
        if(!isInt(val, 0)){
          return false;
        }
        int64_t v = toInt(val);
        return v >= INT32_MIN && v <= INT32_MAX;
      }
      case Type::TIME:
        if(isInt(val, 0)){
          return 1;
        }
        return Time::isTime(val);
      case Type::UINT:
      case Type::UINT64:
        return isInt(val, 0);
      case Type::IP:
        return isIPAddress(val);
      case Type::STR:
        return 1;
      case Type::VOID:
        return 0;
    };
  }

  //Sets the value from a string containg the value
  //Verifies data
  //Returns whether the value was set
  status_t setValueStr(const String &str){
    if(!validateData(str, type)){
      return BAD;
    }
    if(type == Type::TIME){
      Time t {Time::toTime(str)};
      return setVal(&t.raw);
    }
    else if(type < Type::VOID){ //Integer
      int64_t val = toInt(str);
      return setVal(&val);
    }
    else if(type == Type::STR){
      String val {str};
      replaceURIEncodedChars(val);
      return setVal(&val);
    }
    else if(type == Type::IP){
      IPAddress ip {};
      ip.fromString(str);
      return setVal(&ip);
    }
    return BAD;
  }

  //Converts data point value to a string
  String toString(bool formatTime = 0) const {
    if(data == nullptr){
      return "null";
    }
    else{
      String out {};
      switch(type){
        case BOOL:
        case UINT8://Without cast to int it would be treated as a char
          out = static_cast<int>(*static_cast<uint8_t*>(data));
          break;
        case INT8:
          out = static_cast<int>(*static_cast<int8_t*>(data));
          break;
        case INT:
          out = *static_cast<int32_t*>(data);
          break;
        case UINT:
          out = *static_cast<uint32_t*>(data);
          break;
        case UINT64:
          out = *static_cast<uint64_t*>(data);
          break;
        case TIME:
          if(formatTime){
            return Time::toString(*static_cast<Time::Time_t*>(data));
          }
          else{
            out = *static_cast<Time::Time_t*>(data);
          }
          break;
        case VOID:
          out = "null";
          break;
        case STR:
          return *static_cast<String*>(data);
        case IP:
          return static_cast<IPAddress*>(data)->toString();
      };
      return out;
    }
  }

  /*Converts to a JSON object
    If formatTime is true then times will be formatted for human radability (ie 5:00 instead of 300)
    Has the keys
      - dp: data point name
      - val: value of data point
      - m: whether the data point is modifiable
      - t: type of data point (represented as an int)
  */
  String toJSON(bool formatTime = false) const {
    String out {};
    out.reserve(50);
    out += '\"';
    out += name;
    out += "\":{\n";
    out += "\"val\":";
    out += '"'; 
    out += toString(formatTime);
    out += '"'; 
    out += ",\n";
    out += "\"m\":";
    out += settable;
    out += ",\n";
    out += "\"t\":";
    out += type;
    out += "\n}";
    return out;
  }

  //Returns whether this data point matches the filter
  bool matchesFilter(const Filter &filter) const {
    switch(filter.type){
      case Filter::Type::TYPE:
        if(filter.inverse){
          return filter.value != type;
        }
        return filter.value == type;
      case Filter::Type::SETTABLE:
        if(filter.inverse){
          return static_cast<bool>(data) != settable;
        }
        return static_cast<bool>(data) == settable;
      case Filter::Type::NONE:
        return 0;
    };
  }

  bool operator==(const DataPoint &rhs) const {
    return name == rhs.name;
  }

  DataPoint(const char *n, data_t d, Type t, bool settable = false, status_t (&s)(data_t, Type) = dummyFn) : name(n), data(d), type(t), settable(settable), set(s){}
  //Const constructor (will be unmodifiable no matter what)
  DataPoint(const char *n, const void *d, Type t) : name(n), data(const_cast<void*>(d)), type(t), settable(0), set(dummyFn){}
  static const DataPoint NULL_DATAPOINT;
};
const DataPoint DataPoint::NULL_DATAPOINT {"NULL", nullptr, DataPoint::Type::VOID, false};
#endif //DATA_POINT_H