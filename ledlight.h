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
    int control(int hh, int mm, int ss);
    int control(uint16_t v);
    uint16_t dim();
    void dim(int v);
    int max_power();
    void max_power(int v);
    int power();
    void power(float v);
    int brightness();
    void brightness(float v);
    uint8_t powerAtTime(uint8_t h, uint8_t m);
    void powerAtTime(uint8_t val, uint8_t h, uint8_t m);
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
    uint16_t _max_pwm_value = MAX_PWM_VALUE;
    uint8_t _enable_schedule;
    uint8_t _status; 
    uint8_t _forced_status;
    uint16_t _dim = 10;
    uint8_t _pin;
    float _power; 
    int _int_power;
    int _on_h;
    int _on_m;
    int _off_h;
    int _off_m;
    int _reset_flag;
    uint8_t _powerAtTime[144];
};

#endif
