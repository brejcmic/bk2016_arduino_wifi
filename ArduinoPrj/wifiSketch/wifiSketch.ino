#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(8, 7); // RX, TX

#define LED_RED             4
#define LED_YEL             2
#define PWM_1               3
#define PWM_2               5
#define PWM_3               6
#define PWM_4               9

#define esp8266Ser  softSerial
#define ESP8266SPEED        9600// cilova rzchlost kolmunikace
#define ESP8266CHARCT       20// pocet najednou odesilanych znaku
#define ESP8266TXPER        10// perioda vysilani v ms
#define ESP8266PACKETLEN    1024 //maximalni velikost packetu v byte
#define COM_WATCHDOG_TIME   3000// cas v ms, po kterem se resetuje stav RX
#define COM_RX_LEN          32 //delka fifa pro prijem z esp8266
#define COM_SR_LEN          8 //delka fifa pro prijem ze servisni linky
#define COM_MSG_LEN         64 //delka kruhoveho bufferu pro ulozeni servisni zpravy, mocnina 2
#define COM_MSG_MSK         COM_MSG_LEN-1 //maska pro pretekani indexu bufferu servisni zpravy

#define DEBUG       0
#define debugSer    Serial

#if DEBUG == 1
#define PRTDBG(x)   debugSer.println(F(x)) //jen pro hlaseni
#define WRTDBG(x)   debugSer.write(x) //bez konverze
#define WRCDBG(x)   debugSer.print(x) //s konverzi
#define wrLog(x, b) debugSer.write(x) //trvaly vypis logu
#define wrMsg(x)    debugSer.println(F(x)) //vypis zpravy
#else
#define PRTDBG(x)
#define WRTDBG(x)
#define WRCDBG(x)
#define wrLog(x, b) if(b) debugSer.write(x) //podmineny vypis logu
#define wrMsg(x)    debugSer.println(F(x)) //vypis zpravy
#endif

const char headhttp[] = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "};//nechat v ramce
const char mainpage[] PROGMEM = {"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title>Aquaduino</title><link href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=\" rel=\"icon\" type=\"image/x-icon\" /></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zluta\"><input type=\"submit\" name=\"do\" value=\"cervena\"></form> </body></html>"};

typedef enum {
  WAITTX, REQSNDPGTX, ACKSNDTX, ACKTX, HEADTX, MAINPAGETX, FAVICONTX, NEXTPACKTTX, ATSENDTX, ONBUTTX, OFFBUTTX
} txStates_t;

typedef enum {
  WAITRX, CLIENTRX, COMMARX, BYTECNTRX, REQRX, URLRX, FLUSHRX, SERVISMODERX
} rxStates_t;

typedef enum {
  LOGSR, WAITSR, ATCOMMSR, ATSENDSR
} srStates_t;

typedef struct
{
  unsigned int Byte0;
  unsigned int Byte1;
  unsigned int Byte2;
  unsigned int Byte3;
  unsigned int Byte4;
  unsigned int Byte5;
} mac_t;
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
}
//=========================================================================================
void loop() {
  com_monitor();
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
  unsigned int ret;
  char rx[4];
  String atMsg;
  int idx;
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
      wrMsg("\nRESET esp8266.");
      for (idx = 0; idx < 5; idx++)
      {
        wrLog('*', 1);
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
    wrMsg("\nInicializace komunikace s esp8266.");
    for (idx = 0; idx < 5; idx++)
    {
      wrLog('*', 1);
      delay(1000);
    }
    esp8266Ser.begin(ESP8266SPEED);
    //Kontrola primu
    //----------------------------------------------------
    wrMsg("\nPokus o spojeni");
    esp8266Ser.println(F("AT"));
    com_delay(0); //reset casu
    idx = 0;
    while (!com_checkRxESP8266For("OK", rx, 4) && (ret & 1))
    {
      if(com_delay(1000))
      {
        idx++;
        wrLog('*', 1);
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
      wrMsg("\nPripojovani k WIFI siti");
      atMsg = String("AT+CWJAP_CUR=\"");
      atMsg += String("brejcmicDebug");
      atMsg += String("\",\"");
      atMsg += String("testwifi");
      atMsg += String("\"");
      esp8266Ser.println(atMsg);//pokus o pripojeni k wifi
      com_delay(0); //reset casu
      idx = 0;
      while (!com_checkRxESP8266For("OK", rx, 4) && (ret & 1))
      {
        if(com_delay(1000))
        {
          idx++;
          wrLog('*', 1);
        }
        else if(idx > 15)
        {
          wrMsg("\n<X> WIFI sit nedostupna.");
          ret--;//shozeni nejnizsiho bitu
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
unsigned int com_checkRxESP8266For(const char* cArr, char *fifo, unsigned int flen)
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
unsigned int com_findInFifo(const char* cArr, const char *fifo, unsigned int flen)
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
  unsigned int b;

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
unsigned int com_delay(unsigned long timeDelay)
{
  static unsigned long lastTime;
  unsigned long currTime;
  unsigned int ret;

  currTime = millis();
  ret = ((currTime - lastTime) >= timeDelay);
  if(ret)
  {
    lastTime = currTime;
  }
  return ret;
}
//=========================================================================================
unsigned int com_putCharInCyrcBuff(char nChar, char *cArr, unsigned int msk)
{
  static unsigned int idx = 0;
  idx++;
  idx &= msk;
  cArr[idx] = nChar;
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
  static int client; //klient
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
  String page; //retezec pro vysilani
  int idx; //index pro cyklus for

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
            reqState = REQSNDPGTX;
          }
          else if(com_findInFifo("/ ", rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nMainpage");
            reqState = REQSNDPGTX;
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

    case REQSNDPGTX://pozadovano vzsilani hlavni stranky
      page = String(headhttp);
      page += String((sizeof(mainpage) - 1));
      page += String("\r\n\r\n");
      txlen = page.length();
      txpb = txlen; //delka paketu hlavicky zde neni omezena
      txch = 0; //vysilani prvniho znaku paketu
      ackState = HEADTX; //po obdrzeni potvrzeni vysilani skocit do vysilani hlavicky
      txState = ATSENDTX;
      break;

    case ACKSNDTX:
    case ACKTX: //ocekavani prijmu znaku ">" pro povoleni vysilani
      if(rxState != WAITRX) //chyba, modul prijima jina data a prikaz je treba zrusit
      {
        com_delay(0);//synchronizace casu
        txState = WAITTX;
      }
      else if(com_delay(2000)) //novy prikaz SEND
      {
        txState = ATSENDTX;
      }
      break;

    case HEADTX: //odesilani hlavicky
      page = String(headhttp);
      page += String((sizeof(mainpage) - 1));
      page += String("\r\n\r\n");
      esp8266Ser.print(page);
      //uprava delek o velikost odeslane hlavicky
      txlen = sizeof(mainpage) - 1;
      txpb = 0;
      txch = 0; //vysilani prvniho znaku
      com_delay(0);//synchronizace casu
      reqState = MAINPAGETX;
      ackState = NEXTPACKTTX;
      txState = ACKSNDTX;
      WRCDBG(page);
      break;

    case MAINPAGETX:
      if(com_delay(ESP8266TXPER))
      {
        for (idx = 0; idx < ESP8266CHARCT; idx++, txch++)
        {
          if(txch >= txpb)
          {
            com_delay(0);//synchronizace casu
            reqState = MAINPAGETX;
            ackState = NEXTPACKTTX;
            txState = ACKSNDTX;
            break;
          }
          else
          {
            dataByte = pgm_read_byte_near(mainpage + txch);
            wrLog(dataByte, (srState == LOGSR));
            esp8266Ser.write(dataByte);
          }
        }
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
        esp8266Ser.print(',');
        esp8266Ser.println(txpb - txch);
        txState = ACKTX;
        com_delay(0); //znovunastaveni casu
      }
      else
      {
        txState = WAITTX;
      }
      break;

    case ONBUTTX:
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_YEL, LOW);
      analogWrite(PWM_1, 80);
      analogWrite(PWM_2, 0);
      analogWrite(PWM_3, 160);
      txState = REQSNDPGTX;
      break;

    case OFFBUTTX:
      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_YEL, HIGH);
      analogWrite(PWM_1, 255);
      analogWrite(PWM_2, 255);
      analogWrite(PWM_3, 255);
      txState = REQSNDPGTX;
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
          wrMsg("\nSR+END\n\nOK");
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

