//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef UTIL_H
#define UTIL_H

#ifndef ENABLE_SERIAL
#define ENABLE_SERIAL true
#endif //ENABLE_SERIAL

//Conversions
namespace Convert{
  inline float mmToFt(uint16_t mm){
    return mm * 0.00328084;
  }

  inline uint32_t mmtoIn(uint16_t mm){
    return mm * 0.03937008;
  }

  inline uint16_t ftToMm(float feet){
    return static_cast<uint16_t>(feet * 304.8);
  }
};

template<typename T>
inline void print(const T &t){
  #if ENABLE_SERIAL
  Serial.print(t);
  #endif
}

template<typename T>
inline void println(const T &t){
  #if ENABLE_SERIAL
  Serial.println(t);
  #endif
}

inline void println(){
  #if ENABLE_SERIAL
  Serial.println();
  #endif
}
/*
inline void print(const char *msg){
  #if ENABLE_SERIAL
  Serial.print(msg);
  #endif
}

inline void print(const String &msg){
  #if ENABLE_SERIAL
  Serial.print(msg);
  #endif
}

inline void print(int m){
  #if ENABLE_SERIAL
  Serial.print(m);
  #endif
}

inline void print(float m){
  #if ENABLE_SERIAL
  Serial.print(m);
  #endif
}*/

//Returns whether the string is an integer
//Can handle arbitray length integers
//If allowNeg = 0 then the number has to be positive
bool isInt(const String &s, bool allowNeg = true){
  if(s.length() == 0){ return false; }
  //First digit isnt - and not a digit or it is - and there are no other chars or neg isnt allowed
  if((s[0] != '-' && !isDigit(s[0])) || (s[0] == '-' && (s.length() < 2 || !allowNeg))){
    return false;
  }
  for(size_t i = 1; i < s.length(); ++i){
    if(!isDigit(s[i])){
      return false;
    }
  }
  return true;
}
int64_t toInt(const String &s){
  /*int64_t val = 0;
  int64_t place = 1;
  uint8_t endIdx = 0;
  if(s[0] == '-'){
    place = -1;
    ++endIdx;
  }
  for(int i = s.length()-1; endIdx <= i; --i){
    val += place * (s[i]-'0');
    place *= 10;
  }*/
  try{
    return strtoll(s.c_str(), nullptr, 10);
  }
  catch(...){
    return INT64_MAX;
  }
}

//Represents a URI encoded character
struct EncodedChar{
  const char *code;
  char c;
  EncodedChar(const char *code, char c) : code(code), c(c){}
};
const EncodedChar ENCODED_CHARS[] = {{"%20", ' '}, {"%21", '!'}, {"%22", '"'}, {"%23", '#'}, {"%24", '$'}, {"%25", '%'}, {"%26", '&'}, {"%27", '\''}, {"%28", '('}, {"%29", ')'}, {"%2A", '*'}, {"%2B", '+'}, {"%2C", ','}, {"%2D", '-'}, {"%2E", '.'}, {"%2F", '/'}, {"%3A", ':'}, {"%3B", ';'}, {"%3C", '<'}, {"%3D", '='}, {"%3E", '>'}, {"%3F", '?'}, {"%40", '@'}, {"%5B", '['}, {"%5C", '\\'}, {"%5D", ']'}, {"%5E", '^'}, {"%5F", '_'}, {"%60", '`'}, {"%7B", '{'}, {"%7C", '|'}, {"%7D", '}'}, {"%7E", '~'}};
const uint8_t ENCODED_CHAR_COUNT = sizeof(ENCODED_CHARS) / sizeof(EncodedChar);

//Replaces URI encoded chars in the string with their actual char representation
void replaceURIEncodedChars(String &str){
  for(uint8_t i = 0; i < ENCODED_CHAR_COUNT; ++i){
    str.replace(ENCODED_CHARS[i].code, String(ENCODED_CHARS[i].c));
  }
}

//Essentially exit(), but it doesn't reboot
//Enters an infinite loop
void stopExec(){
  while(1){}
}

#endif //UTIL_H