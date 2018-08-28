//**************************************************************************
// Code for the LoLin NodeMCU V3 board with onboard ESP8266-12E WiFi module.
// 
// What the program does:
// - Detects pulses from an electric power meter, by using two LM393 based
//   optic sensor boards. Pulses for kWh and kVar are detected separately.
// - Calculates kW and kVar based on duration between pulses.
// - Calculates total consumed energy like shown on the meter, if the
//   counter value on the webpage is set equal to the meter's counter at
//   boot time.
//**************************************************************************
// GPIO(MCU pin)     NodeMCU V3 pin                        Pull Up / Down
//    0              D3  Boot select - Use if it works ok      Pull Up
//    1              D10 UART0 TX Serial - Avoid it            Pull Up
//    2              D4  Boot select - Use if it works ok      Pull Up
//    3              D9  UART0 RX Serial - Avoid               Pull Up
//    4              D2  Normally for I2C SDA                  Pull Up
//    5              D1  Normally for I2C SCL                  Pull Up
//    6              --- For SDIO Flash - Not useable          Pull Up
//    7              --- For SDIO Flash - Not useable          Pull Up
//    8              --- For SDIO Flash - Not useable          Pull Up
//    9              D11 For SDIO Flash - May not work         Pull Up
//   10              D12 For SDIO Flash - May work             Pull Up
//   11              --- For SDIO Flash - Not useable          Pull Up
//   12              D6  Ok Use                                Pull Up
//   13              D7  Ok Use                                Pull Up
//   14              D5  Ok Use                                Pull Up
//   15              D8  Boot select - Use if it works ok      Pull Up (N/A)
//   16              D0  Wake up - May cause reset - Avoid it  Pull Down
//
// The value of the internal pull-resistors are between 30k and 100k ohms.
// A google search shows that using them is not the way to go (difficult).
//
// GPIO 0 -> 15 may be used as interrupt pins
//
// The 3.3V supply on the board may deliver up to 800mA (also from google).
//**************************************************************************
// Logic input/output levels with 3.3V power supply to ESP8266:
// LOW: -0.3V -> 0.825V, HiGH: 2.475V -> 3.6V
//**************************************************************************
// BOOT SELECT ALTERNATIVES:
// GPIO15   GPIO0    GPIO2         Mode
// 0V       0V       3.3V          UART Bootloader
// 0V       3.3V     3.3V          Boot sketch (SPI flash)
// 3.3V     x        x             SDIO mode (not used for Arduino)
//**************************************************************************

#include <ESP8266WiFi.h>
// #include <WiFiClient.h> // Already included in ESP8266WiFi.h
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <math.h>

#define PULSES_PER_UNIT 2000           // A unit is one kW or one kVar
#define MICROS_PER_HOUR 3600000000
#define MICROS_BETWEEN_UPDATES 1000000 // Limit strain on the websock connection

// Create objects
MDNSResponder mdns;
ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// WiFi details
static const char ssid[] = "Your_ssid";
static const char password[] = "Your_password";

unsigned long currMicros = 0;             // Microseconds since ESP8266 started
                                          // or since last micros wraparound.
const byte wattInterruptPin = 13;         // D7 on NodeMCU V3 board
const byte varInterruptPin = 14;          // D5 on NodeMCU V3 board
volatile byte wattInterruptIndicator = 0; // Indicator for watt interrupt
volatile byte varInterruptIndicator = 0;  // Indicator for var interrupt
unsigned long wattOldMicros = 0;          // Micros on previous watt interrupt
unsigned long lastWattUpdate = 0;         // Micros on last kW update
long wattMicrosInterval = 0;              // Micros since last watt interrupt
unsigned long varOldMicros = 0;           // Micros on previous var interrupt
unsigned long lastVarUpdate = 0;          // Micros on last kVar update
long varMicrosInterval = 0;               // Micros since last var interrupt
double kW = 0;                            // kiloWatt value
double kVar = 0;                          // KiloVar value
unsigned long totalKwhPulses = 1;         // Total number of kWh pulses on the
                                          // meter since new (calculated).
unsigned long tempKwhPulses = 1;          // Number of kWh pulses since NodeMCU
                                          // was started.
unsigned long tempKvarhPulses = 1;        // Number of kVArh pulses since
                                          // NodeMCU was started.
unsigned long totalKwh = 0;               // Enter this value on the webpage
                                          // at boot time, read it on the meter.
double cosPhi = 1.;                       // cos phi calculated from pulses
                                          // since NodeMCU was started.


// THE WEBSITE STUFF
// The following lines declare a string that goes into flash memory.
// Defined by: "rawliteral(... any text / characters... )rawliteral"
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name = "viewport" content = "width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0">
<title>ESP8266 WebSocket Demo</title>
<style>
"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
</style>

<script>
// Create a Websock object for the javascript
var websock;

function start() {
  websock = new WebSocket('ws://' + window.location.hostname + ':81/');
  websock.onopen = function(evt) { console.log('websock open'); };
  websock.onclose = function(evt) { console.log('websock close'); };
  websock.onerror = function(evt) { console.log(evt); };
  
  // The onmessage function below is called when a message
  // is sent to the server from the Arduino hardware code.
  // Example call: webSocket.broadcastTXT(str02);
  websock.onmessage = function(evt) {
    console.log(evt);
    var g = evt.data;
    // Update of the measurands
    if (g.substring(0, 3) === '000') {
      var a = g.substring(3);
      document.getElementById('kiloWatt').innerHTML = a;
    }
    if (g.substring(0, 3) === '001') {
      var a = g.substring(3);
      document.getElementById('wattMillInt').innerHTML = a;
    }
    if (g.substring(0, 3) === '002') {
      var a = g.substring(3);
      document.getElementById('kiloWattHour').innerHTML = a;
    }
    if (g.substring(0, 3) === '003') {
      var a = g.substring(3);
      document.getElementById('kwhPulses').innerHTML = a;
    }
    if (g.substring(0, 3) === '004') {
      var a = g.substring(3);
      document.getElementById('cosinPhi').innerHTML = a;
    }
    if (g.substring(0, 3) === '005') {
      var a = g.substring(3);
      document.getElementById('kiloVar').innerHTML = a;
    }
    if (g.substring(0, 3) === '006') {
      var a = g.substring(3);
      document.getElementById('varMillInt').innerHTML = a;
    }
    if (g.substring(0, 3) === '007') {
      var a = g.substring(3);
      document.getElementById('kvarhPulses').innerHTML = a;
    }
  }; // End of onmessage function
} // End of start function


// The following function is called from button click on the client
// html page. The websock.send(...) generates a websocket event and
// calls the webSocketEvent(...) method in the C code.
// Set the meter kWh value manually
function changeKwhValue() {
  var numKwh = '00#' + String(document.getElementById('newKwh').value);
  //console.log('numKwh: ' + numKwh);
  websock.send(numKwh); // Send value to the hardware C code
}
</script>
</head>

<!-- Starting the javascript -->
<body onload="javascript:start();">

<!-- Content shown on the client screen -->
<table>
<tr><td><b>ESP8266</b></td><td><b> WEBSOCKET</b></td></tr>
<tr><td>NodeMCU v3</td><td> dev board</td></tr>
<tr><td><br></td></tr>
<tr><td><b>ACTIVE POWER (kW):</b></td>
<td align="right"><div id="kiloWatt">xxxxxxx</div></td></tr>
<tr><td><b>Pulse interval (ms):</b></td>
<td align="right"><div id="wattMillInt">xxxxxxx</div></td></tr>
<tr><td><b>Pulses since startup:</b></td>
<td align="right"><div id="kwhPulses">xxxxxxxx</div></td></tr>
<tr><td><b>(kWh input pin = D7)</b></td></tr>
<tr><td><br></td></tr>
<tr><td><b>TOTAL ENERGY (kWh):</b></td>
<td align="right"><div id="kiloWattHour">xxxxxxx</div></td></tr>
<tr><td><b>cos phi since startup:</b></td>
<td align="right"><div id="cosinPhi">xxxxxxx</div></td></tr>
<tr><td><br></td></tr>
<tr><td><b>REACTIVE POWER (kVAr):</b></td>
<td align="right"><div id="kiloVar">xxxxxxx</div></td></tr>
<tr><td><b>Pulse interval (ms):</b></td>
<td align="right"><div id="varMillInt">xxxxxxx</div></td></tr>
<tr><td><b>Pulses since startup:</b></td>
<td align="right"><div id="kvarhPulses">xxxxxxxx</div></td></tr>
<tr><td><b>(kVArh input pin = D5)</b></td></tr>
<tr><td><br></td></tr>
</table>
<form name="kWhNumber" >
  Set kWh (0-999999):
  <input id="newKwh" type="number" name="quantity" min="0" max="999999" value="000000" maxlength="6">
  <button id="a" type="button" onclick="changeKwhValue(this);">Send</button>
</form>

</body>
</html>
)rawliteral";
// End of what goes into flash memory
// END OF THE WEBSITE STUFF



void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  Serial.println();
  Serial.printf("webSocketEvent(%d, %d, ...)\r\n", num, type);
  
  switch(type) {
    case WStype_DISCONNECTED: {
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    }
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      break;
    }
    case WStype_TEXT: {
      if ((payload[0] == '0') && (payload[1] == '0') && (payload[2] == '#')) {
        char ch3 = payload[3];
        char ch4 = payload[4];
        char ch5 = payload[5];
        char ch6 = payload[6];
        char ch7 = payload[7];
        char ch8 = payload[8];
        String nStr = "";
        nStr = nStr + ch3 + ch4 + ch5 + ch6 + ch7 + ch8;
        int nInt = atoi(nStr.c_str());
        totalKwhPulses = nInt * PULSES_PER_UNIT;
      }
      break;
    }
    case WStype_BIN: {
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
      hexdump(payload, length);
      // echo data back to browser
      webSocket.sendBIN(num, payload, length);
      break;
    }
    default: {
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
    }
  }
}



void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}



void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}



void setup() {

  Serial.begin(115200);
  pinMode(wattInterruptPin, INPUT_PULLUP);
  pinMode(varInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(wattInterruptPin), handleWattInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(varInterruptPin), handlevarInterrupt, FALLING);

  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] BOOT WAIT %d...\r\n", t);
    Serial.flush();
    delay(1000);
  }

  WiFiMulti.addAP(ssid, password);

  while(WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("espWebSock", WiFi.localIP())) {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);
    mdns.addService("ws", "tcp", 81);
  } else {
    Serial.println("MDNS.begin failed");
  }
  Serial.print("Connect to http://espWebSock.local or http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}



void handleWattInterrupt() {
  wattInterruptIndicator++;
}



void handlevarInterrupt() {
  varInterruptIndicator++;
}



void loop() {
  webSocket.loop();
  server.handleClient();

  if (wattInterruptIndicator>0) { // Check if we have a watt interrupt
    currMicros = micros();
    wattMicrosInterval = currMicros - wattOldMicros;
    kW = ((double)1 / PULSES_PER_UNIT) / ((double)wattMicrosInterval / MICROS_PER_HOUR);
    wattOldMicros = currMicros;
    totalKwhPulses += 1;
    tempKwhPulses += 1;
    totalKwh = totalKwhPulses / PULSES_PER_UNIT;
    cosPhi = 1. / sqrt(1 + pow((double)tempKvarhPulses / (double)tempKwhPulses, 2));
    if ((currMicros - lastWattUpdate) > MICROS_BETWEEN_UPDATES) {
      // Serial.print("kW:                 ");
      // Serial.println(kW);
      // Serial.print("Avg. over (ms):     ");
      // Serial.println(wattMicrosInterval / 1000);
      // Serial.print("Total kWh:          ");
      // Serial.println(totalKwh);
      // Serial.print("cos phi:            ");
      // Serial.println(cosPhi);
      // Serial.println();
      String str00 = "000";
      String str01 = String(kW);
      String str02 = str00 + str01;
      webSocket.broadcastTXT(str02);
      String str10 = "001";
      String str11 = String(wattMicrosInterval / 1000);
      String str12 = str10 + str11;
      webSocket.broadcastTXT(str12);
      String str20 = "002";
      String str21 = String(totalKwh);
      String str22 = str20 + str21;
      webSocket.broadcastTXT(str22);
      String str30 = "003";
      String str31 = String(tempKwhPulses);
      String str32 = str30 + str31;
      webSocket.broadcastTXT(str32);
      String str40 = "004";
      String str41 = String(cosPhi);
      String str42 = str40 + str41;
      webSocket.broadcastTXT(str42);
      lastWattUpdate = currMicros;
    }
    wattInterruptIndicator--;
  }
  
  if (varInterruptIndicator>0) { // Check if we have a var interrupt
    currMicros = micros();
    varMicrosInterval = currMicros - varOldMicros;
    kVar = ((double)1 / PULSES_PER_UNIT) / ((double)varMicrosInterval / MICROS_PER_HOUR);
    varOldMicros = currMicros;
    tempKvarhPulses += 1;
    if ((currMicros - lastVarUpdate) > MICROS_BETWEEN_UPDATES) {
      // Serial.print("kVar:               ");
      // Serial.println(kVar);
      // Serial.print("Avg. over (ms):     ");
      // Serial.println(varMicrosInterval / 1000);
      // Serial.println();
      String str50 = "005";
      String str51 = String(kVar);
      String str52 = str50 + str51;
      webSocket.broadcastTXT(str52);
      String str60 = "006";
      String str61 = String(varMicrosInterval / 1000);
      String str62 = str60 + str61;
      webSocket.broadcastTXT(str62);
      String str70 = "007";
      String str71 = String(tempKvarhPulses);
      String str72 = str70 + str71;
      webSocket.broadcastTXT(str72);
      lastVarUpdate = currMicros;
    }
    varInterruptIndicator--;
  }
}

