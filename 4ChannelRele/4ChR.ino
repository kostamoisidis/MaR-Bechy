#define RELE1_PIN 1
#define RELE2_PIN 0 
void setup() {
 pinMode(RELE1_PIN, OUTPUT);
 pinMode(RELE2_PIN, OUTPUT);
}
void loop() {
 digitalWrite(RELE1_PIN, HIGH);   //relé1 rozepnuto (záleží také na typu)
 delay(1000);                     //1 s čekání
 digitalWrite(RELE2_PIN, HIGH);   //relé2 rozepnuto (záleží také na typu)
 delay(1000);                     //1 s čekání
 digitalWrite(RELE1_PIN, LOW);    //relé1 sepnuto (záleží také na typu relé)
 delay(1000);                     //1 s čekání
 digitalWrite(RELE2_PIN, LOW);    //relé2 sepnuto (záleží také na typu relé)
 delay(1000);                     //1 s čekání
} 