#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(8, 7); // RX, TX

#define LED_RED             4
#define LED_YEL             2
#define PWM_1               3
#define PWM_2               5
#define PWM_3               6
#define PWM_4               9

#define VERSION   "Aquaduino verze 160420"

#define DS3231_I2C_ADDRESS 0x68

#define esp8266Ser  softSerial
#define ESP8266SPEED        9600// cilova rzchlost komunikace
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

#define DEBUG       1
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

typedef enum {
  WAITTX, REQHEADTX, REQMNPGTX, ACKSNDTX, ACKTX, HEADTX, MAINPAGETX, FAVICONTX, NEXTPACKTTX, ATSENDTX, ONBUTTX, OFFBUTTX
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
//=========================================================================================
void setup() {
  pinMode(LED_RED, OUTPUT);           // set pin to input
  pinMode(LED_YEL, OUTPUT);           // set pin to input
  pinMode(PWM_1, OUTPUT);
  pinMode(PWM_2, OUTPUT);
  pinMode(PWM_3, OUTPUT);
  pinMode(PWM_4, OUTPUT);
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_YEL, LOW);
  com_setupServisCh();
  com_setupEsp8266();
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_YEL, HIGH);
  //RTC
  Wire.begin();
}
//=========================================================================================
void loop() {
  com_monitor();
}
//=========================================================================================
// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val){
    return( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val){
    return( (val/16*10) + (val%16) );
}
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
unsigned int com_setupServisCh(void)
{
  //navazani komunikace servisni linky
  //----------------------------------------------------
  debugSer.begin(115200, SERIAL_8N1);
  return 1;
}
//=========================================================================================
unsigned int com_setupEsp8266()
{
  byte ret;
  char rx[4];
  String atMsg;
  byte idx;
  File thisFile;
  unsigned long mainPgSize;
  //SD karta
  ret = 2; //pocet pokusu o navazani spojeni
  //Predpoklada se na pocatku uspesna inicializace
  ret = (ret << 1) + 1;
  if(!SD.begin() && ret) 
  {
    wrMsg("\n<X> SD karta neni k dispozici");
    ret = 0;
  }
  else
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
  }
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
    atMsg = String(F("AT+UART_CUR="));
    atMsg += ESP8266SPEED;
    atMsg += String(F(",8,1,0,0"));
    esp8266Ser.println(atMsg);
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
    while (!com_checkRxESP8266For("OK", rx, 4) && (ret & 1))
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
      while (!com_checkRxESP8266For("OK", rx, 4) && (ret & 1))
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
        while(thisFile.available())
        {
          rx[0] = thisFile.read();
          esp8266Ser.print(rx[0]);
          WRCDBG(rx[0]);
        }
        esp8266Ser.print(F("\r\n"));
        thisFile.close();
        com_delay(0); //reset casu
        idx = 0;
        while (!com_checkRxESP8266For("OK", rx, 4) && (ret & 1))
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
      while (!com_checkRxESP8266For("OK", rx, 4)  && (ret & 1))
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
byte com_checkRxESP8266For(char* cArr, char *fifo, unsigned int flen)
{
  char dataByte;

  if(esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    com_putCharInFifo(dataByte, fifo, flen);
    return com_findInFifo(cArr, fifo, flen);
  }
  return 0;
}
//=========================================================================================
//Funkce hleda retezec v poslednich flen prijatych znacich.
byte com_findInFifo(char* cArr, const char *fifo, unsigned int flen)
{
  unsigned int ret; //navratova hodnota
  unsigned int j; //index znaku v retezci

  //zjisteni delky retezce
  j = 0;
  while (cArr[j] != '\0')
  {
    j++;
  }

  ret = (j != 0 && j <= flen);
  flen = 0;
  //hledani retezce ve fifo
  while (j > 0 && ret)
  {
    j--;
    ret = (fifo[flen] == cArr[j]);
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
  static unsigned long txpb; //hranice vysilaneho packetu v poctu byte
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
        if(com_findInFifo("+IPD,", rxFifo, COM_RX_LEN))
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
          else if(com_findInFifo("ERROR,", rxFifo, COM_RX_LEN))
          {
            txState = WAITTX;//reset vysilani
          }
        }
        else if(txState == ACKSNDTX)
        {
          if(com_findInFifo("SEND OK", rxFifo, COM_RX_LEN))
          {
            txState = ackState;//odeslani je potvrzeno       
          }
          else if(com_findInFifo("ERROR,", rxFifo, COM_RX_LEN))
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
        if(com_findInFifo("GET ", rxFifo, COM_RX_LEN))
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
          if(com_findInFifo("/favicon.ico ", rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nFavicon");
            reqState = REQHEADTX;
          }
          else if(com_findInFifo("/ ", rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nMainpage");
            reqState = REQHEADTX;
          }
          else if(com_findInFifo("/wtf?do=cervena ", rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nTlacitko zapni");
            reqState = ONBUTTX;
          }
          else if(com_findInFifo("/wtf?do=zluta ", rxFifo, COM_RX_LEN))
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
      txpb = 0;
      txch = 0; //vysilani prvniho znaku paketu
      reqState = HEADTX;
      txState = NEXTPACKTTX;
      break;

    case REQMNPGTX:
      thisFile = SD.open(SD_FILE_MNPG, FILE_READ);
      txlen = thisFile.size();
      thisFile.close();
      txpb = 0;
      txch = 0; //vysilani prvniho znaku
      com_delay(0);//synchronizace casu
      reqState = MAINPAGETX;
      txState = NEXTPACKTTX;
      break;

    case ACKSNDTX:
    case ACKTX: //ocekavani prijmu znaku ">" pro povoleni vysilani
      if(com_delay(4000)) //novy prikaz SEND
      {
        txState = ATSENDTX;
      }
      break;

    case HEADTX: //odesilani hlavicky
      thisFile = SD.open(SD_FILE_HEAD, FILE_READ);
      while(thisFile.available())
      {
        dataByte = thisFile.read();
        wrLog(dataByte, (srState == LOGSR));
        esp8266Ser.write(dataByte);
      }
      thisFile.close();
      ackState = REQMNPGTX;
      txState = ACKSNDTX;
      break;

    case MAINPAGETX:
      if(com_delay(ESP8266TXPER))
      {
        thisFile = SD.open(SD_FILE_MNPG, FILE_READ);
        thisFile.seek(txch);
        for (idx = 0; idx < ESP8266CHARCT && (txState == MAINPAGETX); idx++, txch++)
        {
          if(txch >= txpb)
          {
            com_delay(0);//synchronizace casu
            reqState = MAINPAGETX;
            ackState = NEXTPACKTTX;
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
      }
      break;

    case NEXTPACKTTX:
      if(txch >= txlen)
      {
        com_delay(0);//synchronizace casu
        reqState = WAITTX;
        txState = WAITTX;
        PRTDBG("\n\nVse odeslano");
      }
      else if(com_delay(50)) //50 ms prodleva
      {
        txpb += ESP8266PACKETLEN;
        if(txpb > txlen) txpb = txlen;
        ackState = reqState; //po obdrzeni potvrzeni vysilani skocit do vysilani stranky
        txState = ATSENDTX;
        PRTDBG("\nNovy packet");
      }
      break;

    case ATSENDTX:
      if(rxState == WAITRX)
      {
        esp8266Ser.print(F("AT+CIPSEND="));
        esp8266Ser.print(client);
        esp8266Ser.print(F(","));
        esp8266Ser.println(txpb - txch);
        txState = ACKTX;
        com_delay(0); //znovunastaveni casu
      }
      break;

    case ONBUTTX:
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_YEL, LOW);
      analogWrite(PWM_1, 80);
      analogWrite(PWM_2, 0);
      analogWrite(PWM_3, 160);
      reqState = REQHEADTX;
      txState = REQHEADTX;
      break;

    case OFFBUTTX:
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YEL, HIGH);
      analogWrite(PWM_1, 255);
      analogWrite(PWM_2, 255);
      analogWrite(PWM_3, 255);
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
        if(com_findInFifo("SR+END", srFifo, COM_SR_LEN))
        {
          wrMsg("\nSR+END\n\nOK\n");
          srState = WAITSR;
          break;
        }
      case WAITSR:
        if(com_findInFifo("AT", srFifo, COM_SR_LEN))
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

