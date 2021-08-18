#define OUT 53
#define VCC 51
void setup() {
  Serial.begin(9600);
    digitalWrite(VCC, HIGH);
}
void loop() {
    Serial.println( digitalRead(OUT) );
    delay(200);
}