#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>
#include <WiFiClient.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
PZEM004Tv30 pzem(D3, D4); // (RX, TX) of NodeMCU connect to (TX, RX) of PZEM-004t

// Replace with your network credentials
const char* ssid = "(tidak diketahui)";
const char* password = "qwerty123";

float voltage, current, power, energy, frequency, powerfactor;

//Your Domain name with URL path or IP address with path
String serverName = "http://linveruns.000webhostapp.com/api/postdata";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

void setup() {
  Serial.begin(9600); // used to determine the speed and receipt of data through the serial port
  Serial.println("Program started"); // program started
  initWiFi();
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  lcd.init(); // initialize the lcd
  lcd.backlight(); // backlight is turned on
  lcd.setCursor(3, 0);
  lcd.print("</LINVER/>");
  lcd.setCursor(0, 1);
  lcd.print("Program Started!");
  delay(500);
}

void loop() {
  PZEM_func();
  display_LCD();
  // Send an HTTP POST request depending on timerDelay
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      //    String serverPath = serverName + "?temperature=24.37";

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName.c_str());

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      // Data to send with HTTP POST
      String httpRequestData = "Voltage=" + String(voltage) + "&Current=" + String(current) + "&Power=" + String(power) + "&Frequency=" + String(frequency) + "&Pf=" + String(powerfactor) + "&Energy=" + String(energy) + "&submit=enter";
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  //The ESP8266 tries to reconnect automatically when the connection is lost
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void PZEM_func() {
  voltage = pzem.voltage();
  current = pzem.current();
  power = pzem.power();
  energy = pzem.energy();
  frequency = pzem.frequency();
  powerfactor = pzem.pf();
}

void display_LCD() {
  lcd.clear();

  // print voltage value
  if ( !isnan(voltage) ) {
    lcd.setCursor(0, 0);
    lcd.print("V=");
    lcd.print(voltage);
  }
  else  {
    lcd.setCursor(0, 0);
    lcd.print("V=");
    lcd.print("n/A");
  }

  // print current value
  if ( !isnan(current) ) {
    lcd.setCursor(0, 1);
    lcd.print("A=");
    lcd.print(current);
  }
  else {
    lcd.setCursor(0, 1);
    lcd.print("A=");
    lcd.print("n/A");
  }

  // print power value
  if ( !isnan(power) ) {
    lcd.setCursor(9, 0);
    lcd.print("W=");
    lcd.print(power);
  }
  else  {
    lcd.setCursor(9, 0);
    lcd.print("W=");
    lcd.print("n/A");
  }

  // print frequency value
  if ( !isnan(frequency) ) {
    lcd.setCursor(9, 1);
    lcd.print("F=");
    lcd.print(frequency);
  }
  else {
    lcd.setCursor(9, 1);
    lcd.print("F=");
    lcd.print("n/A");
  }

  delay(1000);
}
