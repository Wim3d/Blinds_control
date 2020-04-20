/*     
      Blinds control
      Written by W. Hoogervorst
      july 2019
      Quarter steps
      Edit april 2020: +10% and -10% added in Webinterface
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define mySSID "put your WiFi network here"
#define myPASSWORD "put your WiFi password here"

#define OPEN_STEPS 400    //10% of maximum number of steps
#define CENTER_STEPS 2000 //50% of maximum number of steps
#define CLOSE_STEPS 3600  //90% maximum number of steps
#define MAX_STEPS 4000    //maximum number of steps
#define TEN_PERCENT 400   //10% of maximum number of steps
#define STEP_DELAY 1000   // delay between steps = speed of change
#define WIFI_CONNECT_TIMEOUT_S 15

// for HTTPupdate and webserver
const char* host = "Blinds_control_side";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 4";

/*credentials & definitions */
//MQTT
const char* mqtt_id = host;
const char* percentage_topic = "blinds/side/percentage";
const char* debug_topic = "blinds/side/debug";
const char* mqtt_server = "192.168.10.104";

long lastReconnectAttempt;

int current_steps = CENTER_STEPS;
int target_steps = CENTER_STEPS;

String tmp_str; // String for publishing the int's as a string to MQTT
char buf[5];

WiFiClient espClient;
PubSubClient client(espClient);

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

  // MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if (!client.connected()) {
    reconnect();
  }
  tmp_str = String(current_steps); //converting temperature to a string
  tmp_str.toCharArray(buf, tmp_str.length() + 1);
  client.publish(debug_topic, buf);
}

void loop()
{
  if (client.connected())
  {
    // Client connected
    client.loop();
    httpServer.handleClient();    // for HTTPupdate
  }
  else
    // Client is not connected
  {
    long now = millis();

    if (now - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
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
        client.loop();
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
        client.loop();
        digitalWrite(stepPin, HIGH);
        delayMicroseconds(STEP_DELAY);
        digitalWrite(stepPin, LOW);
        delayMicroseconds(STEP_DELAY);
        current_steps++;
      }
    }
    digitalWrite(powerPin, HIGH); // disable the stepper motor
    client.publish(debug_topic, "current steps = target steps: ");
    tmp_str = String(current_steps); //converting temperature to a string
    tmp_str.toCharArray(buf, tmp_str.length() + 1);
    client.publish(debug_topic, buf);
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

void callback(char* topic, byte * payload, unsigned int length) { //payload is position in percent
  String stringOne = (char*)payload;
  //String temp_s = stringOne.substring(0, length);
  int temp = stringOne.toInt();
  target_steps = (float)temp / (float)100 * MAX_STEPS; // from percentage to STEPS
  client.publish(debug_topic, "target steps: ");
  tmp_str = String(target_steps); //converting temperature to a string
  tmp_str.toCharArray(buf, tmp_str.length() + 1);
  client.publish(debug_topic, buf);
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  if (client.connect(mqtt_id)) {
    // ... and resubscribe
    client.subscribe(percentage_topic);
  }
  client.publish(debug_topic, "blinds control side connected");
  Serial.println(client.connected());
  return client.connected();
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
