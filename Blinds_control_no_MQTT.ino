/*     Blinds control
       Written by W. Hoogervorst
       july 2019
       Quarter steps
       Edit april 2020: +10% and -10% added in Webinterface
       Version without MQTT
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define OPEN_STEPS 400    //10% of maximum number of steps
#define CENTER_STEPS 2000 //50% of maximum number of steps
#define CLOSE_STEPS 3600  //90% maximum number of steps
#define MAX_STEPS 4000    //maximum number of steps
#define TEN_PERCENT 400   //10% of maximum number of steps
#define STEP_DELAY 1000   //delay between steps = speed of change
#define WIFI_CONNECT_TIMEOUT_S 15

// for HTTPupdate
const char* host = "Blinds_control_side";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 6";

/*credentials & definitions */
#define mySSID "put your WiFi network here"
#define myPASSWORD "put your WiFi password here"

long lastReconnectAttempt;

int current_steps = CENTER_STEPS;
int target_steps = CENTER_STEPS;

// defines pins numbers
const int stepPin = 3;
const int dirPin = 1;
const int powerPin = 2;

void setup() {
  // Sets the two pins as Outputs
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(powerPin, OUTPUT);
WiFi.mode(WIFI_STA);
  digitalWrite(powerPin, HIGH); // disable the stepper motor
  //connect to WiFi
  setup_wifi();
    // for HTTPudate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);

  httpServer.on("/", handleRoot);
  httpServer.on("/center", handle_center);
  httpServer.on("/open10", handle_open10);
  httpServer.on("/close10", handle_close10);
  httpServer.on("/opened", handle_opened);
  httpServer.on("/closed", handle_closed);
  httpServer.on("/pre_update", handle_update);
  httpServer.onNotFound(handle_NotFound);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)     // check if WiFi connection is present
  {
    // Wifi is connected
    httpServer.handleClient();    // for HTTPupdate
  }
  else
    // Client is not connected
  {
    long now = millis();
    if (now - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      setup_wifi();
      if (WiFi.status() == WL_CONNECTED)
        lastReconnectAttempt = 0;
    }
  }
  if (current_steps != target_steps)
  {
    digitalWrite(powerPin, LOW); // enable the stepper motor
    if (current_steps > target_steps)
    {
      digitalWrite(dirPin, HIGH); // Enables the motor to move in a particular direction
      while (current_steps > target_steps)
      {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(STEP_DELAY);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(STEP_DELAY);
        current_steps--;
      }
    }
    else
    {
      digitalWrite(dirPin, LOW); // Enables the motor to move in a particular direction
      while (current_steps < target_steps)
      {
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(STEP_DELAY);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(STEP_DELAY);
        current_steps++;
      }
    }
    digitalWrite(powerPin, HIGH); // disable the stepper motor
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  WiFi.begin(mySSID, myPASSWORD);
  uint32_t time1 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.print(".");
    if (millis() > time1 + (WIFI_CONNECT_TIMEOUT_S * 1000))
      ESP.restart();
  }
}

void handleRoot() {
  //Serial.println("Connected to client");
  httpServer.send(200, "text/html", SendHTML());
}
void handle_OnConnect() {
  //Serial.println("Connected to client");
  httpServer.send(200, "text/html", SendHTML());
}

void handle_center() {
  target_steps = CENTER_STEPS;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_close10() {
  if (current_steps < (MAX_STEPS - TEN_PERCENT))
    target_steps = current_steps + TEN_PERCENT;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_open10() {
  if (current_steps > (TEN_PERCENT))
    target_steps = current_steps - TEN_PERCENT;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_opened() {
  target_steps = OPEN_STEPS;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_closed() {
  target_steps = CLOSE_STEPS;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_update() {
  target_steps = CENTER_STEPS;;
  httpServer.send(200, "text/html", "<a href=\"/update\">Update</a>");
}

void handle_NotFound() {
  httpServer.send(404, "text/plain", "Not found");
}

String SendHTML() {
  String ptr = "<!DOCTYPE html><html>\n";
  ptr += "<head><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>";
  // change line below to change the name of the webpage
  ptr += host;
  ptr += "</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 25px auto 30px;} h3 {color: #444444;margin-bottom: 30px;}\n";
  ptr += ".button {width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px 10px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-1 {background-color: #34495e;}\n";
  ptr += ".button-1:active {background-color: #2c3e50;}\n";
  ptr += ".button-2 {background-color: #0d41d1;}\n";
  ptr += ".button-2:active {background-color: #082985;}\n";
  ptr += ".button-update {background-color: #a32267;}\n";
  ptr += ".button-update:active {background-color: #961f5f;}\n";
  ptr += "p {font-size: 18px;color: #383535;margin-bottom: 15px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  //change lines below to change the displayed names
  ptr += "<h1>Blinds side Web Control</h1>\n";
  ptr += "<h3>Control Blinds and link to HTTPWebUpdate</h3>\n";
  ptr += "<p>WimIOT\nDevice: ";
  ptr += host;
  ptr += "<br>Software version: ";
  ptr += software_version;
  ptr += "<br><br></p>";

  ptr += "<a class=\"button button-1\" href=\"/opened\">Opened</a>&nbsp;\n";
  ptr += "<a class=\"button button-1\" href=\"/center\">Center</a>&nbsp;\n";
  ptr += "<a class=\"button button-1\" href=\"/closed\">Closed</a><br><br><br>\n";

  ptr += "<a class=\"button button-2\" href=\"/open10\">Open 10%</a>&nbsp;\n";
  ptr += "<a class=\"button button-2\" href=\"/close10\">Close 10%</a><br><br><br>\n";

  ptr += "<p>Click for update page</p><a class=\"button button-update\" href=\"/pre_update\">Update</a>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
