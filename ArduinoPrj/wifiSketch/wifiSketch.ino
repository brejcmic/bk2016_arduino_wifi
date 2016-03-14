void setup() {
  Serial.begin(115200, SERIAL_8N1);
  Serial.println("AT");
  delay(1000);
  waitFor("OK\n");
  Serial.println("AT+CIPMUX=1");
  waitFor("OK\n");
  Serial.println("AT+CWJAP=\"sm3\",\"koriandr\"");
  waitFor("OK\n");
  Serial.println("AT+CIPSERVER=1,80");
  waitFor("OK\n");
  waitFor("CONNECT\n");
  Serial.println("AT+CIPSEND=0,6");
  waitFor(">\n");
  Serial.println("Ahoj");
}

void loop() {
  // put your main code here, to run repeatedly:
}

void waitFor(const char* string)
{
  char dataByte;
  int idx;
  
  idx = 0;
  while(string[idx] != '\0') //pri chybe nekonecna smycka
  {
    if(Serial.available())
    {
      dataByte = Serial.read();
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
}
