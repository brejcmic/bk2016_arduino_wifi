#include <SoftwareSerial.h>
#include <avr/pgmspace.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define esp8266Ser  softSerial
#define ESP8266SPEED  9600// cilova rzchlost kolmunikace
#define ESP8266CHARCT 20// pocet najednou odesilanych znaku
#define ESP8266TXPER  10// perioda vysilani v ms
#define COM_WATCHDOG_TIME   3000// cas v ms, po kterem se resetuje stav RX
#define COM_RX_LEN    32 //delka fifa pro prijem z esp8266
#define COM_SR_LEN    8 //delka fifa pro prijem ze servisni linky
#define COM_MSG_LEN   64 //delka kruhoveho bufferu pro ulozeni servisni zpravy, mocnina 2
#define COM_MSG_MSK   COM_MSG_LEN-1 //maska pro pretekani indexu bufferu servisni zpravy

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
const char mainpage[] PROGMEM = {"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title>Aquaduino</title><link href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQEAYAAABPYyMiAAAABmJLR0T///////8JWPfcAAAACXBIWXMAAABIAAAASABGyWs+AAAAF0lEQVRIx2NgGAWjYBSMglEwCkbBSAcACBAAAeaR9cIAAAAASUVORK5CYII=\" rel=\"icon\" type=\"image/x-icon\" /></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>"};

typedef enum{
  WAITTX, REQSNDPGTX, ACKPGTX, HEADTX, MAINPAGETX, FAVICONTX, ONBUTTX, OFFBUTTX
}txStates_t;

typedef enum{
  WAITRX, CLIENTRX, COMMARX, BYTECNTRX, REQRX, URLRX, FLUSHRX, SERVISMODERX
}rxStates_t;

typedef enum{
  LOGSR, WAITSR, ATCOMMSR, ATSENDSR
}srStates_t;

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

//Funkce hleda retezec v poslednich flen prijatych znacich.
unsigned int com_findInFifo(const char* cArr, const char *fifo, unsigned int flen)
{
  unsigned int ret; //navratova hodnota
  unsigned int j; //index znaku v retezci

  //zjisteni delky retezce
  j = 0;
  while(cArr[j] != '\0')
  {
    j++;
  }
  
  ret = (j != 0 && j <= flen);
  flen = 0;
  //hledani retezce ve fifo
  while(j > 0 && ret)
  {
    j--;
    ret = (fifo[flen] == cArr[j]);
    flen++;
  }
  return ret;
}
//Funkce nahraje novy znak do fifo, novy znak bude mit index 0.
void com_putCharInFifo(char nChar, char *fifo, unsigned int flen)
{
  unsigned int b;

  b = (flen > 1);
  while(b)
  {
    flen--;
    fifo[flen] = fifo[flen-1];
    b = (flen > 0);
  }
  fifo[0] = nChar;
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

unsigned int com_putCharInCyrcBuff(char nChar, char *cArr, unsigned int msk)
{
  static unsigned int idx = 0;
  idx++;
  idx &= msk;
  cArr[idx] = nChar;
}

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
  static int txch = 0; //index aktualne vysilaneho znaku
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
  while(esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    com_putCharInFifo(dataByte, rxFifo, COM_RX_LEN);
    rxch++; //pocet prectenych znaku zvysit o 1
    wrLog(dataByte, (srState == LOGSR));
    switch(rxState)
    {
      case WAITRX:
        if(com_findInFifo("+IPD,", rxFifo, COM_RX_LEN))
        {
          PRTDBG("\nPrijem");
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
          else if(com_findInFifo("/wtf?do=rozsvit ", rxFifo, COM_RX_LEN))
          {
            PRTDBG("\nTlacitko zapni");
            reqState = ONBUTTX;
          }
          else if(com_findInFifo("/wtf?do=zhasni ", rxFifo, COM_RX_LEN))
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
  switch(txState)
  {
    case WAITTX: //idle
      if(rxState == WAITRX)//RX musi byt v klidu
      {
        com_delay(0);//znovunastaveni casu
        txState = reqState;
        if(srState == ATSENDSR) //odeslani servisni zpravy
        {
          esp8266Ser.println(srMsg);
          srState = LOGSR;
        }
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
  //SERVIS------------------------------------------------------
  while(debugSer.available())
  {
    dataByte = debugSer.read();
    com_putCharInFifo(dataByte, srFifo, COM_SR_LEN);

    switch(srState)
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

