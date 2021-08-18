#include <SoftwareSerial.h>
#include <avr/wdt.h>
#include <LCD5110_Graph.h>

/**
 *    ************** Golbal Definitions **************
 */

/* General */
#define bauds            9600

#define numberLength     13
#define accNumberLength  12

#define smsMaxLen        60

#define commandLength    4
#define accCommandLength 3

#define ATHeaderLen      5
#define accATHeaderLen   4

#define keyboardModulo   4

#define numberOffset    10
#define commandOffset   51


/* Keyboard */
#define keyboard_pinCount 3
#define keyboard_pin1     53
#define keyboard_pin2     51
#define keyboard_pin3     49

/* Display */
extern uint8_t          SmallFont[];
#define optimalContrast 50

/* 4 channel relay */
#define relay_1_pin   44
#define relay_2_pin   46
#define relay_vcc_pin 42

/* 1-Bit Optocoupler */
#define opto_out 52
#define opto_vcc 50

/**
 *   ************** Structs **************
 */

/* Status of heating */
typedef struct TStatus
{   
    bool            heating                  = false;
    bool            water                    = false;
} STS;

/* Linked list of whitelisted numbers */
typedef struct TWhitelist
{
    char            number[numberLength]    = "";
    char          * name                    = "";
    TWhitelist    * next                    = NULL;
} WHT;

/* Incoming sms buffer */
typedef struct TSmsBuffer
{
    char            text[smsMaxLen]         = "";
    char            number[numberLength]    = "";
    char            command[commandLength]  = "";
    char            header[ATHeaderLen]     = "";
} SMS;


/*
 * ZTV - zapni topeni vodu
 * VTV - vypni topeni vodu
 * ZV - zapni vodu
 * VV - vypni vodu
 * HLP - help
 */
enum heating 
{
    ZTV = 1,
    VTV = 2,
    ZV  = 3,
    VV  = 4,
    ZT  = 5,
    VT  = 6,
    HLP = 7,
};

/**
 *   ************** Pinout visualation / documentation **************
 */

/**
 *  Membrain Keyboard
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
 * GSM module
 * 
 * VCC - 4.1 v
 * GND - ground
 * RXD - pin 11
 * TXD - pin 12
 */

/**
 * LCD Display
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

/**
 *   ************** Global variables **************
 */
SoftwareSerial SIM(12, 11);
LCD5110 myGLCD(6,7,8,9,10);
bool debugMode = true;
uint8_t cyclicBuffer = 0;


const int pins[keyboard_pinCount] = { keyboard_pin1,
                             keyboard_pin2, 
                             keyboard_pin3 };

const uint8_t personCount = 3;
char whiteList[personCount][numberLength] = {
};

const char * smsHeader = "+CMT";
const char * helpMSG = "Arduino:\n"
                       "ZTV - Zapni topeni a vodu\n"
                       "VTV - Vypni topeni a vodu\n"
                       "ZV - Zapni vodu\n"
                       "VV - Vypni vodu\n"
                       "HLP - Help\n"
                       "STS - Status";

STS status;
SMS smsBuffer;
WHT list;


/**
 * 
 * Setup functions
 * 
 */

void serialMonitorSetup ( void )
{
    Serial.begin(bauds);
}

void keyboardSetup ( void )
{
    for ( int i = 0 ; i < keyboard_pinCount ; i++ )
        pinMode( pins[i] , INPUT_PULLUP );    
}

void optoSetup ( void )
{
    pinMode(opto_vcc, OUTPUT);
    digitalWrite(opto_vcc, HIGH);   
}

void relaySetup ( void )
{
    pinMode(relay_1_pin, OUTPUT);
    pinMode(relay_2_pin, OUTPUT);
    pinMode(relay_vcc_pin, OUTPUT);
    digitalWrite(relay_1_pin, HIGH);
    digitalWrite(relay_2_pin, HIGH);
    digitalWrite(relay_vcc_pin, HIGH);   
}

void gsmSetup ( void )
{
    SIM.begin(bauds);

    Serial.println("Initializing...");

    SIM.println("AT");
    wrapper();

    SIM.println("AT+CMGF=1");
    wrapper();

    SIM.println("AT+CNMI=1,2,0,0,0");
    /*
    https://stackoverflow.com/questions/58908575/cnmi-command-how-to-receive-notification-and-save-to-sim-card-incoming-sms
    */
    wrapper();
}

void lcdSetup ( void )
{
    myGLCD.InitLCD(optimalContrast);
    myGLCD.setFont(SmallFont);
    myGLCD.disableSleep();
    randomSeed(analogRead(7));
}

/**
 * 
 * Runtime functions
 * 
 */

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void statusGet ( char * respMsg )
{
    strncat( respMsg, "Arduino Status:\n", 17);

    if ( status.heating )
        strncat( respMsg, "Topeni ZAPNUTO\n", 16 );
    else
        strncat( respMsg, "Topeni VYPNUTO\n", 16 );

    if ( status.water )
        strncat( respMsg, "voda ZAPNUTA\0", 13 );
    else
        strncat( respMsg, "voda VYPNUTA\0", 13 );

    return respMsg;
}   

void statusSet( int num )
{
    switch (num)
    {
        case ZTV:
            status.heating  = true;
            status.water    = true;
            break;
        case VTV:
            status.heating  = false;
            status.water    = false;
            break;
        case ZV:
            status.water    = true;
            break;
        case VV:
            status.water    = false;
            break;
        case ZT:
            status.heating  = true;
            break;
        case VT:
            status.heating  = false;
            break;
    }
}

bool evalCommand ( void )
{
    if ( ! strncmp( smsBuffer.command, "ZTV", 3 ) )
        statusSet( ZTV );
    else if ( ! strncmp( smsBuffer.command , "VTV", 3 ) )
        statusSet( VTV );
    else if ( ! strncmp( smsBuffer.command, "ZV", 3 ) ) // fixed prefix issue
        statusSet( ZV );
    else if ( ! strncmp( smsBuffer.command, "VV", 3 ) )
        statusSet( VV );
    else if ( ! strncmp( smsBuffer.command, "HLP", 3 ) )
    {
        smsResponse( smsBuffer.command, "HELP" );
        return 0;
    }
    else if ( ! strncmp( smsBuffer.command, "STS", 3 ) )
    {
        char * resp;
        statusGet( resp );
        smsResponse( smsBuffer.number, resp );
        return 0;
    }
    else if ( ! strncmp( smsBuffer.command, "RBT", 3 ) && 
              ! strncmp( smsBuffer.number, "00000000000000" , accNumberLength ))
        reboot(); // pretty burtal i know
    else
        return 1;
    
    char responseText[25] = "Arduino: ACK-";
    strncat( responseText, smsBuffer.command, strlen( smsBuffer.command ) );
    smsResponse( smsBuffer.number, responseText );

    return 0;
}

void smsResponse( char * number, char * responseText )
{
  /**
   * string for sms example: "AT+CMGS=\"+*********\"\r"
   */

  char atNumMsg[50] = "AT+CMGS=\"+";
  strncat(atNumMsg, number, 12);
  strncat(atNumMsg, "\"\r", strlen("\"\r"));

  Serial.println("Sending SMS...");

  SIM.println("AT");
  delay(500);
  SIM.println("AT+CMGF=1");
  delay(500);
  SIM.println(atNumMsg);
  delay(500);

  if ( strncmp( "HELP", responseText, 4 ) == 0 )
    responseText = helpMSG; // this might not work :D
  
  SIM.print( responseText );
  delay(500);
  SIM.write(26);
  Serial.println( "Text Sent." );
  delay(500);
}


void displayStatus ( void )
{
    // Current status display
    char statusPart[2][15];

    if ( status.heating )
        strncpy( statusPart[0], "TOPENI: ZAP.\0", sizeof(statusPart[0]));
    else 
        strncpy( statusPart[0], "TOPENI: VYP.\0", sizeof(statusPart[0]));
    
    if ( status.water )
        strncpy( statusPart[1], "VODA: ZAP.\0", sizeof(statusPart[1]));
    else
        strncpy( statusPart[1], "VODA: VYP.\0", sizeof(statusPart[1]));
    
    myGLCD.print(statusPart[0], LEFT, 1);
    myGLCD.print(statusPart[1], LEFT, 9);
    
    
    // Current selection display
    myGLCD.print("TOPENI", 3, 22);
    myGLCD.print("VODA", 50, 22);

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
            myGLCD.update();
    }
}

void parsNumber ( void )
{
  int j = 0;
  for( int i = numberOffset ; i < numberOffset + accNumberLength ; i ++ )
    smsBuffer.number[j++] = smsBuffer.text[i];
  smsBuffer.number[j] = '\0';
}

void parsHeader ( void )
{
  for( int i = 0 ; i < 4 ; i ++ )
    smsBuffer.header[i] = smsBuffer.text[i+2];
  smsBuffer.header[4] = '\0';
}

void parsCommand ( void )
{
  int j = 0;
  for( int i = commandOffset ; i < commandOffset + accCommandLength ; i ++ )
    smsBuffer.command[j++] = smsBuffer.text[i];
  smsBuffer.command[j] = '\0';
}

void manualControl ( void )
{
    cyclicBuffer = cyclicBuffer % keyboardModulo;
    if ( cyclicBuffer < 0 )
        cyclicBuffer += keyboardModulo;
  
    if ( ! digitalRead(pins[0])  )
    {
        cyclicBuffer++;
        delay(200);
    }
  
    if ( ! digitalRead(pins[1]) )
    {
        cyclicBuffer--;
        delay(200);
    }

    if ( ! digitalRead(pins[2]) )
    {
        statusSet( cyclicBuffer  );
        delay(200);
    }

}

void clearSMSBuffer ( void )
{
    strncpy( smsBuffer.command, "", sizeof( smsBuffer.command ));
    strncpy( smsBuffer.number , "", sizeof( smsBuffer.number ));
    strncpy( smsBuffer.text   , "", sizeof( smsBuffer.text ));
    strncpy( smsBuffer.header , "", sizeof( smsBuffer.header ));
}

bool checkNumWhitelist( void )
{
    bool isValid = false;
    
    for (int i = 0; i < personCount; i++)
    {
        if ( debugMode )
        {
            Serial.write("comparing: ");
            Serial.write("\n");
            for (int j = 0; j < accNumberLength; j++)
                Serial.write(smsBuffer.number[j]);
            Serial.write("\n");
            for (int j = 0; j < accNumberLength; j++)
                Serial.write(whiteList[i][j]);
            Serial.write("\n");
        }

        if (!strncmp(smsBuffer.number, whiteList[i], accNumberLength))
        {
            Serial.write("Number recognized.\n");
            isValid = true;
            break;
        }
    }

    return isValid;
}

void smsControl ( void )
{
    if ( SIM.available() > smsMaxLen )
    {
        if ( debugMode )
            Serial.println( "Incoming text is too long.\n" );
        return;
    }

    for ( int i = 0 ; i < SIM.available() ; i ++ )
        smsBuffer.text[i] = SIM.read();


    if ( debugMode )
        for ( int i = 0 ; i < strlen(smsBuffer.text) ; i ++ )
            Serial.write( smsBuffer.text[i] );

    parsHeader();

    if ( strncmp( smsBuffer.header, smsHeader, 4 ) )
    {
        if ( debugMode )
            Serial.write( "Header rejected.\n" );
        return;                                         
    }

    if ( debugMode )
        Serial.write( "Header match found.\n" );
  
    parsNumber();
    
    if ( debugMode )
    {
        Serial.write("Parsed number:");
        Serial.write( smsBuffer.number );
        Serial.write("\n");
    }

    if ( ! checkNumWhitelist() )
    {
        Serial.write("Number rejected\n");
        return;
    }

    parsCommand();

    if ( debugMode )
    {
        Serial.write( "Parsed command: " );
        Serial.write( smsBuffer.command );
        Serial.write( "\n" );
    }

    if ( evalCommand() )
        Serial.println( "Command not recognized." );
    
    clearSMSBuffer();
}

void wrapper ( void )
{
    displayStatus();
    
    manualControl();

    if ( SIM.available() )
        smsControl();

    //TODO input buffer
    while (Serial.available())
        SIM.write(Serial.read());
}

void greetMsg ( void )
{
      smsResponse( "1100000000000", "Hello from Arduino! :)\0" );
}

/**
 * 
 * Setup nad loop
 * 
 */
void setup( void )
{
    if ( debugMode )
    {
        serialMonitorSetup();
        greetMsg();
    }
    keyboardSetup();
    optoSetup();
    relaySetup();
    gsmSetup();
    lcdSetup();
}

void loop( void )
{
    wrapper();
}