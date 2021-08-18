#include <SoftwareSerial.h>
#include <avr/wdt.h>
#define bauds 9600
#define serialBuffMaxSize 200
#define cmdSize 5               // it is 3  but let there be some space
#define phonNumLen 20           
#define acctualNumLen 12
#define headerSize 5            // it is 4 but let there be some space
#define numberOffset 10
#define commandOffset 51

#define debug
//#define hardDebug
//#define greetSMS


/*
 * Numbers whitelisted 
 */
const int personCount = 3;
char whiteList[personCount][phonNumLen] = {
};


/**
 * Golbal Definitions
 */
bool firstRun = true;
bool incomingMSG = false;
const char * smsHeader = "+CMT";
const char * helpMSG = "Arduino:\n"
                       "ZTV - Zapni topeni a vodu\n"
                       "VTV - Vypni topeni a vodu\n"
                       "ZV - Zapni vodu\n"
                       "VV - Vypni vodu\n"
                       "HLP - Help\n"
                       "STS - Status\n";


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

SoftwareSerial SIM(12, 10);
STS status;

// *****************************************

void setup()
{

  Serial.begin(bauds);
  SIM.begin(bauds);

  Serial.println("Initializing...");
  delay(1000);
  SIM.println("AT");
  updateSerial();
  SIM.println("AT+CMGF=1");
  updateSerial();
  SIM.println("AT+CNMI=1,2,0,0,0");
  updateSerial();
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

  Serial.println("Sending SMS...");

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
  Serial.println("Text Sent.");
  delay(500);
}

// *****************************************

void fresponse(char *cislo, char *responseText)
{
  Serial.write("Fake message sent.\n");
}

// *****************************************

/*
 * ZTV - zapni topeni vodu
 * VTV - vypni topeni vodu
 * ZV - zapni vodu
 * VV - vypni vodu
 * HLP - help
 */

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

void statusSet(int num)
{
  switch (num)
  {
    case 1:
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
    default:
      Serial.write( "Error" );
      return;
  }
}

// *****************************************

void parsNumber ( char * number, SBUF * serialBuff )
{
  int j = 0;
  for( int i = numberOffset ; i < numberOffset + 12 ; i ++ )
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
  for( int i = commandOffset ; i < commandOffset + 3 ; i ++ )
    command[j++] = serialBuff->text[i];
  command[j] = '\0';
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
  else if (!strncmp(command, "HLP", 3))
  {
    response(number, "HELP");
  }
  else if (!strncmp(command, "STS", 3))
  {
    response(number, statusGet());
  }
  else if (!strncmp(command, "RBT", 3) && 
           !strncmp( number, "0000000000000" , acctualNumLen )) // Only someone
  {
    Serial.write("Rebooting\n");
    delay(500);
    reboot();
  }
  else
  {
    Serial.write("Command not recognized.\n");
  }
  
  if ( status.hasChanged )
  {
    if ( status.heating )
    {
      //kod
      Serial.write("Some switch moved.\n");
    }
    else
    {
      //kod
      Serial.write("Some switch moved.\n");
    }
    if ( status.water )
    {
      //kod
      Serial.write("Some switch moved.\n");
    }
    else
    {
      //kod
      Serial.write("Some switch moved.\n");
    }
  }
}

// *****************************************

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

// *****************************************

void updateSerial()
{
  delay(500);
  
  #ifdef greetSMS
    if ( firstRun )
    {
      response( "0000000000003", "Hello from Arduino! :)\0" );
      firstRun = false;
    }
  #endif
  
  SBUF serialBuff = { "", 0 };

  char command[cmdSize];
  char number[phonNumLen];
  char header[headerSize];

  

  while (Serial.available())
    SIM.write(Serial.read());
  
  int incomingText = SIM.available();
  
  if ( incomingText > serialBuffMaxSize )
    Serial.println( "Incoming text is too long.\n" );

  for ( int i = 0 ; i < incomingText ; i ++ )
  {
    serialBuff.text[i] = SIM.read();
    serialBuff.len++;
    incomingMSG = true;
  }

  #ifdef debug
    for ( int i = 0 ; i < serialBuff.len ; i ++ )
      Serial.write( serialBuff.text[i] );
  #endif

  parsHeader( header, &serialBuff );

  if ( strncmp( header, smsHeader, 4 ) )            // +CMT should be incoming messsage
  {
    incomingMSG = false;                            // set to false bcs the message was not sms         
    return;                                         // if the header doesnt match +CMT, return
  }

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
    Serial.write("Number not recognized\n");
    incomingMSG = false;
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

  incomingMSG = false;                         // after the message is processed, set the flag to false
}