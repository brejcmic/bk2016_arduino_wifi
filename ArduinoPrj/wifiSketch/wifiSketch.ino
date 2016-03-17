#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

const char htmlHead[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: 4\r\n";
const char htmlStr[] = "Ahoj\r\n";

void setup() {
  Serial.begin(115200, SERIAL_8N1);
  mySerial.begin(115200);
  Serial.println("AT+UART_CUR=9600,8,1,0,0");
  mySerial.println("AT+UART_CUR=9600,8,1,0,0");
  mySerial.begin(9600);
  delay(1000);
  Serial.println("AT");
  mySerial.println("AT");
  delay(1000);
  waitFor("OK");
  Serial.println("AT+CIPMUX=1");
  mySerial.println("AT+CIPMUX=1");
  delay(1000);
  waitFor("OK");
  Serial.println("AT+CWJAP=\"brejcmicDebug\",\"testwifi\"");
  mySerial.println("AT+CWJAP=\"brejcmicDebug\",\"testwifi\"");
  waitFor("OK");
  Serial.println("AT+CIPSERVER=1,80");
  mySerial.println("AT+CIPSERVER=1,80");
  waitFor("OK");
  Serial.println("Cekam na GET /button");
  waitFor("GET /button");
  printHtml();
}

void loop() {
  // put your main code here, to run repeatedly:
}

void waitFor(const char* string)
{
  char dataByte;
  int idx;

  Serial.println("Prijato:");
  idx = 0;
  while(string[idx] != '\0') //pri chybe nekonecna smycka
  {
    if(mySerial.available())
    {
      dataByte = mySerial.read();
      Serial.write(dataByte);
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
  Serial.print("\n\n");
}

void printHtml()
{
  String webPage;

  webPage = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: 4\r\n";
  webPage += "Ahoj\r\n";

  Serial.print("AT+CIPSEND=0,");
  Serial.println(webPage.length());
  mySerial.print("AT+CIPSEND=0,");
  mySerial.println(webPage.length());
  waitFor(">");
  Serial.println(webPage);
  mySerial.print(webPage);
  waitFor("OK");
}

