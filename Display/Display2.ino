#include <LCD5110_Graph.h>
LCD5110 myGLCD(6,7,8,9,10);

extern uint8_t SmallFont[];


void setup()
{
  myGLCD.InitLCD(50);
  myGLCD.invert(true);
  myGLCD.setFont(SmallFont);
  myGLCD.invertText(false);
  randomSeed(analogRead(7));
}

void loop(){  
  
  myGLCD.print("2", CENTER, 5);
  
  myGLCD.update();
  delay(1000);
}