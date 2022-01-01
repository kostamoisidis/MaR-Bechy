#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <LCD5110_Graph.h>

#define bauds 9600
#define serialBuffMaxSize 200
#define cmdSize 5               // it is 3  but let there be some space
#define acctualCmdSize 3 
#define phoneNumLen 20           
#define acctualNumLen 12
#define headerSize 5            // it is 4 but let there be some space
#define numberOffset 10
#define commandOffset 51

#define debug
#define hardDebug
//#define greetSMS


/*
 * ZTV - zapni topeni vodu
 * VTV - vypni topeni vodu
 * ZV - zapni vodu
 * VV - vypni vodu
 * HLP - help
 */


/*
 * Phone numbers whitelist
 * example num:
 * 420123123123
 */
const int personCount = 3;
char whiteList[personCount][phoneNumLen] = {
  "some","numbers","here"
};


/**
 * Golbal Definitions
 */
// Keyboard
const int pinCount = 3;
const int pins[pinCount] = { 53, 51, 49 };
int cyclicBuffer = 0;

// Display
extern uint8_t SmallFont[];

// Runtime
bool firstOptMsg = true;
bool optoHasChanged = false;
bool firstRun = true;
const char * smsHeader = "+CMT";
const char * helpMSG = "Arduino:\n"
                       "ZTV - Zapni topeni a vodu\n"
                       "VTV - Vypni topeni a vodu\n"
                       "ZV - Zapni vodu\n"
                       "VV - Vypni vodu\n"
                       "ZPT - Zapni pouze topeni\n"
                       "VPT - Vypni pouze topeni\n"
                       "HLP - Help\n"
                       "STS - Status";

// 4 channel rele
#define RELE1_PIN 44
#define RELE2_PIN 46
#define RELE_VCC  42

// opto
#define OUT 52 //fialova
#define VCC 50 //bila

typedef struct TStatus
{
  bool heating;
  bool water;
  bool hasChanged;
} STS;

typedef struct TWhiteList
{
  char phone[13];
  char name;
} WHTLST;

typedef struct TBuffer
{
  char text[serialBuffMaxSize];
  int len;
} SBUF;

/**
 * Zapojeni Membr. klaves
 * 
 * 
 *             ┌──────────────────────────────┐
 * pin 49  ────┤                              │
 *             │                              │
 * pin 51  ────┤        x     x     x         │
 *             │       x x   x x   x x        │
 * pin 53  ────┤        x     x     x         │
 *             │                              │
 *  GND    ────┤                              │
 *             └──────────────────────────────┘
 * 
 */

/**
 * Zapojeni GSM modulu
 * 
 * VCC - 4.1 v
 * GND - ground
 * RXD - pin 11
 * TXD - pin 12
 */
SoftwareSerial SIM(12, 11);

/**
 * Zapojeni LCD
 * 
 * RST   - pin 9
 * CE    - pin 10
 * DC    - pin 8
 * DIN   - pin 7
 * CLK   - pin 6
 * 
 * VCC   - 3.3V
 * LIGHT - ground
 * GND   - ground
 */
LCD5110 myGLCD(6,7,8,9,10);
STS status;

// *****************************************

void setup()
{
  /**
   * 
   * Keyboard init
   * must be first otherways pins will fire a few times before the setup reaches this init
   * 
   */
  for ( int i = 0 ; i < pinCount ; i++ )
    pinMode( pins[i] , INPUT_PULLUP );

  /**
   * 
   * OPTO
   * 
   */
  pinMode(VCC, OUTPUT);
  digitalWrite(VCC, HIGH);

  /**
   * 
   * RELE
   * 
   */
  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  pinMode(RELE_VCC, OUTPUT);
  digitalWrite(RELE1_PIN, HIGH);
  digitalWrite(RELE2_PIN, HIGH);
  digitalWrite(RELE_VCC, HIGH);

  /**
  * 
  * Serial monitor init
  * 
  */
  Serial.begin(bauds);
  
  /**
  * 
  * GSM init
  * 
  */
  #ifdef debug
    Serial.println("Initializing sim con...");
    Serial.println("60 sec waiting for GSM");
  #endif
  delay(60000); // wait for gsmModule to connect to celular properly
  SIM.begin(bauds);
  SIM.println("AT");
  updateSerial();
  SIM.println("AT+CMGF=1");
  updateSerial();
  SIM.println("AT+CNMI=1,2,0,0,0");
  /*
  https://stackoverflow.com/questions/58908575/cnmi-command-how-to-receive-notification-and-save-to-sim-card-incoming-sms
  */
  updateSerial();

  /**
   * 
   * LCD init
   * 
   */
  myGLCD.InitLCD(50);
  myGLCD.setFont(SmallFont);
  myGLCD.disableSleep();
  randomSeed(analogRead(7));
}

// *****************************************

void loop()
{
  updateSerial();
}

// *****************************************

// *****************************************

bool checkNumWhitelist(char *number)
{
  bool isValid = false;
  
  for (int i = 0; i < personCount; i++)
  {
    #ifdef debug
      Serial.write("comparing: ");
      Serial.write("\n");
      for (int j = 0; j < acctualNumLen; j++)
        Serial.write(number[j]);
      Serial.write("\n");
      for (int j = 0; j < acctualNumLen; j++)
        Serial.write(whiteList[i][j]);
      Serial.write("\n");
    #endif

    if (!strncmp(number, whiteList[i], acctualNumLen))
    {
      #ifdef debug
        Serial.write("Number recognized.\n");
      #endif
      isValid = true;
      break;
    }
  }

  return isValid;
}

// *****************************************

void response(char * number, char *responseText)
{
  /**
   * string for sms example: "AT+CMGS=\"+*********\"\r"
   */

  char atNumMsg[50] = "AT+CMGS=\"+";
  strncat(atNumMsg, number, 12);
  strncat(atNumMsg, "\"\r", strlen("\"\r"));

  #ifdef hardDebug
    Serial.println("Concatenated AT num string:");

    for (int i = 0; i < strlen(atNumMsg); i++)
      Serial.println(atNumMsg[i]);

    Serial.println("\n");
  #endif

  #ifdef debug
    Serial.println("Sending SMS...");
  #endif

  SIM.println("AT");
  delay(500);
  SIM.println("AT+CMGF=1");
  delay(500);
  SIM.println(atNumMsg);
  delay(500);

  if (strncmp("HELP", responseText, 4) == 0)
    responseText = (char*)helpMSG;
  
  SIM.print(responseText);
  delay(500);
  SIM.write(26);
  #ifdef debug
    Serial.println("Text Sent.");
  #endif
  delay(500);
}

// *****************************************

void fresponse(char *cislo, char *responseText)
{
  #ifdef debug
    Serial.write("Fake message sent.\n");
  #endif
}

// *****************************************

char * statusGet(void)
{
  if ( status.heating && status.water )
    return "Arduino Status:\nTopeni ZAPNUTO\nvoda ZAPNUTA.\0";
  if ( status.heating && ! status.water )
    return "Arduino Status:\nTopeni ZAPNUTO\nvoda VYPNUTA.\0";
  if ( ! status.heating && status.water )
    return "Arduino Status:\nTopeni VYPNUTO\nvoda ZAPNUTA.\0";
  if ( ! status.heating && ! status.water )
    return "Arduino Status:\nTopeni VYPNUTO\nvoda VYPNUTA.\0";   
  return "Error\0";
}

// ***********************************************************

void optoErrorMsg ( char * error )
{
  myGLCD.clrScr();
  myGLCD.update();
  myGLCD.print( error, CENTER, 5);
  myGLCD.update();
  delay(1000);
  myGLCD.clrScr();
  myGLCD.update();
}

void optoCheck ( void )
{
  if ( digitalRead(OUT) && status.heating )
  {
    Serial.println("OPT. disconected. Heating off");
    status.heating = false;
    status.hasChanged = true;
    optoHasChanged = true;
    if ( firstOptMsg )
    {
      optoErrorMsg("OPT. ODPOJEN");
      firstOptMsg = false;
    }
    setRele(1);
  }
  else if ( ! digitalRead(OUT) && optoHasChanged ){
    Serial.println("OPT. conected again. Heating on");
    status.heating = true;
    status.hasChanged = true;
    optoHasChanged = false;
    setRele(1);
  }
}

// ***********************************************************

void statusSet(int num)
{
  switch (num)
  {
    case 1:
      /*
      if ( digitalRead(OUT) )// pin from opto condition
      {   
        optoErrorMsg("OPTOCLEN ERR1");
        status.water = true;
        status.hasChanged = true;
        break;                
      }
      */
      firstOptMsg = true;
      status.heating = true;
      status.water = true;
      status.hasChanged = true;
      break;
    case 2:
      status.heating = false;
      status.water = false;
      status.hasChanged = true;
      break;
    case 3:
      status.water = true;
      status.hasChanged = true;
      break;
    case 4:
      status.water = false;
      status.hasChanged = true;
      break;
    case 5:
      /*
      if ( digitalRead(OUT) )// pin from opto condition
      {   
        optoErrorMsg("OPTOCLEN ERR2");
        break;                
      }
      firstOptMsg = true;
      */
      status.heating = true;
      status.hasChanged = true;
      break;
    case 6:
      status.heating = false;
      status.hasChanged = true;
      break;
    default:
      Serial.write( "Error" );
      return;
  }
}

// *****************************************

void parsNumber ( char * number, SBUF * serialBuff )
{
  int j = 0;
  for( int i = numberOffset ; i < numberOffset + acctualNumLen ; i ++ )
    number[j++] = serialBuff->text[i];
  number[j] = '\0';
}

void parsHeader ( char * header, SBUF * serialBuff )
{
  for( int i = 0 ; i < 4 ; i ++ )
    header[i] = serialBuff->text[i+2];         // the "+CMT" is 2 chars from beginng hardDebug for reference
  header[4] = '\0';
}

void parsCommand ( char * command, SBUF * serialBuff )
{
  int j = 0;
  for( int i = commandOffset ; i < commandOffset + acctualCmdSize ; i ++ )
    command[j++] = serialBuff->text[i];
  command[j] = '\0';
}

// *****************************************
void setRele( int fix )
{
  if ( status.heating )
  {
    #ifdef debug
      if ( ! fix )
        Serial.println( "heating rele LOW ( ZAPNUTO )" );
    #endif
    digitalWrite(RELE1_PIN, LOW);
  }
  else
  {
    #ifdef debug
      if ( ! fix )
        Serial.println( "heating rele HIGH ( VYPNUTO )" );
    #endif
    digitalWrite(RELE1_PIN, HIGH);
  }
  if ( status.water )
  {
    #ifdef debug
      if ( ! fix )
        Serial.println( "water rele LOW ( ZAPNUTO )" );
    #endif
    digitalWrite(RELE2_PIN, LOW);
  }
  else
  {
    #ifdef debug
      if ( ! fix )
        Serial.println( "water rele HIGH ( VYPNUTO )" );
    #endif
    digitalWrite(RELE2_PIN, HIGH);
  }

  status.hasChanged = false;
}


// *****************************************

void evalCommand ( char * command, char * number )
{
  if (!strncmp(command, "ZTV", 3))
  {
    response(number, "Arduino: ACK-ZTV");
    statusSet(1);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "VTV", 3))
  {
    response(number, "Arduino: ACK-VTV");
    statusSet(2);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "ZV", 2))
  {
    response(number, "Arduino: ACK-ZV");
    statusSet(3);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "VV", 2))
  {
    response(number, "Arduino: ACK-VV");
    statusSet(4);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "ZPT", 3))
  {
    response(number, "Arduino: ACK-ZPT");
    statusSet(5);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "VPT", 3))
  {
    response(number, "Arduino: ACK-VPT");
    statusSet(6);
    status.hasChanged = true;
  }
  else if (!strncmp(command, "HLP", 3))
  {
    response(number, "HELP");
  }
  else if (!strncmp(command, "STS", 3))
  {
    response(number, statusGet());
  }
  else if (!strncmp(command, "RBT", 3) && 
           !strncmp( number, "SomePriorityNumHere" , acctualNumLen )) // Undocumented, only 
  {
    #ifdef debug
      Serial.write("Rebooting\n");
    #endif
    reboot();
  }
  else
  {
    #ifdef debug
      Serial.write("Command not recognized.\n");
    #endif
  }

  if ( status.hasChanged )
    setRele(0);
}

// *****************************************

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

// *****************************************

// *****************************************

void displayStatus ( void )
{
  if ( status.heating && status.water )
  {
    myGLCD.print("TOPENI: ZAP.", LEFT, 1);
    myGLCD.print("VODA: ZAP.", LEFT, 9);
    myGLCD.update();
  }
    
  if ( status.heating && ! status.water )
  {
    myGLCD.print("TOPENI: ZAP.", LEFT, 1);
    myGLCD.print("VODA: VYP.", LEFT, 9);
    myGLCD.update();
  }
    
  if ( ! status.heating && status.water )
  {
    myGLCD.print("TOPENI: VYP.", LEFT, 1);
    myGLCD.print("VODA: ZAP.", LEFT, 9);
    myGLCD.update();
  }
    
  if ( ! status.heating && ! status.water )
  {
    myGLCD.print("TOPENI: VYP.", LEFT, 1);
    myGLCD.print("VODA: VYP.", LEFT, 9);
    myGLCD.update();
  }
    
}

void manControl ( void )
{
  myGLCD.print("TOPENI", 3, 22);
  myGLCD.print("VODA", 50, 22);
  myGLCD.update();

  cyclicBuffer = cyclicBuffer % 4;
  if ( cyclicBuffer < 0 )
    cyclicBuffer += 4;
  

  if ( ! digitalRead(pins[0])  )
  {
    #ifdef debug
      Serial.println( "ButtonDown" );
    #endif
    cyclicBuffer++;
    delay(200);
  }
  
  if ( ! digitalRead(pins[1]) )
  {
    #ifdef debug
      Serial.println( "ButtonUP" );
    #endif
    cyclicBuffer--;
    delay(200);
  }

  if ( ! digitalRead(pins[2]) )
  {
    #ifdef debug
      Serial.println( "ButtonConfirm" );
    #endif
    if ( cyclicBuffer == 0 )
      statusSet( cyclicBuffer + 5 ); // special case of commands no cover by SMS
    else if ( cyclicBuffer == 1 )
      statusSet( cyclicBuffer + 5 ); // special case of commands no cover by SMS
    else 
      statusSet( cyclicBuffer + 1 );
    
    status.hasChanged = true;
    if ( status.hasChanged )         // this looks nonsensical but little time to test
      setRele(0);
    
    delay(200);
  }

  switch ( cyclicBuffer )
  {
    case 0:
      myGLCD.invertText(true);
      myGLCD.print("ZAP.", 10, 31);
      myGLCD.invertText(false);
      myGLCD.print("VYP.", 10, 40);
      myGLCD.print("ZAP.", 52, 31);
      myGLCD.print("VYP.", 52, 40);
      myGLCD.update();
      break;
    case 1:
      myGLCD.print("ZAP.", 10, 31);
      myGLCD.invertText(true);
      myGLCD.print("VYP.", 10, 40);
      myGLCD.invertText(false);
      myGLCD.print("ZAP.", 52, 31);
      myGLCD.print("VYP.", 52, 40);
      myGLCD.update();
      break;
    case 2:
      myGLCD.print("ZAP.", 10, 31);
      myGLCD.print("VYP.", 10, 40);
      myGLCD.invertText(true);
      myGLCD.print("ZAP.", 52, 31);
      myGLCD.invertText(false);
      myGLCD.print("VYP.", 52, 40);
      myGLCD.update();
      break;
    case 3:
      myGLCD.print("ZAP.", 10, 31);
      myGLCD.print("VYP.", 10, 40);
      myGLCD.print("ZAP.", 52, 31);
      myGLCD.invertText(true);
      myGLCD.print("VYP.", 52, 40);
      myGLCD.invertText(false);
      myGLCD.update();
      break;
    default:
      myGLCD.print("ZAP.", 10, 31);
      myGLCD.print("VYP.", 10, 40);
      myGLCD.print("ZAP.", 52, 31);
      myGLCD.print("VYP.", 52, 40);
  }
}

// *****************************************

void updateSerial()
{ 
  SBUF serialBuff = { "", 0 };

  char command[cmdSize];
  char number[phoneNumLen];
  char header[headerSize];

  //  optoCheck();
  displayStatus();
  manControl();
    
  while (Serial.available())
    SIM.write(Serial.read());
  
  int incomingText = SIM.available();
  
  if ( incomingText > serialBuffMaxSize )
  {
    #ifdef debug
      Serial.println( "Incoming text is too long.\n" );
    #endif
    return;
  }

  for ( int i = 0 ; i < incomingText ; i ++ )
  {
    serialBuff.text[i] = SIM.read();
    serialBuff.len++;
  }

  #ifdef debug
    for ( int i = 0 ; i < serialBuff.len ; i ++ ){
      Serial.write( serialBuff.text[i] );
    }
  #endif
  
  parsHeader( header, &serialBuff );

  if ( strncmp( header, smsHeader, 4 ) )            // +CMT should be incoming messsage
    return;                                         // if the header doesnt match +CMT, return

  #ifdef debug
    Serial.write( "Header match found.\n" );
  #endif
  
  parsNumber( number, &serialBuff );
  
  #ifdef debug
    Serial.write("Parsed number:");
    Serial.write( number );
    Serial.write("\n");
  #endif

  if ( ! checkNumWhitelist( number ) )
  {
    #ifdef debug
      Serial.write("Number not recognized\n");
    #endif
    return;
  }
  
  parsCommand( command, &serialBuff );
  
  #ifdef debug
  Serial.write( "Parsed command: " );
    for ( int i = 0 ; i < strlen(command) ; i ++ )
      Serial.write( command[i] );
  Serial.write( "\n" );
  #endif

  evalCommand( command, number );
}