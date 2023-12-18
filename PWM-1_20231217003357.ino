#include <pwm.h>
#include <AGTimerR4.h>

#define PWM_PIN 6
#define INTERVAL_SAMPLING 32  // micro sec

PwmOut pwm(PWM_PIN);

void timerCallback() {
  static int cnt = 0;
  static int dx = 1;
  cnt += dx;
  if     (cnt == 256) { cnt = 255; dx = -1; }
  else if(cnt ==  -1) { cnt = 0;   dx =  1; }
  analogWrite(PWM_PIN, cnt);
}

void setup() {
  analogWriteResolution(8);
  pwm.begin(256, 128, true, TIMER_SOURCE_DIV_1);  // Fpwm=93.75kHz; 50%
  AGTimer.init(INTERVAL_SAMPLING, timerCallback);
}

void loop() {}
