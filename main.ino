#define CS_DI 5
#define DC_DI 6
#define RST_DI 7
#define TINY_GSM_MODEM_SIM800
#define ss Serial3
#define SIM800 Serial2

#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include <fonts.h>
#include <HCuOLED.h>
#include <SPI.h>

TinyGPSPlus gps;

TinyGsm modem(SIM800);

TinyGsmClient client(modem);

HCuOLED HCuOLED(SSD1307, CS_DI, DC_DI, RST_DI);

static const uint32_t StdBaud = 9600;

const char apn[]      = "internet.mts.ru";
const char gprsUser[] = "mts";
const char gprsPass[] = "mts";

const String server   = "testserv1.k-telecom.org";
const char resource[] = "/api";
const char sendData[] = "/api/positions/hardware";
const int  port       = 5000;

String _response = "";
String _IMEI = ""; 
String simBalance = "";

HttpClient http(client, server, port);

void setup()
{
  Serial.begin(StdBaud);
  ss.begin(StdBaud);
  Serial2.begin(StdBaud);

  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);
  delay(300);
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    Serial.println(" fail");
    delay(1000);
    return;
  } Serial.println(" success");

  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }
  
  Serial.print(F("Connecting to "));
  Serial.print(apn);

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" fail");
    delay(1000);
    return;
  } Serial.println(" success");

  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  }
  
  _IMEI = modem.getIMEI();

  HCuOLED.Reset(); 
}

void loop()
{
  if (ss.available() > 0)
  {
    if (gps.encode(ss.read()))
    {
      simBalance = getSimBalance();

      uint8_t level = round((analogRead(A0)* 100)/400);

      // Экран
      HCuOLED.Reset();
      HCuOLED.Reset();
      
      HCuOLED.Cursor(10, 6);
      HCuOLED.SetFont(Terminal_8pt);
      HCuOLED.Print(gps.location.lat(), 7, 7);
      
      HCuOLED.Cursor(10, 20);
      HCuOLED.SetFont(Terminal_8pt);
      HCuOLED.Print(gps.location.lng(), 7, 7);
     
      HCuOLED.Cursor(10,34);
      HCuOLED.SetFont(Terminal_8pt);
      HCuOLED.Print(simBalance.toFloat(), 2, 2);
      
      HCuOLED.Cursor(10,48);
      HCuOLED.SetFont(Terminal_8pt);
      HCuOLED.Print(level, 2, 2);
      
      HCuOLED.Rect(0, 0, 127, 63, OUTLINE);
      HCuOLED.Refresh();
      
        Serial.println("========== HTTP/POST ==========");
        
        String contentType = "application/x-www-form-urlencoded";
        String postData = "latitude=";
        postData.concat(String(gps.location.lat(), 6));
        postData.concat("&longitude=");
        postData.concat(String(gps.location.lng(), 6));
        postData.concat("&imei=");
        postData.concat(_IMEI);
        postData.concat("&balance=");
        postData.concat(simBalance);
        int err = http.post(sendData, contentType, postData);
        
        if (err != 0)
        {
          Serial.print(F("failed to connect: "));
          Serial.print(err);
          delay(10000);
          return;
        }
        int statusCode = http.responseStatusCode();
        Serial.print(F("Response status code: "));
        Serial.println(statusCode);
        if (!statusCode) 
        {
          delay(10000);
          Serial.print("err+");
          return;
        }
        String body = http.responseBody();
        Serial.println(F("Response:"));
        Serial.println(body);

      delay(5000);
      
      
    }
  }
}

String getSimBalance() {
  _response = sendATCommand("AT+CUSD=1,\"*100#\"", true);
  _response = waitResponse();                 // Получаем ответ от модема для анализа
  _response.trim();                           // Убираем лишние пробелы в начале и конце
  if (_response.startsWith("+CUSD:")) {       // Пришло уведомление о USSD-ответе
    String msgBalance = _response.substring(_response.indexOf("\"") + 1);  // Получаем непосредственно содержимое ответа
    msgBalance = msgBalance.substring(0, msgBalance.indexOf("\""));
    msgBalance = UCS2ToString(msgBalance);          // Декодируем ответ
    float balance = getFloatFromString(msgBalance);   // Парсим ответ на содержание числа
    Serial.println("Баланс: " + (String(balance)));     // Выводим полученный ответ
    return String(balance);
  }
  else {
    return "Get balance: failed";
  }
}

String sendATCommand(String cmd, bool waiting) {
  String _resp = "";                            // Переменная для хранения результата
  Serial.println(cmd);                          // Дублируем команду в монитор порта
  SIM800.println(cmd);                          // Отправляем команду модулю
  if (waiting) {                                // Если необходимо дождаться ответа...
    _resp = waitResponse();                     // ... ждем, когда будет передан ответ
    // Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
    if (_resp.startsWith(cmd)) {                // Убираем из ответа дублирующуюся команду
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
    }
    Serial.println(_resp);                      // Дублируем ответ в монитор порта
  }
  return _resp;                                 // Возвращаем результат. Пусто, если проблема
}

String waitResponse() {                         // Функция ожидания ответа и возврата полученного результата
  String _resp = "";                            // Переменная для хранения результата
  long _timeout = millis() + 10000;             // Переменная для отслеживания таймаута (10 секунд)
  while (!SIM800.available() && millis() < _timeout)  {}; // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
  if (SIM800.available()) {                     // Если есть, что считывать...
    _resp = SIM800.readString();                // ... считываем и запоминаем
  }
  else {                                        // Если пришел таймаут, то...
    Serial.println("Timeout...");               // ... оповещаем об этом и...
  }
  return _resp;                                 // ... возвращаем результат. Пусто, если проблема
}

String UCS2ToString(String s) {                      
  String result = "";
  unsigned char c[5] = "";                           
  for (int i = 0; i < s.length() - 3; i += 4) {      
    unsigned long code = (((unsigned int)HexSymbolToChar(s[i])) << 12) +   
                         (((unsigned int)HexSymbolToChar(s[i + 1])) << 8) +
                         (((unsigned int)HexSymbolToChar(s[i + 2])) << 4) +
                         ((unsigned int)HexSymbolToChar(s[i + 3]));
    if (code <= 0x7F) {                               
      c[0] = (char)code;                              
      c[1] = 0;                                   
    } else if (code <= 0x7FF) {
      c[0] = (char)(0xC0 | (code >> 6));
      c[1] = (char)(0x80 | (code & 0x3F));
      c[2] = 0;
    } else if (code <= 0xFFFF) {
      c[0] = (char)(0xE0 | (code >> 12));
      c[1] = (char)(0x80 | ((code >> 6) & 0x3F));
      c[2] = (char)(0x80 | (code & 0x3F));
      c[3] = 0;
    } else if (code <= 0x1FFFFF) {
      c[0] = (char)(0xE0 | (code >> 18));
      c[1] = (char)(0xE0 | ((code >> 12) & 0x3F));
      c[2] = (char)(0x80 | ((code >> 6) & 0x3F));
      c[3] = (char)(0x80 | (code & 0x3F));
      c[4] = 0;
    }
    result += String((char*)c);  
  }
  return (result);
}

unsigned char HexSymbolToChar(char c) {
  if      ((c >= 0x30) && (c <= 0x39)) return (c - 0x30);
  else if ((c >= 'A') && (c <= 'F'))   return (c - 'A' + 10);
  else                                 return (0);
}

float getFloatFromString(String str) {            // Функция извлечения цифр из сообщения - для парсинга баланса из USSD-запроса
  bool   flag     = false;
  String result   = "";
  str.replace(",", ".");                          // Если в качестве разделителя десятичных используется запятая - меняем её на точку.
  for (int i = 0; i < str.length(); i++) {
    if (isDigit(str[i]) || (str[i] == (char)46 && flag)) { // Если начинается группа цифр (при этом, на точку без цифр не обращаем внимания),
      result += str[i];                           // начинаем собирать их вместе
      if (!flag) flag = true;                     // Выставляем флаг, который указывает на то, что сборка числа началась.
    }
    else {                                        // Если цифры закончились и флаг говорит о том, что сборка уже была,
      if (flag) break;                            // считаем, что все.
    }
  }
  return result.toFloat();                        // Возвращаем полученное число.
}
