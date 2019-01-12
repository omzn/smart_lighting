#include "ledlight.h"

ledLight::ledLight(uint8_t pin) {
  _dim = 0;
  _pin = pin;
  _reset_flag = 1;
  _status = 0;
  _forced_status = 0;
  pinMode(_pin, OUTPUT);
}

uint8_t ledLight::status() { return _status; }

int ledLight::enable() { return _enable_schedule; }

// スケジュールを有効(1)・無効(0)にする．
void ledLight::enable(int v) { _enable_schedule = (v == 0 ? 0 : 1); }

// スケジュールの点灯・消灯時刻を設定する．
void ledLight::schedule(int on_h, int on_m, int off_h, int off_m) {
  if (on_h >= 0 && on_h <= 23) {
    _on_h = on_h;
  } else {
    _on_h = 0;
  }
  if (on_m >= 0 && on_m <= 59) {
    _on_m = on_m;
  } else {
    _on_m = 0;
  }
  if (off_h >= 0 && off_h <= 23) {
    _off_h = off_h;
  } else {
    _off_h = 0;
  }
  if (off_m >= 0 && off_m <= 59) {
    _off_m = off_m;
  } else {
    _off_m = 0;
  }
}

int ledLight::max_power() { return int(_max_pwm_value); }

void ledLight::max_power(int v) {
  _max_pwm_value = constrain(v, 0, MAX_PWM_VALUE);
}

int ledLight::brightness() { 
  return int(_power / _max_pwm_value * 100); 
}

void ledLight::brightness(float v) { 
  power(map(v,0,100,0,_max_pwm_value));
}

int ledLight::power() { return int(_power); }

void ledLight::power(float v) {
  _power = constrain(v, 0, _max_pwm_value);

  if (_power >= _max_pwm_value) {
    _power = _max_pwm_value;
    _int_power = _max_pwm_value;
    analogWrite(_pin, _max_pwm_value); // here to be modified
    _status = LIGHT_ON;
  } else if (_power <= 0) {
    _power = 0;
    _int_power = 0;
    analogWrite(_pin, 0); // here to be modified
    _status = LIGHT_OFF;
  } else {
    if (_int_power != int(_power)) {
      _int_power = int(_power);
      analogWrite(_pin, _int_power); // here to be modified
    }
  }
#ifdef DEBUG
  Serial.print("pwm: ");
  Serial.println(_power);
  Serial.print("status: ");
  Serial.println(_status);
#endif
}

void ledLight::on_h(int v) { _on_h = v; }
void ledLight::on_m(int v) { _on_m = v; }
void ledLight::off_h(int v) { _off_h = v; }
void ledLight::off_m(int v) { _off_m = v; }
int ledLight::on_h() { return _on_h; }
int ledLight::on_m() { return _on_m; }
int ledLight::off_h() { return _off_h; }
int ledLight::off_m() { return _off_m; }

uint16_t ledLight::dim() { return _dim; }

// dimの間隔(秒)を指定する．dimが0の時は，照明は瞬時に点消灯する．
void ledLight::dim(int v) { _dim = v; }

void ledLight::powerAtTime(uint8_t val, uint8_t h, uint8_t m) {
  if (m % 10 == 0) {
    _powerAtTime[h * 6 + m / 10] = val;
  }
}

uint8_t ledLight::powerAtTime(uint8_t h, uint8_t m) {
  if (m % 10 == 0) {
    return _powerAtTime[h * 6 + m / 10];
  }
}

// ライトをvalの強さで光らせる．スケジュールが設定されているときは，-1を返し何もしない．
int ledLight::control(uint16_t val) {
  if (!_enable_schedule) {
    brightness(val);
    return val;
  } else {
    _forced_status = val > 0 ? LIGHT_ON : LIGHT_OFF;
    return -1;
  }
}

// hh:mm時点で点灯すべきかどうかを判断する．1秒毎に呼び出される．
int ledLight::control(int hh, int mm, int ss) {
  if (_enable_schedule) {
    uint8_t current_power, previous_power, next_power;
    float current_pwm, previous_pwm, next_pwm;
    int index = hh * 6 + mm / 10;

    previous_power = _powerAtTime[index];
    previous_pwm = map(previous_power,0,100,0,_max_pwm_value);
    next_power = _powerAtTime[(index + 1) % 144];
    next_pwm = map(next_power,0,100,0,_max_pwm_value);
    float diff = (next_pwm - previous_pwm) / 600.0;
    current_pwm = previous_pwm + diff * ((mm % 10)*60 + ss);

    if (int(current_pwm) != power()) {
      if (current_pwm > 0) {
        _status = LIGHT_ON;
        power(current_pwm);
      } else {
        _status = LIGHT_OFF;
        power(0);
      }
      Serial.println("pwm:" + String(current_pwm) );
    }
  }
  return _status;
}
