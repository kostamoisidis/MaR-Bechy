// 4 channel rele
#define RELE1_PIN 44
#define RELE2_PIN 46
#define RELE_VCC  42

void setup() {
  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  pinMode(RELE_VCC, OUTPUT);
  digitalWrite(RELE1_PIN, HIGH);
  digitalWrite(RELE2_PIN, HIGH);
  digitalWrite(RELE_VCC, HIGH);
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