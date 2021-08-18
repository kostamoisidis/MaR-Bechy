const int pinNum = 3;
const int pins[pinNum] = { 5, 4, 3 };
bool state[pinNum];

void setup() {
  Serial.begin(9600);
  
  for ( int i = 0 ; i < pinNum ; i++ )
    pinMode( pins[i] , INPUT_PULLUP );
    
}

void loop() {
  for (int i = 0; i < pinNum; i++) {
    state[i] = digitalRead(pins[i]);
    if (state[i] == 0) {
      Serial.print("pin ");
      Serial.print(i);
      Serial.println(" pressed.");
    }
  }
  delay(1000);
}