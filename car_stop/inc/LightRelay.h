//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef LIGHT_RELAY_H
#define LIGHT_RELAY_H
//Relay controlling a light
class LightRelay : public Light{
  public:
  const bool ACTIVE_LOW;
  LightRelay(uint8_t p, bool activeLow = false) : _pin(p), ACTIVE_LOW(activeLow){ //Set to true so it is set to off when we call off()
    pinMode(p, OUTPUT);
    off();
  }
  void on() {
    digitalWrite(_pin, _getPinOutFromState(State::ON));
    Light::on();
  }
  void off() {
    digitalWrite(_pin, _getPinOutFromState(State::OFF));
    Light::off();
  }
  void alertInitState(bool initErr, bool waitForLeave){
    if(initErr){ blink(500, 3); }
    else if(waitForLeave){ blink(500, 2); }
    else{ blink(500); }
  }
  private:
    bool _getPinOutFromState(State state){
      if(ACTIVE_LOW){ return state == State::OFF ? true : false; }
      return state == State::OFF ? false : true;
    }
    uint8_t _pin;
};
#endif //LIGHT_RELAY_H