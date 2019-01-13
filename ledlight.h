#ifndef LEDLIGHT_H
#define LEDLIGHT_H

#include "Arduino.h"
#include "pca9633.h"

#define MAX_PWM_VALUE 255
#define MAX_LED_NUM (4)

#define PCA9633_ADDRESS_1            (0x60)
#define PCA9633_ADDRESS_2            (0x61)

enum {LIGHT_OFF=0,LIGHT_ON,LIGHT_TURNING_OFF,LIGHT_TURNING_ON};
enum {LIGHT0=0,LIGHT1,LIGHT2,LIGHT3,LIGHT4,LIGHT5,LIGHT6,LIGHT7};

class ledLight {
  public:
    ledLight();
    void begin();
    int  enable();
    void enable(int v);
    int control(uint8_t light, int hh, int mm, int ss);
    int control(uint8_t light, uint16_t v);
    int max_power();
    void max_power(int v);
    int power(uint8_t light);
    void power(float v);
    void power(uint8_t light,float v);
    int brightness(uint8_t light);
    void brightness(uint8_t light,float v);
    uint8_t powerAtTime(uint8_t light,uint8_t h, uint8_t m);
    void powerAtTime(uint8_t light,uint8_t val, uint8_t h, uint8_t m);
    uint8_t status(uint8_t light);
  protected:
    PCA9633 driver[2];
    uint16_t _max_pwm_value = MAX_PWM_VALUE;
    uint8_t _enable_schedule;
    uint8_t _status[MAX_LED_NUM]; 
    float _power[MAX_LED_NUM]; 
    int _int_power[MAX_LED_NUM];
    int _reset_flag;
    uint8_t _powerAtTime[MAX_LED_NUM][144];
};

#endif
