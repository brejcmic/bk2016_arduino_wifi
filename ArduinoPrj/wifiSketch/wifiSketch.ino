#include <SoftwareSerial.h>

SoftwareSerial softSerial(10, 11); // RX, TX

#define esp8266Ser  softSerial
#define debugSer    Serial

struct{
  int client;
  int rxLength;
  int page;
}htmlReq;

void setup() {
  debugSer.begin(115200, SERIAL_8N1);
  esp8266Ser.begin(115200);
  debugSer.println("AT+UART_CUR=9600,8,1,0,0");
  esp8266Ser.println("AT+UART_CUR=9600,8,1,0,0");
  delay(2000); //musi byt delay na prenastaveni
  esp8266Ser.begin(9600);
  debugSer.println("AT");
  esp8266Ser.println("AT");
  waitFor("OK");
  debugSer.println("AT+CIPMUX=1");
  esp8266Ser.println("AT+CIPMUX=1");
  waitFor("OK");
  debugSer.println("AT+CWJAP=\"brejcmicDebug\",\"testwifi\"");
  esp8266Ser.println("AT+CWJAP=\"brejcmicDebug\",\"testwifi\"");
  waitFor("OK");
  debugSer.println("AT+CIPSERVER=1,80");
  esp8266Ser.println("AT+CIPSERVER=1,80");
  waitFor("OK");
}

void loop() {
  // put your main code here, to run repeatedly:
  
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
  htmlReq.client = esp8266Ser.read();
  htmlReq.client -= 48;
  htmlReq.client = esp8266Ser.read();//carka
  //pocet prichozich byte
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
  idx = 2;
  waitFor("GET");
  dataByte = '\0';
  while(dataByte != '\n' || dataByte != '\r')
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
  //identifikace stranky
  if(page == String("H: /button")) htmlReq.page = 1;
  else htmlReq.page = 0;
  while(idx < htmlReq.rxLength) //zbytek vypsat na konzoli
  {
    if(esp8266Ser.available())
    {
      dataByte = esp8266Ser.read();
      debugSer.write(dataByte);
      idx++;
    }
  }
  debugSer.print("\n\n");
}

void waitFor(const char* string)
{
  char dataByte;
  int idx;

  debugSer.println("Prijato:");
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

  headPage = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n";
  
  webPage = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"><html><head><title> Stranka 1</title></head><body>Ahoj<form action=\"/wtf\"><input type=\"submit\" name=\"do\" value=\"zhasni\"><input type=\"submit\" name=\"do\" value=\"rozsvit\"></form> </body></html>";
  headPage += "Content-Length: ";
  headPage += webPage.length();
  headPage += "\r\n\r\n"; //musi byt 2

  debugSer.print("AT+CIPSEND=0,");
  debugSer.println(headPage.length() + webPage.length());
  esp8266Ser.print("AT+CIPSEND=0,");
  esp8266Ser.println(headPage.length() + webPage.length());
  waitFor(">");
  debugSer.println(headPage + webPage);
  esp8266Ser.print(headPage + webPage);
  waitFor("OK");
}

