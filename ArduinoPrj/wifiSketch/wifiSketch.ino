#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define esp8266Ser  softSerial
#define ESP8266SPEED  9600// cilova rzchlost kolmunikace
#define ESP8266CHARCT 20// pocet najednou odesilanych znaku
#define ESP8266TXPER  10// perioda vysilani v ms
#define COM_WATCHDOG_TIME   3000// cas v ms, po kterem se resetuje stav RX
#define COM_URL_LENGTH 80 //maximalni delka retezce URL

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

const char headhttp[] = {"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "};//nechat v ramce
const char mainpage[] PROGMEM = {"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title>Aquaduino</title><link href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=\" rel=\"icon\" type=\"image/x-icon\" /></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>"};

typedef enum{
  WAITTX, REQSNDPGTX, ACKPGTX, HEADTX, MAINPAGETX, FAVICONTX, ONBUTTX, OFFBUTTX
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
  com_monitor();
}

//Funkce hleda retezec v poslednich 8 prijatych znacich
//Pokud nema byt pridan znak do fifa, je treba predat znak "\0"
int com_findString(const char* cArr, char recChar)
{
  static char fifo[8]; //fifo poslednich 8 znaku
  static unsigned int idxf = 0; //index fifa
  unsigned int j; //index znaku v retezci
  unsigned int i; //index index znaku ve fifo

  //pridani znaku do fifa
  if(recChar != '\0')
  {
    fifo[idxf] = recChar;
    idxf++;
    idxf &= 0x07;
  }
  //zjisteni delky retezce
  j = 0;
  while(cArr[j] != '\0') j++;
  if(j > 8 || j == 0) return 0;

  //hledani retezce ve fifo
  i = idxf;
  while(j > 0 && (fifo[i] == cArr[--j]))
  {
    i--;
    i &= 0x07;
  }
  if(j == 0) return 1;
  else return 0;
}

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

void com_monitor(void)
{
  //rx-----------------------------------------------
  static rxStates_t rxState = WAITRX;//stav prijmu
  static int rxLength; //pocet prijatych byte (tachometr)
  static int rxch = 0;//index nacteneho znaku
  static char pageUrl[COM_URL_LENGTH];//jmeno pozadovane stranky v GET
  static int urlIdx = 0;//index zapisu do pole
  //tx-----------------------------------------------
  static txStates_t txState = WAITTX;//stav vysilani
  static txStates_t reqState = WAITTX;//pozadovany stav vysilani dle prijmu
  static txStates_t ackState = WAITTX;//stav pro vysilani po obdrzeni znaku ">"
  static int client; //klient
  static int txch = 0; //index aktualne vysilaneho znaku
  //obecne-------------------------------------------
  char dataByte; //nacteny nebo vysilany znak
  String page; //prijaty retezec retezec pro vysilani
  int idx; //index pro cyklus for

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
        if(com_findString("+IPD,", dataByte))
        {
          rxState = CLIENTRX;//zaznamenan prijem
        }
        if(dataByte == '>' && txState == ACKPGTX)
        {
            txState = ackState;//vysilani je povoleno
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
          }
        }
        else
        {
          dataByte -= 48;
          rxLength = 10 * rxLength + dataByte;
        }
        break;
        
      case REQRX://cekani na GET
        if(com_findString("GET ", dataByte))
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
            reqState = REQSNDPGTX;
          }
          else if(page == String("/"))
          {
            reqState = REQSNDPGTX;
          }
          else if(page == String("/wtf?do=rozsvit"))
          {
            reqState = ONBUTTX;
          }
          else if(page == String("/wtf?do=zhasni")) 
          {
            reqState = OFFBUTTX;
          }
          else 
          {
            reqState = WAITTX;
          }
          
          rxState = FLUSHRX;
        }
        else
        {
          pageUrl[urlIdx] = dataByte;
          if(urlIdx < (COM_URL_LENGTH-1)) urlIdx++;
          else rxState = FLUSHRX; //stranku nelze identifikovat
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
      if(rxState == WAITRX)//RX musi byt v klidu
      {
        com_delay(0);//znovunastaveni casu
        txState = reqState;
      }
      else if(com_delay(COM_WATCHDOG_TIME))//WATCHDOG FOR RX
      {
        rxState = WAITRX;
      }
      break;
      
    case REQSNDPGTX://pozadovano vzsilani hlavni stranky
      page = String(headhttp);
      page += String((sizeof(mainpage)-1));
      page += String("\r\n\r\n");
      esp8266Ser.print(F("AT+CIPSEND="));
      esp8266Ser.print(client);
      esp8266Ser.print(',');
      esp8266Ser.println((sizeof(mainpage)-1) + page.length());
      ackState = HEADTX; //po obdrzeni potvrzeni vysilani skocit do vysilani hlavicky
      txState = ACKPGTX;
      break;
      
    case ACKPGTX: //ocekavani prijmu znaku ">" pro povoleni vysilani
      if(rxState != WAITRX)//chyba, modul prijima jina data a prikaz je treba zrusit
      {
        esp8266Ser.println(F("+++"));//Tato sekvence rusi odesilani
        txState = WAITTX;
      }
      break;
      
    case HEADTX: //odesilani hlavicky
      page = String(headhttp);
      page += String((sizeof(mainpage)-1));
      page += String("\r\n\r\n");
      esp8266Ser.print(page);
      txch = 0; //vysilani prvniho znaku
      com_delay(0);//synchronizace casu
      txState = MAINPAGETX;
      WRCDBG(page);
      break;
      
    case MAINPAGETX:
      if(com_delay(ESP8266TXPER))
      {
        dataByte = pgm_read_byte_near(mainpage + txch);
        for(idx = 0; idx < ESP8266CHARCT && dataByte != '\0'; idx++)
        {
          WRTDBG(dataByte);
          esp8266Ser.write(dataByte);
          txch++;
          dataByte = pgm_read_byte_near(mainpage + txch);
        }
        if(dataByte == '\0') 
        {
          reqState = WAITTX;
          txState = WAITTX;
          com_delay(0);//synchronizace casu
          PRTDBG("\n\nVse odeslano");
        }
      }
      break;
      
    case ONBUTTX:
      digitalWrite(7, HIGH);
      txState = REQSNDPGTX;
      break;
      
    case OFFBUTTX:
      digitalWrite(7, LOW);
      txState = REQSNDPGTX;
      break;
      
    default:
      reqState = WAITTX;
      txState = WAITTX;
      ackState = WAITTX;
      break;
  }
}

