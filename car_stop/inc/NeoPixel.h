//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef NEOPIXEL_H
#define NEOPIXEL_H
#include <Adafruit_NeoPixel.h>
#include "Light.h"
#ifndef HAS_COLOR
#define HAS_COLOR false
#endif //HAS_COLOR

//Wrapper for Adafruit_NeoPixel
class NeoPixel : public Light{
  public:
    using color_t = uint32_t;
    color_t color;
    #if HAS_COLOR
    NeoPixel(int pin, int count, color_t color): 
      color(color), 
      _curColor(0),
      _light(count, pin, NEO_GRB | NEO_KHZ800)
    {}
    #else
    NeoPixel(int pin, int count, color_t color): 
      color(color), 
      _curColor(0)
    {}
    #endif

    static color_t constexpr Color(uint8_t r, uint8_t g, uint8_t b) {
      return Adafruit_NeoPixel::Color(r, g, b);
    }
    static const color_t RED    = 0xFF0000;
    static const color_t GREEN  = 0x00FF00;
    static const color_t BLUE   = 0x0000FF;

    bool init(){
      #if HAS_COLOR
      if(!_light.begin()){ return false; }
      _light.clear();
      _light.show();
      #endif
      return true;
    }

    void on(){
      #if HAS_COLOR
      if(_state == State::OFF || _curColor == 0 || _curColor != color){
        _curColor = color;
        _light.fill(color);
        _light.show();
      }
      Light::on();
      #endif
    }

    void show(color_t newColor){
      #if HAS_COLOR
      if(newColor != _curColor){
        _curColor = newColor;
        _light.fill(newColor);
        _light.show();
      }
      Light::on();
      #endif
    }

    void off(){
      #if HAS_COLOR
      if(_state == State::ON){
        _curColor = 0;
        _light.clear();
        _light.show();
      }
      Light::off();
      #endif
    }

    void alertInitState(bool initErr, bool waitForLeave){
      #if HAS_COLOR
      color_t colorTmp = color;
      if(initErr){ color = RED; }
      else if(waitForLeave){ color = BLUE; }
      else{ color = GREEN; }
      blink(500);
      color = colorTmp;
      #endif
    }
  private:
    color_t _curColor;
    #if HAS_COLOR
    Adafruit_NeoPixel _light;
    #endif
};
#endif //NEOPIXEL_H