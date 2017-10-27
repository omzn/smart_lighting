#ifndef LEDLIGHT_H
#define LEDLIGHT_H

#include "Arduino.h"

#define MAX_PWM_VALUE 1023

enum {LIGHT_OFF=0,LIGHT_ON,LIGHT_TURNING_OFF,LIGHT_TURNING_ON};

class ledLight {
  public:
    ledLight(uint8_t pin);

    int  enable();
    void enable(int v);
    void schedule(int on_h, int on_m, int off_h, int off_m);
    int control(int hh, int mm);
    int control(uint16_t v);
    uint16_t dim();
    void dim(int v);
    int power();
    void power(float v);
    int on_h();
    int on_m();
    int off_h();
    int off_m();
    void on_h(int v);
    void on_m(int v);
    void off_h(int v);
    void off_m(int v);
    uint8_t status();
  protected:
    uint8_t _enable_schedule;
    uint8_t _status; 
    uint16_t _dim = 10;
    uint8_t _pin;
    float _power; 
    int _int_power;
    int _on_h;
    int _on_m;
    int _off_h;
    int _off_m;
    int _reset_flag;
};

#endif
