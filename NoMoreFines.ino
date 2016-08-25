#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//1) RST – Reset                      pin6             
//2) CE – Chip Enable                 pin7             
//3) D/C – Data/Command Selection     pin5             
//4) DIN – Serial Input               pin4             
//5) CLK – Clock Input                pin3            
//6) VCC – 3.3V                       VCC
//7) LIGHT – Backlight Control
//8) GND – Ground                     GND

#define GPS_TX      8
#define GPS_RX      9

#define LCD_RESET   6
#define LCD_CE      7
#define LCD_DATCMD  5
#define LCD_DATA    4
#define LCD_CLK     3   
#define LCD_LIGHT                 

#define BUTTON1     15
#define BUTTON2     14
#define BUTTON3     16
#define BUTTON4     10
#define BUZZER      A0

#define MODE_DEFAULT    0
#define OPTIONS_MENU    1
#define SPEED_MENU      2
#define ODO_MENU        3

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences. 
#define GPSECHO  false

Adafruit_PCD8544 display = Adafruit_PCD8544(LCD_CLK, LCD_DATA, LCD_DATCMD, LCD_CE ,LCD_RESET);
SoftwareSerial mySerial(GPS_TX, GPS_RX);
Adafruit_GPS GPS(&mySerial);
char hours = 0;
char minutes = 0;
char seconds = 0;
char gpsSpeed = 0;
char gpsFixQual = 0;
char prev_seconds = 0;
char buffer[20];
double tripDistance = 0;
long odoDistance = 0;
char contrast = 30;
int mode = MODE_DEFAULT;
int speedlimit[6];
int select   = 0;
bool overspeeding = false;
bool buzzerOn = false;

#define XPOS 0
#define YPOS 1
int i = 0;
uint32_t timer = millis();
uint32_t timer1 = millis();
const unsigned int TIMEZONE = +12;


void setup()   {
   Serial.begin(115200);
   display.begin();
   display.setContrast(contrast);
   display.clearDisplay();
   display.display();
   display.clearDisplay();

   pinMode(BUTTON1, INPUT);
   pinMode(BUTTON2, INPUT);
   pinMode(BUTTON3, INPUT);
   pinMode(BUTTON4, INPUT);
   pinMode(BUZZER, OUTPUT);

   // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
   GPS.begin(9600);

   EEPROMLoad();
  //Serial.print("Odo = "); Serial.println(odoDistance); 

   if ( (contrast < 0) || (odoDistance < (long)0) )
   {
      contrast = 30;
      odoDistance = 0;
      speedlimit[0] =  55;
      speedlimit[1] =  65;
      speedlimit[2] = 105;
      speedlimit[3] = 115;
      speedlimit[4] = 120;
      speedlimit[5] = 200; 
   }

}

void loop() {
    char c;
    
     switch (mode)
     {
        case MODE_DEFAULT :
          c = GPS.read();
          // if you want to debug, this is a good time to do it!
          if (GPSECHO)
            if (c) Serial.print(c);
      
          // if a sentence is received, we can check the checksum, parse it...
          if (GPS.newNMEAreceived()) {   
             if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
                return;  // we can fail to parse a sentence in which case we should just wait for another
             hours = GPS.hour;
             minutes = GPS.minute;
             seconds = GPS.seconds;
             gpsSpeed = (char)(1.852 * GPS.speed);
             gpsFixQual = (char)GPS.fixquality;
        }
      
        if (seconds != prev_seconds) {
           prev_seconds = seconds;
           UpdateDisplay();
        }
       if (timer1 > millis())  timer1 = millis();
       if (millis() - timer1 > 1000) {
          timer1 = millis(); // reset the timer
          UpdateDisplay();  //approximately every 1000 ms update the screen
       }
        
        if (timer > millis())  timer = millis();
        if (millis() - timer > 200) { 
          timer = millis(); // reset the timer
          CheckOverspeeding();
          if ( overspeeding && !buzzerOn ) {
             buzzerOn = true;
          }
          else {
             buzzerOn = false;
          }
          digitalWrite(BUZZER, buzzerOn); 
        }
        break;
        
     case OPTIONS_MENU:
        ShowOptionsMenu();
        break;
        
     case SPEED_MENU:   
        ShowSpeedLimitMenu();
        break;

     case ODO_MENU:
        ShowSetOdoMenu();
        break;   
  }

  CheckButtonPress();

}

void UpdateDisplay()
{
     display.clearDisplay();
     drawSpeed(gpsSpeed);
     drawTime();
     drawGpsFixQual();
     drawTripDistance();
     drawOdoDistance();
     display.display();
}

void drawGpsFixQual()
{
   const int X = display.width()-1;
   const int Y = 0;
   const int H = 6;
   const int N = 3;
   const int W = N*5;
   if (gpsFixQual > 0)
       display.drawRect(X-W, Y+H, N,-1, BLACK);
   if (gpsFixQual > 1)
       display.drawRect(X-W+N, Y+H, N,-2, BLACK);
   if (gpsFixQual > 2)
       display.drawRect(X-W+N*2, Y+H, N,-3, BLACK);
   if (gpsFixQual > 3)
       display.drawRect(X-W+N*3, Y+H, N,-4, BLACK);
   if (gpsFixQual > 4)
       display.drawRect(X-W+N*4, Y+H, N,-5, BLACK);
}



void drawSpeed(int speed)
{
  display.setTextSize(3);
  display.setTextColor(BLACK);
  if (speed < 100)
    display.setCursor(25,12);
  else
    display.setCursor(15,12);
  sprintf(buffer,"%02d",speed); 
  display.print(buffer);
}

void drawTime()
{
    display.setTextSize(1);
    unsigned int hourTZ = hours + TIMEZONE;
    if (hourTZ < 0) { hourTZ += 24; }
    if (hourTZ == 24) { hourTZ = 24 - hourTZ; }
    display.setCursor(0,0);
    sprintf(buffer,"%02d:%02d:%02d",hourTZ, minutes, seconds); 
    display.print(buffer);
}

void drawTripDistance()
{
   display.setTextSize(1);
   display.setCursor(0,display.height()-7);
   display.print(tripDistance);
}

void drawOdoDistance()
{
   display.setTextSize(1);
   sprintf(buffer,"%d",(int)odoDistance);
   int width = strlen(buffer);
   
   display.setCursor(display.width()-width*7,display.height()-7);
   display.print((long int)odoDistance);
}  

void CheckOverspeeding() {
  overspeeding = false;
  for (int i=0; i<6; i+=2) {
      if ( (gpsSpeed > speedlimit[i]) && (gpsSpeed < speedlimit[i+1]) ) {
          overspeeding = true;
      }
  }
}

void ShowOptionsMenu() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(18, 0);
    display.print("SETTINGS");
    display.setCursor(0, 12);
    display.print("< Speed ");
    display.setCursor(display.width()-31, 12);
    display.print("Odo >");
    display.setCursor(0, 30);
    display.print("< Misc");
     display.setCursor(display.width()-37, 30);
     display.print("Exit >");
     display.display();
    delay(200);
}

void ShowSetOdoMenu() {
  display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(18, 0);
    display.print("ODOMETER");
    display.setCursor(0, 12);
    display.print("< UP");
    display.setCursor(0, 36);
    display.print("< DOWN");
    display.setCursor(display.width()-37, 36);
    display.print("Save >");
    display.setCursor(24, 24);
    display.print(odoDistance);
    display.display();
    delay(200);
}

void  ShowSpeedLimitMenu() {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(28, 0);
    display.print("STRT STOP");
    display.setCursor(0, 10);
    display.print("SPD1");
    display.setCursor(0, 20);
    display.print("SPD2");
    display.setCursor(0, 30);
    display.print("SPD3");

    char X = 0;
    char Y = 0;
    switch (select) {
      case 0: X=0; Y=0; break;
      case 1: X=28; Y=0; break;
      case 2: X=0; Y=10; break;
      case 3: X=28; Y=10; break;    
      case 4: X=0; Y=20; break;
      case 5: X=28; Y=20; break;         
    }
    display.drawRect(30+X, 18+Y, 24,-9, BLACK);
    
    display.setCursor(32, 10); display.print(speedlimit[0]);
    display.setCursor(32, 20); display.print(speedlimit[2]);
    display.setCursor(32, 30); display.print(speedlimit[4]);
    display.setCursor(60, 10); display.print(speedlimit[1]);
    display.setCursor(60, 20); display.print(speedlimit[3]);
    display.setCursor(60, 30); display.print(speedlimit[5]);

    display.setCursor(0, display.height()-7);
    display.print("< ADJ");
    display.setCursor(display.width()-37, display.height()-7);
    display.print("SAVE >");
    display.display();
    delay(200);
}

void CheckButtonPress() {
     bool saveAndExit = false;
     static bool but2Pressed = false;
     bool but1 = !digitalRead(BUTTON1);
     bool but2 = !digitalRead(BUTTON2);
     bool but3 = !digitalRead(BUTTON3);
     bool but4 = !digitalRead(BUTTON4);
     int butPressed = 0;
     static int lastbut = 5;
     static int incrVal = 0;

    if ( but1 ) butPressed = 1;
    if ( but2 ) butPressed = 2;
    if ( but3 ) butPressed = 3;
    if ( but4 ) butPressed = 4;

     switch (mode)  {
        case MODE_DEFAULT:
           switch (butPressed) {   
                case 3: if (contrast < 60) {
                            contrast += 1;
                            display.setContrast(contrast);
                             delay(200);
                        }
                        break;
                case 4: if (contrast > 0) {
                           contrast -= 1;   
                           display.setContrast(contrast);
                           delay(200);
                        }
                        break;
           }   
           if (but1 && but2)  {
              mode = OPTIONS_MENU; 
           }
           if (!but1 && but2)  {
             if ( (lastbut == 2) && (but2Pressed == false) ) {
                 but2Pressed = true;  
                 tripDistance = 0;
             }
           }
           else {
              but2Pressed = false;
           }
           break;        
        case OPTIONS_MENU:
           delay(200);
           switch (butPressed) {
             case 1: mode = SPEED_MENU; break;
             case 3: mode = ODO_MENU; break;
             case 4: mode = MODE_DEFAULT; break;
             default : break;
           }
           break;
        case ODO_MENU:
             if (lastbut == butPressed) {
                incrVal = incrVal*2+1;
             }
             else {
                incrVal = 0;
             }
              switch (butPressed) {
                 case 1: odoDistance += incrVal; break;
                 case 2: if (odoDistance >= incrVal) odoDistance -= incrVal; break;
                 case 4:  saveAndExit = true ; break;
              }
             delay(100);
             break;
           
        case SPEED_MENU:
           switch (butPressed) {
             case 1: if  (speedlimit[select] < 200) speedlimit[select] += 1; break;
             case 2: if  (speedlimit[select] > 0) speedlimit[select] -= 1; break;
             case 3: if (select < 6) select += 1;
                     if (select == 6) select = 0;
                     break;
             case 4:  saveAndExit = true ; break;
           }
           break;
     }  
     lastbut = butPressed;
     if (butPressed > 0) {
        digitalWrite(BUZZER, true); 
        delay(50);
     }
     digitalWrite(BUZZER, false); 
     if (saveAndExit) {
        lastbut = 5;
        EEPROMSave(); 
        mode = MODE_DEFAULT;  
     } 
}

void EEPROMLoad() {
   int addr = 0;
   contrast = EEPROM.read(addr++);
   for (int i=0; i<6; i++) {
      addr += i*2;
      speedlimit[i] = EEPROMReadInt(addr);
   }
   addr += 2;
   odoDistance = EEPROMReadlong( addr );
}

void EEPROMSave() {
   int addr = 0;
   EEPROM.update(addr++, contrast);
   for (int i=0; i<6; i++) {
      addr += i*2;
      EEPROMUpdateInt(addr, speedlimit[i]);
   }
   addr += 2;
   EEPROMWritelong( addr, odoDistance );
}

//This function will write a 2 byte integer to the eeprom at the specified address and address + 1
void EEPROMUpdateInt(int p_address, int p_value) {
     byte lowByte = ((p_value >> 0) & 0xFF);
     byte highByte = ((p_value >> 8) & 0xFF);

     EEPROM.update(p_address, lowByte);
     EEPROM.update(p_address + 1, highByte);
}

//This function will read a 2 byte integer from the eeprom at the specified address and address + 1
unsigned int EEPROMReadInt(int p_address) {
     byte lowByte = EEPROM.read(p_address);
     byte highByte = EEPROM.read(p_address + 1);

     return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, long value) {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.update(address, four);
      EEPROM.update(address + 1, three);
      EEPROM.update(address + 2, two);
      EEPROM.update(address + 3, one);
}

long EEPROMReadlong(long address) {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
      


