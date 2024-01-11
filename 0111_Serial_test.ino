// https://www.arduino.cc/reference/en/

void setup() {
  Serial.begin(115200);
  while(!Serial);
}

void loop() {
  for(;;) {
    while(Serial.available() < 4);

    int dat[4];
    for(int i=0; i<4; i++) dat[i] = Serial.read();
    int adr = 0;
    for(int i=0; i<4; i++) { adr<<=8; adr|=dat[i]; }

    uint8_t x = *((uint8_t*)adr);
    if(Serial.write(x) != 1) err_halt();
  }
}

void err_halt() {
  pinMode(LED_BUILTIN, OUTPUT);
  for(;;) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(50);
  }
}
