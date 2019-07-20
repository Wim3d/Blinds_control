/*     
      Blinds control
      Written by W. Hoogervorst
      july 2019
      Quarter steps
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <credentials.h>

#define OPEN_STEPS 400
#define CENTER_STEPS 2000
#define CLOSE_STEPS 3600
#define MAX_STEPS 4000
#define STEP_DELAY 1000

// for HTTPupdate and webserver
const char* host = "Blinds_control_side";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 4";

/*credentials & definitions */
//MQTT
const char* mqtt_id = "Blinds_Control_side";
const char* percentage_topic = "blinds/side/percentage";
const char* debug_topic = "blinds/side/debug";

long lastReconnectAttempt;

int current_steps = CENTER_STEPS;
int target_steps = CENTER_STEPS;

String tmp_str; // String for publishing the int's as a string to MQTT
char buf[5];

WiFiClient espClient;
PubSubClient client(espClient);

// defines pins numbers
const int stepPin = 3;  // GPIO3
const int dirPin = 1;   // GPIO1
const int powerPin = 2; // GPIO2

void setup() {
  // Sets the two pins as Outputs
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(powerPin, OUTPUT);

  digitalWrite(powerPin, HIGH); // disable the stepper motor
  setup_wifi();
  digitalWrite(powerPin, HIGH); // disable the stepper motor
  // for HTTPudate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);
  httpServer.on("/", handleRoot);
  httpServer.on("/center", handle_center);
  httpServer.on("/open", handle_open);
  httpServer.on("/close", handle_close);
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
  // handle MQTT messages and HTTP requests
  if (client.connected())
  {
    // Client connected
    client.loop();
    httpServer.handleClient();    // for HTTPupdate
  }
  else
    // Client is not connected, reconnect
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
  // handle the blinds position
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
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void callback(char* topic, byte * payload, unsigned int length) { //payload is position in percentage
  String stringOne = (char*)payload;
  //String temp_s = stringOne.substring(0, length);
  int temp = stringOne.toInt();
  target_steps = (float)temp / (float)100 * MAX_STEPS; // from percentage to STEPS
  client.publish(debug_topic, "target steps: "); // for debuging
  tmp_str = String(target_steps); //converting value to a string
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

void handle_open() {
  target_steps = OPEN_STEPS;
  httpServer.send(200, "text/html", SendHTML());
}

void handle_close() {
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
  ptr += mqtt_id;
  ptr += "</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 25px auto 30px;} h3 {color: #444444;margin-bottom: 30px;}\n";
  ptr += ".button {width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px 10px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-1 {background-color: #34495e;}\n";
  ptr += ".button-1:active {background-color: #2c3e50;}\n";
  ptr += ".button-update {background-color: #a32267;}\n";
  ptr += ".button-update:active {background-color: #961f5f;}\n";
  ptr += "p {font-size: 18px;color: #383535;margin-bottom: 15px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Blinds side Web Control</h1>\n";
  ptr += "<h3>Control Blinds and link to HTTPWebUpdate</h3>\n";
  ptr += "<p>WimIOT\nDevice: ";
  ptr += mqtt_id;
  ptr += "<br>Software version: ";
  ptr += software_version;
  ptr += "<br><br></p>";

  ptr += "<a class=\"button button-1\" href=\"/open\">Open</a>&nbsp;\n";
  ptr += "<a class=\"button button-1\" href=\"/center\">Center</a>&nbsp;\n";
  ptr += "<a class=\"button button-1\" href=\"/close\">Close</a><br><br><br>\n";

  ptr += "<p>Click for update page</p><a class=\"button button-update\" href=\"/pre_update\">Update</a>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
