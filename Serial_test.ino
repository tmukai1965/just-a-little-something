// https://www.arduino.cc/reference/en/

void setup() {
  Serial.begin(115200);
  while(!Serial);
}

void loop() {
  for(;;) {
    while(Serial.available() == 0);
    int b = Serial.read();
    //if(b == 255) break;
    b++;
    size_t n = Serial.write(b);
    if(n != 1) err_halt();
  }
}

void err_halt() {
  pinMode(LED_BUILTIN, OUTPUT);
  for(;;) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(50);
  }
}
