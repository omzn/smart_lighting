#include "ledlight.h"

ledLight::ledLight() {
  _reset_flag = 1;
  _status[LIGHT0] = 0;
  _status[LIGHT1] = 0;
  _status[LIGHT2] = 0;
  _status[LIGHT3] = 0;
}

uint8_t ledLight::status(uint8_t v) { return _status[v]; }

int ledLight::enable() { return _enable_schedule; }

// スケジュールを有効(1)・無効(0)にする．
void ledLight::enable(int v) { _enable_schedule = (v == 0 ? 0 : 1); }

int ledLight::max_power() { return int(_max_pwm_value); }

void ledLight::max_power(int v) {
  _max_pwm_value = constrain(v, 0, MAX_PWM_VALUE);
}

int ledLight::brightness(uint8_t light) { 
  return int(_power[light] / _max_pwm_value * 100); 
}

void ledLight::brightness(uint8_t light,float v) { 
  power(light,map(v,0,100,0,_max_pwm_value));
}

int ledLight::power(uint8_t light) { return int(_power[light]); }

void ledLight::power(uint8_t light, float v) {
  _power[light] = constrain(v, 0, _max_pwm_value);

  if (_power[light] >= _max_pwm_value) {
    _power[light] = _max_pwm_value;
    _int_power[light] = _max_pwm_value;
    setpwm(light , _int_power[light]);
    _status[light] = LIGHT_ON;
  } else if (_power <= 0) {
    _power[light] = 0;
    _int_power[light] = 0;
    setpwm(light, _int_power[light]);
    _status[light] = LIGHT_OFF;
  } else {
    if (_int_power[light] != int(_power[light])) {
      _int_power[light] = int(_power[light]);
      setpwm(light,_int_power[light]);
      _status[light] = LIGHT_ON;
    }
  }
#ifdef DEBUG
  Serial.print("pwm: ");
  Serial.println(_power[light]);
  Serial.print("status: ");
  Serial.println(_status[light]);
#endif
}

void ledLight::powerAtTime(uint8_t light,uint8_t val, uint8_t h, uint8_t m) {
  if (m % 10 == 0) {
    _powerAtTime[light][h * 6 + m / 10] = val;
  }
}

uint8_t ledLight::powerAtTime(uint8_t light,uint8_t h, uint8_t m) {
  if (m % 10 == 0) {
    return _powerAtTime[light][h * 6 + m / 10];
  }
}

// ライトをvalの強さで光らせる．スケジュールが設定されているときは，-1を返し何もしない．
int ledLight::control(uint8_t light, uint16_t val) {
  if (!_enable_schedule) {
    brightness(light,val);
    return val;
  } else {
    return -1;
  }
}

// hh:mm時点で点灯すべきかどうかを判断する．1秒毎に呼び出される．
int ledLight::control(uint8_t light,int hh, int mm, int ss) {
  if (_enable_schedule) {
    uint8_t current_power, previous_power, next_power;
    float current_pwm, previous_pwm, next_pwm;
    int index = hh * 6 + mm / 10;

    previous_power = _powerAtTime[light][index];
    previous_pwm = map(previous_power,0,100,0,_max_pwm_value);
    next_power = _powerAtTime[light][(index + 1) % 144];
    next_pwm = map(next_power,0,100,0,_max_pwm_value);
    float diff = (next_pwm - previous_pwm) / 600.0;
    current_pwm = previous_pwm + diff * ((mm % 10)*60 + ss);

    if (int(current_pwm) != power(light)) {
      if (current_pwm > 0) {
        _status[light] = LIGHT_ON;
        power(light,current_pwm);
      } else {
        _status[light] = LIGHT_OFF;
        power(light,0);
      }
      Serial.println("pwm " + String(light) + ":" + String(current_pwm) );
    }
  }
  return _status[light];
}
