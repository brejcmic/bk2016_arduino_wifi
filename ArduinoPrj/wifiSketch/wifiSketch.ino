#include <SoftwareSerial.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define esp8266Ser  softSerial
#define ESP8266SPEED  9600// cilova rzchlost kolmunikace
#define ESP8266CHARCT 20// pocet najednou odesilanych znaku
#define ESP8266TXPER  5ul// perioda vysilani v ms

#define debugSer    Serial

const PROGMEM char mainpage[]= {"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title> Stranka 1</title></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>"};

typedef enum{
  NA, FAVICON, MAINPAGE
}httpRequest;

struct{
  int client;
  int rxLength;
  httpRequest page;
  int txch; //index aktualne vysilaneho znaku
}htmlReq;

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
  digitalWrite(7, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  getRxID();
  printHtml();
}

void getRxID(void)
{
  char dataByte;
  int idx;
  String page = "H:";
  
  dataByte = '\0';
  debugSer.println("Cekam na Prichozi komunikaci");
  waitFor("+IPD,");
  while(esp8266Ser.available() < 2 );
  dataByte = esp8266Ser.read();
  debugSer.write(dataByte);
  htmlReq.client = dataByte - 48;
  dataByte = esp8266Ser.read();//carka
  debugSer.write(dataByte);
  //pocet prichozich byte
  dataByte = '\0';
  htmlReq.rxLength = 0;
  while(dataByte != ':')
  {
    if(dataByte != '\0')
    {
      dataByte -= 48;
      htmlReq.rxLength = 10 * htmlReq.rxLength + dataByte;
      dataByte = '\0';
    }
    else if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
    }
  }
  idx = 4;
  waitFor("GET ");
  dataByte = '\0';
  while(dataByte != ' ')
  {
    if(dataByte != '\0')
    {
      page += String(dataByte);
      dataByte = '\0';
    }
    else if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      idx++;
    }
  }
  debugSer.print("\nKonec identifikace stranky: ");
  debugSer.println(page);
  //identifikace stranky
  if(page == String("H:/favicon.ico")) htmlReq.page = FAVICON;
  else if(page == String("H:/")) htmlReq.page = MAINPAGE;
  else if(page == String("H:/wtf?do=rozsvit"))
  {
    digitalWrite(7, HIGH);
    htmlReq.page = MAINPAGE;
  }
  else if(page == String("H:/wtf?do=zhasni")) 
  {
    digitalWrite(7, LOW);
    htmlReq.page = MAINPAGE;
  }
  else htmlReq.page = NA;
  //zbytek vypsat na konzoli
  debugSer.print("Vypisovani ");
  debugSer.print(htmlReq.rxLength);
  debugSer.println(" znaku:");
  while(idx < htmlReq.rxLength) 
  {
    if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      idx++;
    }
  }
  debugSer.print("\nKonec cteni znaku\n\n");
}

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

void printHtml()
{
  String webPage;
  String headPage;
  if(htmlReq.page == FAVICON)
  {
    
  }
  else if(MAINPAGE)
  {
    headPage = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n";
    
    webPage = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title> Stranka 1</title></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>";
    headPage += "Content-Length: ";
    headPage += webPage.length();
    headPage += "\r\n\r\n"; //musi byt 2
  
    debugSer.print("AT+CIPSEND=");
    debugSer.print(htmlReq.client);
    debugSer.print(',');
    debugSer.println(headPage.length() + webPage.length());
    esp8266Ser.print("AT+CIPSEND=");
    esp8266Ser.print(htmlReq.client);
    esp8266Ser.print(',');
    esp8266Ser.println(headPage.length() + webPage.length());
    waitFor(">");
    debugSer.println(headPage + webPage);
    esp8266Ser.print(headPage + webPage);
    waitFor("OK");
  }
}

int findString(const char* string)
{
  char dataByte;
  static int idx = 0;

  if(esp8266Ser.available())
  {
    dataByte = esp8266Ser.read();
    if(dataByte == string[idx])
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
  }
  return 0;
}

void htmlListener(void)
{
  char dataByte;
  static char state = 1;
  int idx;
  String page = "H:";

  switch(state)
  {
    case 1:
      if(findString("+IPD,")) state = 2;
      break;
    case 2:
      if(esp8266Ser.available())
      {
        dataByte = esp8266Ser.read();
        htmlReq.client = dataByte - 48;
        state = 3;
      }
      break;
    case 3://cteni carky
      if(esp8266Ser.available())
      {
        dataByte = esp8266Ser.read();
        state = 4;
      }
      break;
    case 4://pocet prichozich byte
      if(esp8266Ser.available())
      {
        dataByte = esp8266Ser.read();
        if(dataByte == ':')
        {
          state = 5;
        }
        else if(dataByte != '\0');
        {
          dataByte -= 48;
          htmlReq.rxLength = 10 * htmlReq.rxLength + dataByte;
          dataByte = '\0';
        }
      }
      break;
    case 5://cekani na GET
      if(findString("GET ")) state = 6;
      break;
    case 6://cteni jmena stranky
      if(esp8266Ser.available())
      {
        dataByte = esp8266Ser.read();
        if(dataByte == ' ')
        {
          state = 7;
        }
        else if(dataByte != '\0');
        {
          page += String(dataByte);
        }
      }
      break;
    case 7://vycteni zbytku bufferu
      break;
    default:
      state = 1;
      break;
  }
  dataByte = '\0';
  if(findString("+IPD,"))
  {
    
  }
  waitFor("+IPD,");
  while(esp8266Ser.available() < 2 );
  dataByte = esp8266Ser.read();
  debugSer.write(dataByte);
  htmlReq.client = dataByte - 48;
  dataByte = esp8266Ser.read();//carka
  debugSer.write(dataByte);
  //pocet prichozich byte
  dataByte = '\0';
  htmlReq.rxLength = 0;
  while(dataByte != ':')
  {
    if(dataByte != '\0')
    {
      dataByte -= 48;
      htmlReq.rxLength = 10 * htmlReq.rxLength + dataByte;
      dataByte = '\0';
    }
    else if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
    }
  }
  idx = 4;
  waitFor("GET ");
  dataByte = '\0';
  while(dataByte != ' ')
  {
    if(dataByte != '\0')
    {
      page += String(dataByte);
      dataByte = '\0';
    }
    else if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      idx++;
    }
  }
  debugSer.print("\nKonec identifikace stranky: ");
  debugSer.println(page);
  //identifikace stranky
  if(page == String("H:/favicon.ico")) htmlReq.page = FAVICON;
  else if(page == String("H:/")) htmlReq.page = MAINPAGE;
  else if(page == String("H:/wtf?do=rozsvit"))
  {
    digitalWrite(7, HIGH);
    htmlReq.page = MAINPAGE;
  }
  else if(page == String("H:/wtf?do=zhasni")) 
  {
    digitalWrite(7, LOW);
    htmlReq.page = MAINPAGE;
  }
  else htmlReq.page = NA;
  //zbytek vypsat na konzoli
  debugSer.print("Vypisovani ");
  debugSer.print(htmlReq.rxLength);
  debugSer.println(" znaku:");
  while(idx < htmlReq.rxLength) 
  {
    if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      idx++;
    }
  }
  debugSer.print("\nKonec cteni znaku\n\n");
}

void htmlSender(void)
{
  static unsigned long lastTime;
  char dataByte;
  const char *page;
  int  sizeOfPage;
  int idx;
  
  switch(htmlReq.page)
  {
    case FAVICON:
      break;
    case MAINPAGE:
      page = &mainpage[0];
      sizeOfPage = sizeof(mainpage);
      break;
    default:
      break;
  }
  if(htmlReq.txch == -2) //nove vysilani
  {
    esp8266Ser.print("AT+CIPSEND=");
    esp8266Ser.print(htmlReq.client);
    esp8266Ser.print(',');
    esp8266Ser.print(sizeOfPage);
    htmlReq.txch = -1; //ocekavani odpovedi od ESP8266
  }
  else if(htmlReq.txch == -1) //ocekavani povoleni vzsilani od ESP8266
  {
    if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      if(dataByte == '>') 
      {
        htmlReq.txch = 0; //vysilani prvniho znaku
        lastTime = millis(); //aktualizace casu
      }
    }
  }
  else if(htmlReq.txch >= 0)//vysilani retezce v jednotlivych intervalech po ESP8266CHARCT znacich
  {
    if((millis() - lastTime) > ESP8266TXPER)
    {
      lastTime = millis(); //aktualizace casu
      for(idx = 0; idx < ESP8266CHARCT && page[htmlReq.txch] != '\0'; idx++, htmlReq.txch++)
      {
        esp8266Ser.write(page[htmlReq.txch]);
      }
      if(page[htmlReq.txch] == '\0') htmlReq.txch = -3; //konec vysilani
    }
  }
}

