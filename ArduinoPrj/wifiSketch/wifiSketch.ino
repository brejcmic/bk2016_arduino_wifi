#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(8, 7); // RX, TX

//=========================================================================================
//Symbolicke konstanty
//=========================================================================================
#define LED_RED             4
#define LED_YEL             2

#define PWM_W               3
#define PWM_R               5
#define PWM_G               6
#define PWM_B               9

#define VERSION   "Aquaduino verze 160420"

#define DS3231_I2C_ADDRESS 0x68

#define esp8266Ser  softSerial
#define ESP8266SPEED        9600// cilova rychlost komunikace
#define ESP8266CHARCT       20// pocet najednou odesilanych znaku
#define ESP8266TXPER        10// perioda vysilani v ms
#define ESP8266PACKETLEN    1024 //maximalni velikost packetu v byte
#define COM_WATCHDOG_TIME   3000// cas v ms, po kterem se resetuje stav RX
#define COM_RX_LEN          32 //delka fifa pro prijem z esp8266
#define COM_SR_LEN          8 //delka fifa pro prijem ze servisni linky
#define COM_MSG_LEN         64 //delka kruhoveho bufferu pro ulozeni servisni zpravy, mocnina 2
#define COM_MSG_MSK         COM_MSG_LEN-1 //maska pro pretekani indexu bufferu servisni zpravy

#define SD_FILE_HEAD        F("html/head.txt")
#define SD_FILE_MNPG        F("html/mainpage.htm")
#define SD_FILE_WIFI        F("wificfg.txt")
#define SD_FILE_SSET        F("cfg/sset.txt")
#define SD_FILE_SRIS        F("cfg/sris.txt")
#define SD_FILE_MSET        F("cfg/mset.txt")
#define SD_FILE_MRIS        F("cfg/mris.txt")

#define DEBUG       0
#define debugSer    Serial

#if DEBUG == 1
#define PRTDBG(x)   debugSer.println(F(x)) //jen pro hlaseni
#define WRTDBG(x)   debugSer.write(x) //bez konverze
#define WRCDBG(x)   debugSer.print(x) //s konverzi
#define wrLog(x, b) debugSer.write(x) //trvaly vypis logu
#define wrMsg(x)    debugSer.print(F(x)) //vypis zpravy
#else
#define PRTDBG(x)
#define WRTDBG(x)
#define WRCDBG(x)
#define wrLog(x, b) if(b) debugSer.write(x) //podmineny vypis logu
#define wrMsg(x)    debugSer.print(F(x)) //vypis zpravy
#endif

//=========================================================================================
//Nove datove typy
//=========================================================================================
typedef enum {
  WAITTX, REQHEADTX, REQMNPGTX, HEADTX, MAINPAGETX, ACKSNDTX, ACKTX, NEXTPACKTTX, LASTPACKTTX, ATSENDTX, ONBUTTX, OFFBUTTX
} txStates_t;

typedef enum {
  WAITRX, CLIENTRX, COMMARX, BYTECNTRX, REQRX, URLRX, FLUSHRX, SERVISMODERX
} rxStates_t;

typedef enum {
  LOGSR, WAITSR, ATCOMMSR, ATSENDSR
} srStates_t;

typedef struct {
  byte second;
  byte minute;
  byte hour;
  byte dayOfWeek;
  byte dayOfMonth;
  byte month;
  byte year;
} time_t;

typedef union {
  byte mem[3];
  struct{
    byte hour;
    byte minute;
    byte ramp;
  }p;
} deadline_t;

typedef enum{
  IDLE_STATE, 
  SUNSET_IDLE, SUNSET_INIT, SUNSET_SOON, SUNSET_MIDDLE, SUNSET_LATE,
  SUNRISE_IDLE, SUNRISE_INIT, SUNRISE_SOON, SUNRISE_MIDDLE, SUNRISE_LATE,
  MOONSET_IDLE, MOONSET_INIT, MOONSET,
  MOONRISE_IDLE, MOONRISE_INIT, MOONRISE
}
ledState_t;

//=========================================================================================
//Globalni promenne
//=========================================================================================
deadline_t  sunset;
deadline_t  sunrise;
deadline_t  moonset;
deadline_t  moonrise;

ledState_t ledState;

//=========================================================================================
//PROGRAM
//=========================================================================================
//Setup
//=========================================================================================

void setup() {
  time_t currTime;
  pinMode(LED_RED, OUTPUT);           // set pin to input
  pinMode(LED_YEL, OUTPUT);           // set pin to input
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_YEL, LOW);
  com_setupServisCh();
  if(!initSDCard()) while(1);//nekonecna smycka
  if(!com_setupEsp8266())
  {
    digitalWrite(LED_YEL, HIGH);
    while(1);
  }
  digitalWrite(LED_YEL, HIGH);
  digitalWrite(LED_RED, LOW);
  //RTC
  Wire.begin();
  readDS3231time(&currTime);
  
  pinMode(PWM_W, OUTPUT);
  pinMode(PWM_R, OUTPUT);
  pinMode(PWM_G, OUTPUT);
  pinMode(PWM_B, OUTPUT);
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
}
//=========================================================================================
//Loop
//=========================================================================================
void loop() {
  com_monitor();
  compareTime();
  ledControl();
}

//=========================================================================================
//Komunikace
//=========================================================================================
byte com_setupServisCh(void)
{
  //navazani komunikace servisni linky
  //----------------------------------------------------
  debugSer.begin(115200, SERIAL_8N1);
  return 1;
}
//=========================================================================================
byte com_setupEsp8266()
{
  byte ret;
  union{
    char ch;
    char rx[4];
  }var;
  byte idx;
  File thisFile;
  
  ret = 2; //pocet pokusu o navazani spojeni
  //Predpoklada se na pocatku uspesna inicializace
  ret = (ret << 1) + 1;
  while (ret > 1)
  {
    //RESET pokud je treba
    //----------------------------------------------------
    if(!(ret & 1))
    {
      esp8266Ser.println(F("AT+RST"));
      wrMsg("\nRESET esp8266.\n");
      for (idx = 0; idx < 5; idx++)
      {
        wrMsg("*");
        delay(1000);
      }
      ret++;//nastaveni posledniho bitu
    }
    //navazani komunikace s esp8266 - prenastaveni
    //rychlosti
    //----------------------------------------------------
    esp8266Ser.begin(115200);
    esp8266Ser.print(F("AT+UART_CUR="));
    esp8266Ser.print(ESP8266SPEED);
    esp8266Ser.print(F(",8,1,0,0"));
    esp8266Ser.print(F("\r\n"));
    wrMsg("\nInicializace komunikace s esp8266.\n");
    for (idx = 0; idx < 5; idx++)
    {
      wrMsg("*");
      delay(1000);
    }
    esp8266Ser.begin(ESP8266SPEED);
    //Kontrola primu
    //----------------------------------------------------
    wrMsg("\nPokus o spojeni\n");
    esp8266Ser.println(F("AT"));
    com_delay(0); //reset casu
    idx = 0;
    while (!com_checkRxESP8266For(String(F("OK")), var.rx, 4) && (ret & 1))
    {
      if(com_delay(1000))
      {
        idx++;
        wrMsg("*");
      }
      else if(idx > 2)
      {
        wrMsg("\n<X> esp8266 neodpovida.");
        ret--;//shozeni nejnizsiho bitu
      }
    }
    if(ret & 1)
    {
      wrMsg("\n<ok> Prijata odpoved od esp8266.");
      //Komunikace navazana, nastaveni nasobneho pripojeni
      //----------------------------------------------------
      esp8266Ser.println(F("AT+CIPMUX=1"));
      com_delay(0); //reset casu
      while (!com_checkRxESP8266For(String(F("OK")), var.rx, 4) && (ret & 1))
      {
        if(com_delay(1000))
        {
          wrMsg("\n<X> Nepodarilo se nastavit CIPMUX=1");
          ret--;//shozeni nejnizsiho bitu
        }
      }
    }
    if(ret & 1)
    {
      //Pokus o pripojeni k wifi
      //----------------------------------------------------
      thisFile = SD.open(SD_FILE_WIFI, FILE_READ);
      if(!thisFile)
      {
        wrMsg("\n<X> Nedostupne nastaveni WIFI site (wificfg.txt)");
      }
      else
      {
        wrMsg("\nPripojovani k WIFI siti\n");
        WRCDBG(thisFile.available());
        PRTDBG("\n");
        while(thisFile.available())
        {
          var.ch = thisFile.read();
          esp8266Ser.print(var.ch);
          WRCDBG(var.ch);
        }
        esp8266Ser.print(F("\r\n"));
        PRTDBG("\n");
        thisFile.close();
        com_delay(0); //reset casu
        idx = 0;
        while (!com_checkRxESP8266For(String(F("OK")), var.rx, 4) && (ret & 1))
        {
          if(com_delay(1000))
          {
            idx++;
            wrMsg("*");
          }
          else if(idx > 15)
          {
            wrMsg("\n<X> WIFI sit nedostupna.");
            ret--;//shozeni nejnizsiho bitu
          }
        } 
      }
    }
    if(ret & 1)
    {
      //Nastavit zarizeni jako server
      //----------------------------------------------------
      esp8266Ser.println(F("AT+CIPSERVER=1,80"));//port 80
      com_delay(0); //reset casu
      while (!com_checkRxESP8266For("OK", var.rx, 4)  && (ret & 1))
      {
        if(com_delay(1000))
        {
          wrMsg("\n<X> Nepodarilo se spustit server.");
          ret--;//shozeni nejnizsiho bitu
        }
      }
    }
    if(ret & 1)
    {
      //Vse ok
      //----------------------------------------------------
      wrMsg("\n<ok> Wifi pripojena.");
      ret = 1;
    }
    else
    {
      //Inicializace neprobehla uspesne - pokus o reset
      //----------------------------------------------------
      ret -= 2;
    }
  }
  return ret;
}
//=========================================================================================
byte com_checkRxESP8266For(String cArr, char *fifo, unsigned int flen)
{
  char dataByte;

  if(esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    com_putCharInFifo(dataByte, fifo, flen);
    WRCDBG(dataByte);
    return com_findInFifo(cArr, fifo, flen);
  }
  return 0;
}
//=========================================================================================
//Funkce hleda retezec v poslednich flen prijatych znacich.
byte com_findInFifo(String cArr, const char *fifo, unsigned int flen)
{
  byte ret; //navratova hodnota
  byte j; //index znaku v retezci

  //zjisteni delky retezce
  j = cArr.length();

  ret = (j != 0 && j <= flen);
  flen = 0;
  //hledani retezce ve fifo
  while (j > 0 && ret)
  {
    j--;
    ret = (fifo[flen] == cArr.charAt(j));
    flen++;
  }
  return ret;
}
//=========================================================================================
//Funkce nahraje novy znak do fifo, novy znak bude mit index 0.
void com_putCharInFifo(char nChar, char *fifo, unsigned int flen)
{
  byte b;

  b = (flen > 1);
  while (b)
  {
    flen--;
    fifo[flen] = fifo[flen - 1];
    b = (flen > 0);
  }
  fifo[0] = nChar;
}
//=========================================================================================
byte com_delay(unsigned long timeDelay)
{
  static unsigned long lastTime;
  unsigned long currTime;
  byte ret;

  currTime = millis();
  ret = ((currTime - lastTime) >= timeDelay);
  if(ret)
  {
    lastTime = currTime;
  }
  return ret;
}
//=========================================================================================
void com_monitor(void)
{
  //rx-----------------------------------------------
  static rxStates_t rxState = WAITRX;//stav prijmu
  static int rxLength; //pocet prijatych byte (tachometr)
  static int rxch = 0;//index nacteneho znaku
  static char rxFifo[COM_RX_LEN];//Fifo pro prijem
  //tx-----------------------------------------------
  static txStates_t txState = WAITTX;//stav vysilani
  static txStates_t reqState = WAITTX;//pozadovany stav vysilani dle prijmu
  static txStates_t ackState = WAITTX;//stav pro vysilani po obdrzeni znaku ">"
  static byte client; //klient
  static unsigned long txch = 0; //index aktualne vysilaneho znaku
  static unsigned long txpbl; //spodni hranice vysilaneho packetu v poctu byte
  static unsigned long txpbh; //horni hranice vysilaneho packetu v poctu byte
  static unsigned long txlen; //celkova delka zpravy
  //servis-------------------------------------------
  static srStates_t srState = WAITSR;//stav servisni linky
  static char srFifo[COM_SR_LEN];//servisni fifo prijmu
  static char srMsg[COM_MSG_LEN];//buffer pro servisni zpravu
  static char srIdxMsg; //index bufferu pro servisni zpravu
  //obecne-------------------------------------------
  char dataByte; //nacteny nebo vysilany znak
  int idx; //index pro cyklus for
  File thisFile;

  //RX------------------------------------------------------
  //nacteni byte
  while (esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    com_putCharInFifo(dataByte, rxFifo, COM_RX_LEN);
    rxch++; //pocet prectenych znaku zvysit o 1
    wrLog(dataByte, (srState == LOGSR));
    switch (rxState)
    {
      case WAITRX:
        if(com_findInFifo(String(F("+IPD,")), rxFifo, COM_RX_LEN))
        {
          PRTDBG("\nPrijem");
          rxState = CLIENTRX;//zaznamenan prijem
        }
        if(txState == ACKTX)
        {
          if(dataByte == '>')
          {
            txState = ackState;//vysilani je povoleno
          }
          else if(com_findInFifo(String(F("ERROR")), rxFifo, COM_RX_LEN))
          {
            txState = WAITTX;//reset vysilani
          }
        }
        else if(txState == ACKSNDTX)
        {
          if(com_findInFifo(String(F("SEND OK")), rxFifo, COM_RX_LEN))
          {
            txState = ackState;//odeslani je potvrzeno       
          }
          else if(com_findInFifo(String(F("ERROR")), rxFifo, COM_RX_LEN))
          {
            txState = WAITTX;//reset vysilani
          }
        }
        break;

      case CLIENTRX://identifikace klienta
        client = dataByte - 48;
        rxState = COMMARX;
        break;

      case COMMARX://cteni carky
        rxLength = 0; //vynulovani poctu prichozich byte
        rxState = BYTECNTRX;
        break;

      case BYTECNTRX://pocet prichozich byte
        if(dataByte == ':')
        {
          rxch = 0; //vynulovani poctu nactenych znaku
          if(reqState != WAITTX) //kdyz uz probiha zpracovani html
          {
            rxState = FLUSHRX;
          }
          else //kdyz je volno ke zpracovani
          {
            rxState = REQRX;
            PRTDBG("\ncekam na GET");
          }
        }
        else
        {
          dataByte -= 48;
          rxLength = 10 * rxLength + dataByte;
        }
        break;

      case REQRX://cekani na GET
        if(com_findInFifo(String(F("GET ")), rxFifo, COM_RX_LEN))
        {
          rxState = URLRX;
        }
        else if(rxch > 5) //nenalezeno GET
        {
          rxState = FLUSHRX;
        }
        break;

      case URLRX://cteni jmena stranky
        if(dataByte == ' ')
        {
          //identifikace stranky
          if(com_findInFifo(String(F("/favicon.ico ")), rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nFavicon");
            reqState = REQHEADTX;
          }
          else if(com_findInFifo(String(F("/ ")), rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nMainpage");
            reqState = REQHEADTX;
          }
          else if(com_findInFifo(String(F("/wtf?do=cervena ")), rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nTlacitko zapni");
            reqState = ONBUTTX;
          }
          else if(com_findInFifo(String(F("/wtf?do=zluta ")), rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nTlacitko vypni");
            reqState = OFFBUTTX;
          }
          else
          {
            PRTDBG("\nStranka neidentifikovana");
            reqState = WAITTX;
          }
          rxState = FLUSHRX;
        }
        break;

      case FLUSHRX://vycteni zbytku bufferu
        if(rxch >= rxLength)
        {
          rxState = WAITRX;
          PRTDBG("\nBuffer uvolnen");
        }
        break;

      default:
        rxState = WAITRX;
        break;
    }
  }

  //TX------------------------------------------------------
  switch (txState)
  {
    case WAITTX: //idle
      if(rxState == WAITRX) //RX musi byt v klidu
      {
        com_delay(0);//znovunastaveni casu
        txState = reqState;
        if(srState == ATSENDSR) //odeslani servisni zpravy
        {
          esp8266Ser.println(srMsg);
          srState = LOGSR;
        }
      }
      else if(com_delay(COM_WATCHDOG_TIME)) //WATCHDOG FOR RX
      {
        rxState = WAITRX;
      }
      break;

    case REQHEADTX://pozadovano vysilani hlavni stranky
      thisFile = SD.open(SD_FILE_HEAD, FILE_READ);
      txlen = thisFile.size();
      thisFile.close();
      txpbh = 0;
      txch = 0; //vysilani prvniho znaku paketu
      reqState = HEADTX;
      txState = NEXTPACKTTX;
      break;

    case REQMNPGTX:
      thisFile = SD.open(SD_FILE_MNPG, FILE_READ);
      txlen = thisFile.size();
      thisFile.close();
      txpbh = 0;
      txch = 0; //vysilani prvniho znaku
      com_delay(0);//synchronizace casu
      reqState = MAINPAGETX;
      txState = NEXTPACKTTX;
      break;

    case HEADTX: //odesilani hlavicky
      if(com_delay(ESP8266TXPER))
      {
        thisFile = SD.open(SD_FILE_HEAD, FILE_READ);
        thisFile.seek(txch);
        for (idx = 0; idx < ESP8266CHARCT && (txState == HEADTX); idx++, txch++)
        {
          if(txch >= txpbh)
          {
            if(txch >= txlen)
            {
              ackState = REQMNPGTX;
            }
            else
            {
              ackState = NEXTPACKTTX;
            }
            txState = ACKSNDTX;
          }
          else
          {
            dataByte = thisFile.read();
            wrLog(dataByte, (srState == LOGSR));
            esp8266Ser.write(dataByte);
          }
        }
        thisFile.close();
        com_delay(0);//synchronizace casu
      }
      break;

    case MAINPAGETX:
      if(com_delay(ESP8266TXPER))
      {
        thisFile = SD.open(SD_FILE_MNPG, FILE_READ);
        thisFile.seek(txch);
        for (idx = 0; idx < ESP8266CHARCT && (txState == MAINPAGETX); idx++, txch++)
        {
          if(txch >= txpbh)
          {
            if(txch >= txlen)
            {
              reqState = WAITTX;
              ackState = WAITTX;
              PRTDBG("\n\nVse odeslano");
            }
            else
            {
              ackState = NEXTPACKTTX;
            }
            txState = ACKSNDTX;
          }
          else
          {
            dataByte = thisFile.read();
            wrLog(dataByte, (srState == LOGSR));
            esp8266Ser.write(dataByte);
          }
        }
        thisFile.close();
        com_delay(0);//synchronizace casu
      }
      break;

    case ACKSNDTX: //ocekavani prijmu "SEND OK"
      if(com_delay(4000)) //nove odesilani packetu
      {
        txState = LASTPACKTTX;
      }
      break;
      
    case ACKTX: //ocekavani prijmu znaku ">" pro povoleni vysilani
      if(com_delay(4000)) //novy prikaz SEND
      {
        txState = ATSENDTX;
      }
      break;

    case NEXTPACKTTX:
      if(com_delay(50)) //50 ms prodleva
      {
        txpbh += ESP8266PACKETLEN;
        if(txpbh > txlen) txpbh = txlen;
        txpbl = txch;
        ackState = reqState; //po obdrzeni potvrzeni vysilani skocit do vysilani stranky
        txState = ATSENDTX;
        PRTDBG("\nNovy paket");
      }
      break;

    case LASTPACKTTX:
      if(com_delay(50)) //50 ms prodleva
      {
        txch = txpbl;
        ackState = reqState; //po obdrzeni potvrzeni vysilani skocit do vysilani stranky
        txState = ATSENDTX;
        PRTDBG("\nNove odesilani stareho paketu");
      }
      break;

    case ATSENDTX:
      if(rxState == WAITRX)
      {
        esp8266Ser.print(F("AT+CIPSEND="));
        esp8266Ser.print(client);
        esp8266Ser.print(F(","));
        esp8266Ser.println(txpbh - txpbl);
        txState = ACKTX;
        com_delay(0); //znovunastaveni casu
      }
      break;

    case ONBUTTX:
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_YEL, LOW);
      analogWrite(PWM_W, 80);
      analogWrite(PWM_R, 0);
      analogWrite(PWM_G, 160);
      reqState = REQHEADTX;
      txState = REQHEADTX;
      break;

    case OFFBUTTX:
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YEL, HIGH);
      analogWrite(PWM_W, 255);
      analogWrite(PWM_R, 255);
      analogWrite(PWM_G, 255);
      reqState = REQHEADTX;
      txState = REQHEADTX;
      break;

    default:
      reqState = WAITTX;
      txState = WAITTX;
      ackState = WAITTX;
      break;
  }
  //SERVIS------------------------------------------------------
  while (debugSer.available())
  {
    dataByte = debugSer.read();
    com_putCharInFifo(dataByte, srFifo, COM_SR_LEN);

    switch (srState)
    {
      case LOGSR: //vypisuje log az do prijeti SR+END
        if(com_findInFifo(String(F("SR+END")), srFifo, COM_SR_LEN))
        {
          wrMsg("\nSR+END\n\nOK\n");
          srState = WAITSR;
          break;
        }
      case WAITSR:
        if(com_findInFifo(String(F("AT")), srFifo, COM_SR_LEN))
        {
          srMsg[0] = 'A';
          srMsg[1] = 'T';
          srIdxMsg = 1;
          srState = ATCOMMSR;
        }
        break;

      case ATCOMMSR:
        srIdxMsg++;
        srIdxMsg &= COM_MSG_MSK;
        if(dataByte == '\n')
        {
          srMsg[srIdxMsg] = '\0';
          srState = ATSENDSR;
        }
        else
        {
          srMsg[srIdxMsg] = dataByte;
        }
        break;

      case ATSENDSR: //ceka na odeslani servisni zpravy
        break;

      default:
        srState = WAITSR;
        break;
    }
  }
}

//=========================================================================================
//Pristup k souborum
//=========================================================================================
byte initSDCard()
{
  File thisFile;
  byte ret;
  unsigned long mainPgSize;
  String fileName;
  
  ret = SD.begin();
  if(ret)
  {
    wrMsg("\n<ok> Nalezena SD karta");
    wrMsg("\n\nHledani hlavickoveho souboru: html/head.txt");
    if(SD.exists(SD_FILE_HEAD))
    {
      wrMsg("\n<ok> Hlavickovy soubor nalezen, nebude generovan novy");
    }
    else
    {
      wrMsg("\n<X> Hlavickovy soubor nenalezen, generuje se novy");
      wrMsg("\n\nHledani souboru html/mainpage.htm");
      thisFile = SD.open(SD_FILE_MNPG, FILE_READ);
      if(thisFile)
      {
        wrMsg("\n<ok> Soubor mainpage.htm nalezen");
        mainPgSize = thisFile.size();
        thisFile.close();

        thisFile = SD.open(SD_FILE_HEAD, FILE_WRITE);
        thisFile.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "));
        thisFile.print(mainPgSize);
        thisFile.print(F("\r\n\r\n"));
        thisFile.close();
      }
      else
      {
        wrMsg("\n<X> Soubor hlavni stranky nebyl nalezen");
        ret = 0;
      }
    }
    fileName = String(SD_FILE_SSET);
    if(SD.exists(fileName))
    {
      readDLFile(&sunset, fileName);
    }
    else
    {
      sunset.p.hour = 18;
      sunset.p.minute = 0;
      sunset.p.ramp = 10;
      writeDLFile(&sunset, fileName);
    }
    fileName = String(SD_FILE_SRIS);
    if(SD.exists(fileName))
    {
      readDLFile(&sunrise, fileName);
    }
    else
    {
      sunrise.p.hour = 6;
      sunrise.p.minute = 0;
      sunrise.p.ramp = 10;
      writeDLFile(&sunrise, fileName);
    }
    fileName = String(SD_FILE_MSET);
    if(SD.exists(fileName))
    {
      readDLFile(&moonset, fileName);
    }
    else
    {
      moonset.p.hour = 4;
      moonset.p.minute = 0;
      moonset.p.ramp = 10;
      writeDLFile(&moonset, fileName);
    }
    fileName = String(SD_FILE_MRIS);
    if(SD.exists(fileName))
    {
      readDLFile(&moonrise, fileName);
    }
    else
    {
      moonrise.p.hour = 20;
      moonrise.p.minute = 0;
      moonrise.p.ramp = 10;
      writeDLFile(&moonrise, fileName);
    }
  }
  else
  {
    wrMsg("\n<X> SD karta neni k dispozici");
  }
  return ret;
}
//=========================================================================================
void readDLFile(deadline_t * dl, String fileName)
{
  File thisFile;
  byte idx;
  byte part;
  union{
    char arr[3];
    struct{
      char tens;
      char ones;
      char ch;
    }p;
  }var;
  thisFile = SD.open(fileName, FILE_READ);

  for(part = 0; part < 3; part++)
  {
    var.arr[0] = '0';
    var.arr[1] = '0';
    var.arr[2] = '0';
    for(idx = 0; idx < 2 && thisFile.available() && var.p.ch != ','; idx++)
    {
      var.p.ch = thisFile.read();
      if(var.p.ch >= '0' && var.p.ch <= '9')
      {
        var.arr[idx] = var.p.ch;
      }
    }
    dl->mem[part] = asciiToDec(var.p.tens, var.p.ones);
  }
  thisFile.close();
}
//=========================================================================================
void writeDLFile(deadline_t * dl, String fileName)
{
  File thisFile;
  byte part;

  SD.remove(fileName);
  thisFile = SD.open(fileName, FILE_WRITE);

  for(part = 0; part < 3; part++)
  {
    thisFile.print(dl->mem[part]);
    thisFile.print(",");
  }
  thisFile.close();
}
//=========================================================================================
//Rizeni HW
//=========================================================================================
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
//=========================================================================================
void readDS3231time(time_t *clk){
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); // set DS3231 register pointer to 00h
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
    // request seven bytes of data from DS3231 starting from register 00h
    clk->second = bcdToDec(Wire.read() & 0x7f);
    clk->minute = bcdToDec(Wire.read());
    clk->hour = bcdToDec(Wire.read() & 0x3f);
    clk->dayOfWeek = bcdToDec(Wire.read());
    clk->dayOfMonth = bcdToDec(Wire.read());
    clk->month = bcdToDec(Wire.read());
    clk->year = bcdToDec(Wire.read());
}
//=========================================================================================
byte setDS3231time(time_t *clk){
  byte ret;
  ret = clk->second < 60 && clk->minute < 60 && clk->hour < 60&& 
       clk->dayOfWeek < 8 && clk->dayOfWeek > 0 && 
       clk->dayOfMonth < 32 && clk->dayOfMonth > 0 && 
       clk->month < 13 && clk->month > 0&& clk->year < 100;
  //sets time and date data to DS3231
  if(ret)
  {
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); // set next input to start at the seconds register
    Wire.write(decToBcd(clk->second)); // set seconds
    Wire.write(decToBcd(clk->minute)); // set minutes
    Wire.write(decToBcd(clk->hour)); // set hours
    Wire.write(decToBcd(clk->dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
    Wire.write(decToBcd(clk->dayOfMonth)); // set date (1 to 31)
    Wire.write(decToBcd(clk->month)); // set month
    Wire.write(decToBcd(clk->year)); // set year (0 to 99)
    Wire.endTransmission();
  }
  return ret;  
}
//=========================================================================================
void compareTime(){
  ledState_t state;
  time_t clk;
  byte deltaHourA, deltaHourB, deltaMinuteA, deltaMinuteB;
  
  readDS3231time(&clk);
  if(ledState == IDLE_STATE || ledState == SUNSET_IDLE || ledState == SUNRISE_IDLE || 
     ledState == MOONSET_IDLE || ledState == MOONRISE_IDLE)
  {
//////////////////////////////////SUNSET//////////////////////////////////
    state = SUNSET_INIT;

    deltaHourA = 0;
    if(clk.hour < sunset.p.hour) deltaHourA += 24;
    deltaHourA += clk.hour - sunset.p.hour;

    deltaMinuteA = 0;
    if(clk.minute < sunset.p.minute) deltaMinuteA += 60;
    deltaHourA += clk.minute - sunset.p.minute;
    
//////////////////////////////////SUNRISE//////////////////////////////////
    deltaHourB = 0;
    if(clk.hour < sunrise.p.hour) deltaHourB += 24;
    deltaHourB += clk.hour - sunrise.p.hour;

    deltaMinuteB = 0;
    if(clk.minute < sunrise.p.minute) deltaMinuteB += 60;
    deltaHourB += clk.minute - sunrise.p.minute;

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
//////////////////////////////////MOONSET//////////////////////////////////
    deltaHourB = 0;
    if(clk.hour < moonset.p.hour) deltaHourB += 24;
    deltaHourB += clk.hour - moonset.p.hour;

    deltaMinuteB = 0;
    if(clk.minute < moonset.p.minute) deltaMinuteB += 60;
    deltaHourB += clk.minute - moonset.p.minute;

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
//////////////////////////////////MOONRISE//////////////////////////////////
    deltaHourB = 0;
    if(clk.hour < moonrise.p.hour) deltaHourB += 24;
    deltaHourB += clk.hour - moonrise.p.hour;

    deltaMinuteB = 0;
    if(clk.minute < moonrise.p.minute) deltaMinuteB += 60;
    deltaHourB += clk.minute - moonrise.p.minute;

    if(deltaHourA > deltaHourB){
      state = MOONRISE_INIT;
    }
    else if(deltaHourA == deltaHourB){
      if(deltaMinuteA > deltaMinuteB){
        state = MOONRISE_INIT;
      }
    }
//////////////////////////////////////////////////////////////////////
    if (state == SUNSET_INIT && ledState != SUNSET_IDLE){
      ledState = SUNSET_INIT;
    }
    if (state == SUNRISE_INIT && ledState != SUNRISE_IDLE){
      ledState = SUNRISE_INIT;
    }
    if (state == MOONSET_INIT && ledState != MOONSET_IDLE){
      ledState = MOONSET_INIT;
    }
    if (state == MOONRISE_INIT && ledState != MOONRISE_IDLE){
      ledState = MOONRISE_INIT;
    }
  }
}
//=========================================================================================
void ledControl(){
  static int i = 0;
  static unsigned long timer = 0;
/*------------------SUNSET_INITIALIZING------------------*/  
  switch(ledState){
    
    case SUNSET_INIT:
      i = 0;
      timer = millis();
      ledState = SUNSET_SOON;
      analogWrite(PWM_B, 0);                     //0
      break;
    case SUNSET_SOON:
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/8)*1)/255)){
        timer = millis();
        analogWrite(PWM_R, i);                     //0-255
        analogWrite(PWM_G, (i*0.19));              //0-50
        if(i < 255){
          i++;
        }
        else{
          ledState = SUNSET_MIDDLE;
          i = 255;
          analogWrite(PWM_G, 50);                   //50
          analogWrite(PWM_B, 0);                    //0
        }
      }
      break;
    case SUNSET_MIDDLE: //i musi byt 255
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/8)*5)/35)){
        timer = millis();
        analogWrite(PWM_R, i);                    //255-220
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
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/8)*2)/255)){
        timer = millis();
        analogWrite(PWM_R, 220+(i*35/255));       //220-255
        analogWrite(PWM_G, 50+(i*205/255));       //50-255
        analogWrite(PWM_B, i);                    //0-255
        analogWrite(PWM_W, i);                    //0-255
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
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/8)*2)/255)){
        timer = millis();
        analogWrite(PWM_R, 220+(i*7/51));         //255-220
        analogWrite(PWM_G, 50+(i*41/51));         //255-50
        analogWrite(PWM_B, i);                    //255-0
        analogWrite(PWM_W, i);                    //0-255
        if(i > 0){
          i--;
        }
        else{
          ledState = SUNRISE_MIDDLE;
          i = 220;
          analogWrite(PWM_G, 50);                   //50
          analogWrite(PWM_B, 0);                    //0
        }
      }
      break;
    case SUNRISE_MIDDLE: //i musi byt 255
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/5)*2)/35)){
        timer = millis();
        analogWrite(PWM_R, i);                    //220-255
        if(i < 255){
          i++;
        }
        else{
          ledState = SUNRISE_LATE;
          i = 255;
          analogWrite(PWM_B, 0);                    //0
        }
      }
      break;
    case SUNRISE_LATE:
      if((millis() - timer) >= ((((sunset.p.ramp*60000)/8)*1)/255)){
        timer = millis();
        analogWrite(PWM_R, i);                    //255-0
        analogWrite(PWM_G, (i*10/51));            //50-0
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
      analogWrite(PWM_R, 0);                    //0
      break;
    case MOONSET:
      if((millis() - timer) >= ((moonset.p.ramp*60000)/60)){
        timer = millis();
        analogWrite(PWM_G, i/2);                //0-30
        analogWrite(PWM_B, i);                  //0-60
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
      analogWrite(PWM_R, 0);                    //0
      break;
    case MOONRISE:
      if((millis() - timer) >= ((moonrise.p.ramp*60000)/60)){
        timer = millis();
        analogWrite(PWM_G, i/2);                //30-0
        analogWrite(PWM_B, i);                  //60-0
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

