#include <EEPROM.h>
#include "Wire.h"
#define DS3231_I2C_ADDRESS 0x68
#define BUFFER_SIZE 20
#define EEADR_SSH   0
#define EEADR_SSM   1
#define EEADR_SRH   2
#define EEADR_SRM   3
#define EEADR_MSH   4
#define EEADR_MSM   5
#define EEADR_MRH   6
#define EEADR_MRM   7
#define EEADR_SSD   8
#define EEADR_SRD   9
#define EEADR_MSD   10
#define EEADR_MRD   11
byte sunsetHour;       //vychod slunce hodina
byte sunsetMinute;     //vychod slunce minuta
byte sunriseHour;      //zapad slunce hodina
byte sunriseMinute;    //zapad slunce minta
byte moonsetHour;      //vychod mesice hodina
byte moonsetMinute;    //vychod mesice minuta
byte moonriseHour;     //zapad mesice hodina
byte moonriseMinute;   //zapad mesice minuta
byte sunsetDelay;      //vychod slunce trvani
byte sunriseDelay;     //zapad slunce trvani
byte moonsetDelay;     //vychod mesice trvani
byte moonriseDelay;    //zapad mesice trvani
int W = 5;
int R = 10;
int G = 9;
int B = 11;

typedef enum{
  IDLE_STATE, 
  SUNSET_IDLE, SUNSET_INIT, SUNSET_SOON, SUNSET_MIDDLE, SUNSET_LATE,
  SUNRISE_IDLE, SUNRISE_INIT, SUNRISE_SOON, SUNRISE_MIDDLE, SUNRISE_LATE,
  MOONSET_IDLE, MOONSET_INIT, MOONSET,
  MOONRISE_IDLE, MOONRISE_INIT, MOONRISE
}
ledState_t;

ledState_t ledState;

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val){
    return( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val){
    return( (val/16*10) + (val%16) );
}

// Convert ascii symbols to decimal numbers
byte asciiToDec(byte tens, byte ones){
    return((tens-'0')*10) + (ones-'0');
}

void setup(){
    Wire.begin();
    Serial.begin(9600);
    Serial.println("Komunikace zahajena");
    pinMode(W, OUTPUT); //white  
    pinMode(R, OUTPUT); //red
    pinMode(G, OUTPUT); //green
    pinMode(B, OUTPUT); //bluea

    //inicializace casu z EEPROM
    sunsetHour = EEPROM.read(EEADR_SSH);
    sunsetMinute = EEPROM.read(EEADR_SSM);
    sunsetDelay =  EEPROM.read(EEADR_SSD);
    sunriseHour = EEPROM.read(EEADR_SRH);
    sunriseMinute = EEPROM.read(EEADR_SRM);
    sunriseDelay =  EEPROM.read(EEADR_SRD);
    moonsetHour = EEPROM.read(EEADR_MSH);
    moonsetMinute = EEPROM.read(EEADR_MSM);
    moonsetDelay =  EEPROM.read(EEADR_MSD);
    moonriseHour = EEPROM.read(EEADR_MRH);
    moonriseMinute = EEPROM.read(EEADR_MRM);
    moonriseDelay =  EEPROM.read(EEADR_MRD);
    for (int r = 0 ; r < 2 ; r++){
      for (int g = 0 ; g < 2 ; g++){
        for (int b = 0 ; b < 2 ; b++){
        digitalWrite(R,r); 
        digitalWrite(G,g); 
        digitalWrite(B,b); 
        delay(500); 
        }
      }
    }
    digitalWrite(R,0); 
    digitalWrite(G,0); 
    digitalWrite(B,0); 
}

void loop(){
    compareTime();
    updateTime();
    command();
    ledControl();
}

void updateTime(){
  static long time = 0;
  if((millis() - time) >= 5000){
    time = millis();
    //Serial.println(time/1000);
    displayTime();
    }
}

byte setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year){
    // sets time and date data to DS3231
    if(second > 59 || minute > 59 || hour > 23 || 
       dayOfWeek > 7 || dayOfWeek < 1 || 
       dayOfMonth > 31 || dayOfMonth < 1 || 
       month > 12 || month < 1|| year > 99)
    {
      return 0;
    }
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); // set next input to start at the seconds register
    Wire.write(decToBcd(second)); // set seconds
    Wire.write(decToBcd(minute)); // set minutes
    Wire.write(decToBcd(hour)); // set hours
    Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
    Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
    Wire.write(decToBcd(month)); // set month
    Wire.write(decToBcd(year)); // set year (0 to 99)
    Wire.endTransmission();
    return 1;
}

void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year){
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); // set DS3231 register pointer to 00h
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
    // request seven bytes of data from DS3231 starting from register 00h
    *second = bcdToDec(Wire.read() & 0x7f);
    *minute = bcdToDec(Wire.read());
    *hour = bcdToDec(Wire.read() & 0x3f);
    *dayOfWeek = bcdToDec(Wire.read());
    *dayOfMonth = bcdToDec(Wire.read());
    *month = bcdToDec(Wire.read());
    *year = bcdToDec(Wire.read());
}

void displayTime(){
    byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
    // retrieve data from DS3231
    readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
    // send it to the serial monitor
    Serial.print(hour, DEC);
    // convert the byte variable to a decimal number when displayed
    Serial.print(":");
    if (minute<10){
        Serial.print("0");
    }
    Serial.print(minute, DEC);
    Serial.print(":");
    if (second<10){
        Serial.print("0");
    }
    Serial.println(second, DEC);
    /*Serial.print(" ");
    Serial.print(dayOfMonth, DEC);
    Serial.print("/");
    Serial.print(month, DEC);
    Serial.print("/");
    Serial.print(year, DEC);
    Serial.print(" Day of week: ");
    switch(dayOfWeek){
        case 1:
            Serial.println("Sunday");
            break;
        case 2:
            Serial.println("Monday");
            break;
        case 3:
            Serial.println("Tuesday");
            break;
        case 4:
            Serial.println("Wednesday");
            break;
        case 5:
            Serial.println("Thursday");
            break;
        case 6:
            Serial.println("Friday");
            break;
        case 7:
            Serial.println("Saturday");
            break;
    }*/
}

void command(){
    static char pole[BUFFER_SIZE];
    static byte idx = 0;
    char znak;

    while(Serial.available() > 0){
        znak = Serial.read();
        if(znak == '#'){
            idx = 0;
        }
        else if(znak == '$' && idx > 0){
            pole[idx] = '\0';
            Serial.print("[");
            Serial.print(pole);
            Serial.println("]");
            znak = pole[0];
            switch (znak) {
              case 'S':
                Serial.println("set_time");
                if (pole[1]=='A' && pole[2] == 'H' && asciiToDec(pole[3], pole[4]) <= 24){         //nastaveni hodiny vychodu slunce, prikaz #SAH..
                   sunsetHour = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SSH, sunsetHour);
                   Serial.println("sunset_hour_time_set");
                   }
                   else if (pole[1]=='A' && pole[2] == 'M' && asciiToDec(pole[3], pole[4]) <= 59){ //nastaveni minuty vychodu slunce, prikaz #SAM..
                   sunsetMinute = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SSM, sunsetMinute);
                   Serial.println("sunset_minute_time_set");
                   }
                   else if (pole[1]=='A' && pole[2] == 'D' && asciiToDec(pole[3], pole[4]) <= 99){ //nastaveni delay vychodu slunce, prikaz #SAD..
                   sunsetDelay = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SSD, sunsetDelay);
                   Serial.println("sunset_delay_set");
                   }
                   else if (pole[1]=='P' && pole[2] == 'H' && asciiToDec(pole[3], pole[4]) <= 24){ //nastaveni hodiny zapada slunce, prikaz #SPH..
                   sunriseHour = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SRH, sunriseHour);
                   Serial.println("sunrise_hour_time_set");
                   }
                   else if (pole[1]=='P' && pole[2] == 'M' && asciiToDec(pole[3], pole[4]) <= 59){ //nastaveni minuty zapadu slunce, prikaz #SPM..
                   sunriseMinute = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SRM, sunriseMinute);
                   Serial.println("sunrise_minute_time_set");
                   }
                   else if (pole[1]=='P' && pole[2] == 'D'&& asciiToDec(pole[3], pole[4]) <= 99){ //nastaveni delay zapadu slunce, prikaz #SPD..
                   sunriseDelay = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_SRD, sunriseDelay);
                   Serial.println("sunrise_delay_set");
                   }
                   else if (pole[1]=='M' && pole[2] == 'H' && asciiToDec(pole[3], pole[4]) <= 24){ //nastaveni hodiny vychodu mesice, prikaz #SMH..
                   moonsetHour = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MSH, moonsetHour);
                   Serial.println("moonset_hour_time_set");
                   }
                   else if (pole[1]=='M' && pole[2] == 'M' && asciiToDec(pole[3], pole[4]) <= 59){ //nastaveni minuty vychodu mesice, prikaz #SMM..
                   moonsetMinute = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MSM, moonsetMinute);
                   Serial.println("moonset_minute_time_set");
                   }
                   else if (pole[1]=='M' && pole[2] == 'D' && asciiToDec(pole[3], pole[4]) <= 99){ //nastaveni delay vychodu mesice prikaz #SMD..
                   moonsetDelay = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MSD, moonsetDelay);
                   Serial.println("moonset_delay_set");
                   }
                   else if (pole[1]=='T' && pole[2] == 'H' && asciiToDec(pole[3], pole[4]) <= 24){ //nastaveni hodiny zapadu mesice, prikaz #STH..
                   moonriseHour = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MRH, moonriseHour);
                   Serial.println("moonrise_hour_time_set");
                   }
                   else if (pole[1]=='T' && pole[2] == 'M' && asciiToDec(pole[3], pole[4]) <= 59){ //nastaveni minuty zapadu mesice, prikaz #STM..
                   moonriseMinute = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MRM, moonriseMinute);
                   Serial.println("moonrise_minute_time_set");
                   }
                   else if (pole[1]=='T' && pole[2] == 'D' && asciiToDec(pole[3], pole[4]) <= 99){ //nastaveni delay vychodu mesice prikaz #STD..
                   moonriseDelay = asciiToDec(pole[3], pole[4]);
                   EEPROM.write(EEADR_MRD, moonriseDelay);
                   Serial.println("moonrise_delay_set");
                   }
                   else if (pole[1]=='T' && pole[2] == 'T'){ //nastaveni aktualniho casu RTC #STT SEC MIN HOD DVT DVM MES ROK

                     if(setDS3231time(asciiToDec(pole[3], pole[4]),
                                   asciiToDec(pole[5], pole[6]),
                                   asciiToDec(pole[7], pole[8]),
                                   asciiToDec(pole[9], pole[10]),
                                   asciiToDec(pole[11], pole[12]),
                                   asciiToDec(pole[13], pole[14]),
                                   asciiToDec(pole[15], pole[16])))
                     {
                      Serial.println("time_set");
                     }
                     else
                     {
                      Serial.println("INCORRECT_VALUES");
                     }                
                   }
                   else
                   Serial.println("INCORRECT_VALUES");      
                break;
              case 'P':
                if (pole[1]=='A'){                                //vypise cas vychodu slunce, prikaz #PA
                Serial.print("Sun set time: ");
                if (sunsetHour < 10){
                Serial.print("0");
                }
                Serial.print(sunsetHour, DEC);
                Serial.print(':');
                if (sunsetMinute < 10){
                Serial.print("0");
                }
                Serial.println(sunsetMinute, DEC);  
                }
                else if (pole[1]=='P'){                           //vypise cas zapadu slunce, prikaz #PP
                Serial.print("Sun rise time: ");
                if (sunriseHour < 10){
                Serial.print("0");
                }  
                Serial.print(sunriseHour, DEC);
                Serial.print(':');
                if (sunriseMinute < 10){
                Serial.print("0");
                }
                Serial.println(sunriseMinute, DEC);  
                }
                else if (pole[1]=='M'){                           //vypise cas vychodu mesice, prikaz #PM
                Serial.print("Moon set time: ");
                if (moonsetHour < 10){
                Serial.print("0");
                }
                Serial.print(moonsetHour, DEC);
                Serial.print(':');
                if (moonsetMinute < 10){
                Serial.print("0");
                }
                Serial.println(moonsetMinute, DEC);  
                }
                else if (pole[1]=='N'){                           //vypise cas zapadu mesice, prikaz #PN
                Serial.print("Moon rise time: ");
                if (moonriseHour < 10){
                Serial.print("0");
                }
                Serial.print(moonriseHour, DEC);
                Serial.print(':');
                if (moonriseMinute < 10){
                Serial.print("0");
                }
                Serial.println(moonriseMinute, DEC);  
                }
                else if (pole[1]=='T'){                           //vypise aktualni cas, prikaz #PT
                Serial.print("Current time: ");
                Serial.println("print_time");
                displayTime();  
                }       
                break;
              case 'D':
                if (pole[1]=='A'){                                //vypise delay vychodu slunce, prikaz #DA
                Serial.print("Sun set lenhgt: ");
                Serial.print(sunsetDelay, DEC);
                Serial.println(" min.");
                }
                else if (pole[1]=='P'){                           //vypise delay zapaduu slunce, prikaz #DP
                Serial.print("Sun rise lenhgt: ");
                Serial.print(sunriseDelay, DEC);
                Serial.println(" min.");
                }
                else if (pole[1]=='M'){                           //vypise delay vychodu mesice, prikaz #DM
                Serial.print("Moon set lenhgt: ");
                Serial.print(moonsetDelay, DEC);
                Serial.println(" min.");
                }
                else if (pole[1]=='N'){                           //vypise delay zapadu mesice, prikaz #DN
                Serial.print("Moon rise lenhgt: ");
                Serial.print(moonriseDelay, DEC);
                Serial.println(" min.");
                }
                break;
            }
        }
        else{
            if(idx >= (BUFFER_SIZE - 1)){
                Serial.println("ERROR_LONG_COMMAND");
                idx = 0;
            }
            else{
                pole[idx] = znak;
                idx++;
            }
        }
    }
}

void compareTime(){
  ledState_t state;
  int deltaHourA, deltaHourB, deltaMinuteA, deltaMinuteB;
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  if(ledState == IDLE_STATE || ledState == SUNSET_IDLE || ledState == SUNRISE_IDLE || 
     ledState == MOONSET_IDLE || ledState == MOONRISE_IDLE)
  {
//////////////////////////////////
    state = SUNSET_INIT;
    
    deltaHourA = hour - sunsetHour;
    if(deltaHourA < 0) deltaHourA = hour + 24 - sunsetHour;

    deltaMinuteA = minute - sunsetMinute;
    if(deltaMinuteA < 0) deltaMinuteA = minute + 60 - sunsetMinute;

    deltaHourB = hour - sunriseHour;
    if(deltaHourB < 0) deltaHourB = hour + 24 - sunriseHour;
  
    deltaMinuteB = minute - sunriseMinute;
    if(deltaMinuteB < 0) deltaMinuteB = minute + 60 - sunriseMinute;

    if(deltaHourA > deltaHourB){
      state = SUNRISE_INIT;
      deltaHourA = deltaHourB;
      deltaMinuteA = deltaMinuteB;
    }
    else if(deltaHourA == deltaHourB){
      if(deltaMinuteA > deltaMinuteB){
        state = SUNRISE_INIT;
        deltaHourA = deltaHourB;
        deltaMinuteA = deltaMinuteB;
      }
    }
    deltaHourB = hour - moonsetHour;
    if(deltaHourB < 0) deltaHourB = hour + 24 - moonsetHour;
  
    deltaMinuteB = minute - moonsetMinute;
    if(deltaMinuteB < 0) deltaMinuteB = minute + 60 - moonsetMinute;

    if(deltaHourA > deltaHourB){
      state = MOONSET_INIT;
      deltaHourA = deltaHourB;
      deltaMinuteA = deltaMinuteB;
    }
    else if(deltaHourA == deltaHourB){
      if(deltaMinuteA > deltaMinuteB){
        state = MOONSET_INIT;
        deltaHourA = deltaHourB;
        deltaMinuteA = deltaMinuteB;
      }
    }

    deltaHourB = hour - moonriseHour;
    if(deltaHourB < 0) deltaHourB = hour + 24 - moonriseHour;
  
    deltaMinuteB = minute - moonriseMinute;
    if(deltaMinuteB < 0) deltaMinuteB = minute + 60 - moonriseMinute;

    if(deltaHourA > deltaHourB){
      state = MOONRISE_INIT;
      deltaHourA = deltaHourB;
      deltaMinuteA = deltaMinuteB;
    }
    else if(deltaHourA == deltaHourB){
      if(deltaMinuteA > deltaMinuteB){
        state = MOONRISE_INIT;
        deltaHourA = deltaHourB;
        deltaMinuteA = deltaMinuteB;
      }
    }
//////////////////////////////////////////////////////////////////////
    if (state == SUNSET_INIT && ledState != SUNSET_IDLE){
      //sunSet();
      ledState = SUNSET_INIT;
    }
    if (state == SUNRISE_INIT && ledState != SUNRISE_IDLE){
      //sunRise();
      ledState = SUNRISE_INIT;
    }
    if (state == MOONSET_INIT && ledState != MOONSET_IDLE){
      //moonSet();
      ledState = MOONSET_INIT;
    }
    if (state == MOONRISE_INIT && ledState != MOONRISE_IDLE){
      //moonRise();
      ledState = MOONRISE_INIT;
    }
  }
}

void ledControl(){
  static int i = 0;
  static unsigned long timer = 0;
/*------------------SUNSET_INITIALIZING------------------*/  
  switch(ledState){
    
    case SUNSET_INIT:
      i = 0;
      timer = millis();
      ledState = SUNSET_SOON;
      analogWrite(B, 0);                     //0
      break;
    case SUNSET_SOON:
      if((millis() - timer) >= ((((sunsetDelay*60000)/8)*1)/255)){
        timer = millis();
        analogWrite(R, i);                     //0-255
        analogWrite(G, (i*0.19));              //0-50
        if(i < 255){
          i++;
        }
        else{
          ledState = SUNSET_MIDDLE;
          i = 255;
          analogWrite(G, 50);                   //50
          analogWrite(B, 0);                    //0
        }
      }
      break;
    case SUNSET_MIDDLE: //i musi byt 255
      if((millis() - timer) >= ((((sunsetDelay*60000)/8)*5)/35)){
        timer = millis();
        analogWrite(R, i);                    //255-220
        if(i > 220){
          i--;
        }
        else{
          ledState = SUNSET_LATE;
          i = 0;
        }
      }
      break;
    case SUNSET_LATE:
      if((millis() - timer) >= ((((sunsetDelay*60000)/8)*2)/255)){
        timer = millis();
        analogWrite(R, 220+(i*35/255));       //220-255
        analogWrite(G, 50+(i*205/255));       //50-255
        analogWrite(B, i);                    //0-255
        analogWrite(W, i);                    //0-255
        if(i < 255){
          i++;
        }
        else{
          ledState = SUNSET_IDLE;
          i = 0;
        }
      }
      break;
/*------------------SUNRISE_INITIALIZING------------------*/ 
    case SUNRISE_INIT:
      i = 255;
      timer = millis();
      ledState = SUNRISE_SOON;
      break;
    case SUNRISE_SOON:
      if((millis() - timer) >= ((((sunsetDelay*60000)/8)*2)/255)){
        timer = millis();
        analogWrite(R, 220+(i*7/51));         //255-220
        analogWrite(G, 50+(i*41/51));         //255-50
        analogWrite(B, i);                    //255-0
        analogWrite(W, i);                    //0-255
        if(i > 0){
          i--;
        }
        else{
          ledState = SUNRISE_MIDDLE;
          i = 220;
          analogWrite(G, 50);                   //50
          analogWrite(B, 0);                    //0
        }
      }
      break;
    case SUNRISE_MIDDLE: //i musi byt 255
      if((millis() - timer) >= ((((sunsetDelay*60000)/5)*2)/35)){
        timer = millis();
        analogWrite(R, i);                    //220-255
        if(i < 255){
          i++;
        }
        else{
          ledState = SUNRISE_LATE;
          i = 255;
          analogWrite(B, 0);                    //0
        }
      }
      break;
    case SUNRISE_LATE:
      if((millis() - timer) >= ((((sunsetDelay*60000)/8)*1)/255)){
        timer = millis();
        analogWrite(R, i);                    //255-0
        analogWrite(G, (i*10/51));            //50-0
        if(i > 0){
          i--;
        }
        else{
          ledState = SUNRISE_IDLE;
          i = 0;
        }
      }
      break;
/*------------------MOONSET_INITIALIZING------------------*/ 
    case MOONSET_INIT:
      i = 0;
      timer = millis();
      ledState = MOONSET;
      analogWrite(R, 0);                    //0
      break;
    case MOONSET:
      if((millis() - timer) >= ((moonsetDelay*60000)/60)){
        timer = millis();
        analogWrite(G, i/2);                //0-30
        analogWrite(B, i);                  //0-60
        if(i < 60){
          i++;
        }
        else{
          ledState = MOONSET_IDLE;
          i = 0;
        }
      }
      break;
/*------------------MOONRISE_INITIALIZING------------------*/ 
    case MOONRISE_INIT:
      i = 60;
      timer = millis();
      ledState = MOONRISE;
      analogWrite(R, 0);                    //0
      break;
    case MOONRISE:
      if((millis() - timer) >= ((moonriseDelay*60000)/60)){
        timer = millis();
        analogWrite(G, i/2);                //30-0
        analogWrite(B, i);                  //60-0
        if(i > 0){
          i--;
        }
        else{
          ledState = MOONRISE_IDLE;
          i = 0;
        }
      }
      break;
      
    default:
      i = 0;
      break;
  }
}

