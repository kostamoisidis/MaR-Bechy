#include <LCD5110_Graph.h>
LCD5110 myGLCD(3,7,8,9,11);

extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];
void setup()
{
  myGLCD.InitLCD(70);
  myGLCD.setFont(MediumNumbers);
  randomSeed(analogRead(7));
}

char test1[] = "test";
char test2[] = "test";

void loop(){
  myGLCD.invert(0);
  myGLCD.clrScr();
  
  myGLCD.print(test1, CENTER, 1);
  myGLCD.update();
  delay(2000);
  
  myGLCD.print(test2, CENTER, 30);
  myGLCD.update();
  delay(2000);
  
  myGLCD.invert(1);
  myGLCD.update();
  delay(2000);
}
