#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define esp8266Ser  softSerial
#define ESP8266SPEED  9600// cilova rzchlost kolmunikace
#define ESP8266CHARCT 20// pocet najednou odesilanych znaku
#define ESP8266TXPER  10// perioda vysilani v ms

#define ESPTCK(x,y)     ((x-y) >= ESP8266TXPER)
#define ESPTCKSYNC(x,y) (y = x)

#define DEBUG       1
#define debugSer    Serial
#if DEBUG == 1
#define PRTDBG(x)   debugSer.println(F(x)) //jen pro hlaseni
#define WRTDBG(x)   debugSer.write(x) //bez konverze
#define WRCDBG(x)   debugSer.print(x) //s konverzi
#else
#define PRTDBG(x)
#define WRTDBG(x)
#define WRCDBG(x)
#endif

const char mainpage[] PROGMEM = {"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title>Aquaduino</title><link href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=\" rel=\"icon\" type=\"image/x-icon\" /></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>"};

typedef enum{
  WAITTX, ACKTX, HEADTX, MAINPAGETX, FAVICONTX, ONBUTTX, OFFBUTTX
}txStates_t;

typedef enum{
  WAITRX, CLIENTRX, COMMARX, BYTECNTRX, REQRX, URLRX, FLUSHRX
}rxStates_t;

void waitFor(const char* string)
{
  char dataByte;
  int idx;

  debugSer.println("\nPrijato:");
  idx = 0;
  while(string[idx] != '\0') //pri chybe nekonecna smycka
  {
    if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      if(dataByte == string[idx])
      {
        idx++;
      }
      else
      {
        idx = 0;
      }
    }
  }
  debugSer.print("\n\n");
}

void setup() {
  pinMode(7, OUTPUT);           // set pin to input
  digitalWrite(7, HIGH);
  
  debugSer.begin(115200, SERIAL_8N1);
  esp8266Ser.begin(115200);
  debugSer.println(F("AT+UART_CUR=9600,8,1,0,0"));
  esp8266Ser.println(F("AT+UART_CUR=9600,8,1,0,0"));
  delay(2000); //musi byt delay na prenastaveni
  esp8266Ser.begin(ESP8266SPEED);
  debugSer.println(F("AT"));
  esp8266Ser.println(F("AT"));
  waitFor("OK");
  debugSer.println(F("AT+CIPMUX=1"));
  esp8266Ser.println(F("AT+CIPMUX=1"));
  waitFor("OK");
  debugSer.println(F("AT+CWJAP=\"brejcmicDebug\",\"testwifi\""));
  esp8266Ser.println(F("AT+CWJAP=\"brejcmicDebug\",\"testwifi\""));
  waitFor("OK");
  debugSer.println(F("AT+CIPSERVER=1,80"));
  esp8266Ser.println(F("AT+CIPSERVER=1,80"));
  waitFor("OK");
  PRTDBG("Zkouska makra PRTDBG");
  digitalWrite(7, LOW);
}

void loop() {
  htmlMonitor();
}

int findString(const char* string, char recChar)
{
  static int idx = 0;

  if(recChar == string[idx])
  {
    idx++;
    if(string[idx] == '\0') 
    {
      idx = 0;
      return 1;
    }
  }
  else
  {
    idx = 0;
  }
  return 0;
}

void htmlMonitor(void)
{
  //rx-----------------------------------------------
  char dataByte; //nacteny znak
  static rxStates_t rxState = WAITRX;//stav prijmu
  static int rxLength; //pocet prijatych byte
  static int rxch = 0;//index nacteneho znaku
  static char pageUrl[80];//jmeno pozadovane stranky v GET
  static int urlIdx = 0;//index zapisu do pole
  //tx-----------------------------------------------
  static txStates_t txState = WAITTX;//stav vysilani
  static unsigned long lastTime;//cas uplynuly od posledniho vysilani skupiny znaku
  static int client; //klient
  static txStates_t htmlPage = WAITTX;//typ html stranky je stejny jako jeden ze stavu TX
  static int htmlSize;
  static int txch = -3; //index aktualne vysilaneho znaku
  //obecne-------------------------------------------
  String page;
  String headPage;
  int idx;

  //RX------------------------------------------------------
  //nacteni byte
  while(esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    rxch++; //pocet prectenych znaku zvysit o 1
    WRTDBG(dataByte);
    switch(rxState)
    {
      case WAITRX:
        if(findString("+IPD,", dataByte))
        {
          PRTDBG("\nPrijem zpravy detekovan");
          rxState = CLIENTRX;
        }
        if(dataByte == '>' && txState == ACKTX) 
        {
          PRTDBG("\nVysilani povoleno");
          txState = HEADTX;
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
          if(htmlPage != WAITTX) //kdyz uz probiha zpracovani html
          {
            rxState = FLUSHRX;
          }
          else //kdyz je volno ke zpracovani
          {
            rxState = REQRX;
          }
        }
        else
        {
          dataByte -= 48;
          rxLength = 10 * rxLength + dataByte;
        }
        break;
      case REQRX://cekani na GET
        if(findString("GET ", dataByte))
        {
          urlIdx = 0;
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
          pageUrl[urlIdx] = '\0';
          page = String(pageUrl);
          //identifikace stranky
          if(page == String("/favicon.ico")) 
          {
            htmlSize = sizeof(mainpage)-1;
            htmlPage = MAINPAGETX;
          }
          else if(page == String("/"))
          {
            htmlSize = sizeof(mainpage)-1;
            htmlPage = MAINPAGETX;
          }
          else if(page == String("/wtf?do=rozsvit"))
          {
            htmlSize = sizeof(mainpage)-1;
            htmlPage = ONBUTTX;
          }
          else if(page == String("/wtf?do=zhasni")) 
          {
            htmlSize = sizeof(mainpage)-1;
            htmlPage = OFFBUTTX;
          }
          else 
          {
            htmlSize = sizeof(mainpage);
            htmlPage = MAINPAGETX;
          }
          PRTDBG("\nIdentifikace stranky dokoncena");
          PRTDBG("\nPocet vycitanych znaku: ");
          WRCDBG(rxLength - rxch);
          WRCDBG('\n');
          rxState = FLUSHRX;
        }
        else
        {
          pageUrl[urlIdx] = dataByte;
          if(urlIdx < 79) urlIdx++;
        }
        break;
      case FLUSHRX://vycteni zbytku bufferu
        if(rxch >= (rxLength))
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
  switch(txState)
  {
    case WAITTX: //idle
      if(htmlPage != WAITTX && rxState == WAITRX)
      {
        headPage = String("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ");
        headPage += String(htmlSize);
        headPage += String("\r\n\r\n");
        esp8266Ser.print(F("AT+CIPSEND="));
        esp8266Ser.print(client);
        esp8266Ser.print(',');
        esp8266Ser.println(htmlSize + headPage.length());
        PRTDBG("\nPozadavek vysilani");
        txch = -1; //ocekavani odpovedi od ESP8266
        txState = ACKTX;
      }
      break;
    case ACKTX: //ocekavani prijmu znaku pro povoleni vysilani
      break;
    case HEADTX: //odesilani hlavicky
      headPage = String("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ");
      headPage += String(htmlSize);
      headPage += String("\r\n\r\n");
      esp8266Ser.print(headPage);
      WRCDBG(headPage);
      txch = 0; //vysilani prvniho znaku
      txState = htmlPage;
      ESPTCKSYNC(millis(),lastTime);//aktualizace casu
      break;
    case MAINPAGETX:
      if(ESPTCK(millis(),lastTime))
      {
        WRTDBG("\ntxch: ");
        WRCDBG(txch);
        WRTDBG("\n");
        dataByte = pgm_read_byte_near(mainpage + txch);
        for(idx = 0; idx < ESP8266CHARCT && dataByte != '\0'; idx++)
        {
          esp8266Ser.write(dataByte);
          WRTDBG(dataByte);
          txch++;
          dataByte = pgm_read_byte_near(mainpage + txch);
        }
        if(dataByte == '\0') 
        {
          htmlPage = WAITTX;
          txState = WAITTX;
          PRTDBG("\n\nVse odeslano");
        }
      }
      break;
    case ONBUTTX:
      digitalWrite(7, HIGH);
      txState = MAINPAGETX;
      break;
    case OFFBUTTX:
      digitalWrite(7, LOW);
      txState = MAINPAGETX;
      break;
    default:
      htmlPage = WAITTX;
      txState = WAITTX;
      break;
  }
}

