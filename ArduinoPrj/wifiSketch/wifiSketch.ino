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
  Serial.println("AT+CWJAP=\"sm3\",\"koriandr\"");
  mySerial.println("AT+CWJAP=\"sm3\",\"koriandr\"");
  waitFor("OK");
  Serial.println("AT+CIPSERVER=1,80");
  mySerial.println("AT+CIPSERVER=1,80");
  waitFor("OK");
  Serial.println("Cekam na GET");
  waitFor("GET");
  delay(2000);
  while(mySerial.available())
  {
      Serial.print(mySerial.read());
  }
  Serial.print("\n");
  Serial.println("AT+CIPSEND=0,90");
  mySerial.println("AT+CIPSEND=0,90");
  waitFor(">");
  Serial.println(htmlHead);
  mySerial.println(htmlHead);
  Serial.println(htmlStr);
  mySerial.println(htmlStr);
  waitFor("OK");
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
      Serial.print(dataByte);
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
  while(mySerial.available())
  {
      dataByte = mySerial.read();
      Serial.print(dataByte);
  }
  Serial.print("\n");
}
