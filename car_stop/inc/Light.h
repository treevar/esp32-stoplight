//Copyright 2025-2026 Treevar
//All Rights Reserved
#ifndef LIGHT_H
#define LIGHT_H
//Base class for indicator light
class Light{
  public:
    enum State : bool{
      OFF = 0,
      ON = 1
    };
    virtual bool init(){ return true; }
    virtual void on(){
      _state = State::ON;
      _updateTime();
    }
    virtual void off(){
      _state = State::OFF;
      _updateTime();
    }
    virtual void alertInitState(bool initErr, bool waitForLeave) = 0;
    void flip(){
      if(_state == State::OFF){ on(); }
      else{ off(); }
    }
    void blink(Time sleep, uint8_t count = 1){
      if(count == 0){ return; }
      --count; //Remove one for the blink at the end without a delay after it
      count *= 2; //Need to double becasue each blink takes 2 flips
      while(count-- > 0){
        flip();
        delay(sleep);
      }
      flip();
      delay(sleep);
      flip();
    }
    State state() const { return _state; }
    Time lastSetTime() const { return _lastSetTime; }
  protected:
    void _updateTime(){ _lastSetTime = Time::now(true); }
    State _state;
    Time _lastSetTime;
};
#endif //LIGHT_H
