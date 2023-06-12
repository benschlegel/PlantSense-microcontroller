#include <ESPmDNS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define BUTTON_PIN 0
#define GREEN_PIN 27
#define BLUE_PIN 14
#define RED_PIN 12
#define FULL_BRIGHTNESS 1

#define SERVER_URL "http://192.168.141.24"

#define BUTTON_PRESS_DELAY 200

#define WIFI_SSID "Ben's Hotspot"
#define WIFI_PASSWORD "pword1234"

#define WIFI_AP_SSID "PlantSense - Planty"
#define WIFI_AP_PASSWORD "QPLAF3JN2an"

#define HOST_PREFIX "plantsense_"

#define WIFI_CONNECTION_ATTEMPT_SECONDS 5

#define BUILT_IN_LED_PIN 2

// Define Preferences
Preferences preferences;

// Access point setup
const IPAddress IP = {192, 168, 111, 1};
const IPAddress gateway = IPAddress(192, 168, 111, 1);
const IPAddress NMask = IPAddress(255, 255, 255, 0);

// Global variables
WebServer server(80);  // Object of WebServer(HTTP port, 80 is default)
StaticJsonDocument<250> receivedRgbJson;
StaticJsonDocument<200> multiArgJson;;
StaticJsonDocument<100> singleArgJson;
StaticJsonDocument<100> stateJson;

// Server host address
String serverHost;
const String serverPrefix = "/v1/mc";
const String test = "esp32";

// Wifi credentials
String ssid;
String password;

// LED colors
int led_red = 255;
int led_green = 0;
int led_blue = 255;

// LED settings
int counting = 1; //used for breathing effect, increases intensity if 1, decreases if 0
bool isBreathing = true;
double intensity = 0; // between 0.0 and 1 ("brightness percentage")

// Long press config
bool buttonActive = false;
bool longPressActive = false;

long buttonTimer = 0;
long longPressTime = 500;

// Also used for WiFi access point,
String device_name = "PlantSense - Planty";

void setup() {
  Serial.begin(115200);

  // Get host from preferences
  // setServerHostPreference("https://plantsense.global.rwutscher.com");
  serverHost = getServerHostPreference() + serverPrefix;
  Serial.println(serverHost);
  // setCredentialPreferences(WIFI_SSID, WIFI_PASSWORD);
  ssid = getSSIDPreference();
  password = getPasswordPreference();

  if (ssid == "" || password == "") {
    Serial.println("No wifi credentials were found.");
  }

  // Set color of LED during setup
  setColor(0,0,255,1);
  bool isAP = initWiFiNew();

  if (!isAP) {
    // If connection to wifi was successful, call server and register device
    bool isServerReachable = registerDevice();

    // If server is not reachable, go back to init mode
    Serial.println(WiFi.gatewayIP());
    if (!isServerReachable) {
      initAP();
    } else {
      // WiFi.setHostname(test.c_str());
      SetUpMDNS();
    }
  }

  //Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  server.on("/", handle_root);
  server.on("/heartbeat", handle_heartbeat);
  server.on("/led", HTTP_POST, handle_setLed);
  server.on("/setState", HTTP_POST, handle_setState);
  server.on("/toggleState", HTTP_POST, handle_toggleState);
  server.on("/setHost", HTTP_POST, handle_setHostAddress);
  server.on("/setCredentials", HTTP_POST, handle_setCredentials);
  server.on("/name", HTTP_GET, handle_getName);
  server.on("/name", HTTP_POST, handle_setName);

  server.begin();
  Serial.println("HTTP server started!");

  // Set CA Cert for https (or setInsecure to not check for cert)
  // secureClient->setInsecure();
  delay(100);
}

void loop() {
  // If button is pressed, switch between modes/send notification
  handleButtonPress();

  // Use breathing effect or full brightness
  if(isBreathing) {
    setColor(led_red,led_green,led_blue,intensity);
  } else {
    setColor(led_red,led_green,led_blue,FULL_BRIGHTNESS);
  }

  if(counting){
    intensity += 0.01;
  } else {
    intensity -= 0.01;
  }

  if(intensity >= 1 || intensity <= 0) {
    counting = !counting;
  }
  delay(10);

  server.handleClient();
}

// Invert bc gate is open by default
// Alpha controls brightness (percentage between 0.0 and 1)
void setColor(int red, int green, int blue, double alpha) {
  analogWrite(RED_PIN,   255-(red   * alpha));
  analogWrite(GREEN_PIN, 255-(green * alpha));
  analogWrite(BLUE_PIN,  255-(blue  * alpha));
}

bool initWiFiNew() {
  // First, connect as station
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Check if connection to wifi is possible
  int i;
  for (i = 0; i < WIFI_CONNECTION_ATTEMPT_SECONDS; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(1000);
  }

  if (i < WIFI_CONNECTION_ATTEMPT_SECONDS) {
    // If connection via wifi is already successful, return early;
    Serial.println("Connected to wifi!");
    // Serial.println("Gateway ip: " + WiFi.gatewayIP());
    return false;
  }

  // Open access point to configure device, if connection was not successful
  WiFi.disconnect();
  initAP();
  return true;
}

void initAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  delay(500);

  WiFi.softAPConfig(IP, IP, NMask);
}

void SetUpMDNS() {
  String mac = WiFi.macAddress();
  mac.replace(":", "-");
  String deviceHost = HOST_PREFIX + mac;
  if (MDNS.begin(deviceHost)) {
    Serial.println(F("mDNS responder started"));
    Serial.print(F("I am: "));
    Serial.println(deviceHost);

    // Add service to MDNS-SD
    MDNS.addService(deviceHost, "tcp", 80);
  } else {
    while (1) {
      Serial.println(F("Error setting up MDNS responder"));
      delay(1000);
    }
  }
}

// Gets host preference from storage
String getServerHostPreference() {
  preferences.begin("serverInfo", false);
  String host = preferences.getString("host", "");
  preferences.end();
  return host;
}

String getDeviceNamePreference() {
  preferences.begin("deviceInfo", false);
  String deviceName = preferences.getString("name", "");
  preferences.end();
  return deviceName;
}

void setDeviceNamePreference(String name) {
  preferences.begin("deviceInfo", false);
  preferences.putString("name", name);
  preferences.end();
}

// Set host preference
void setServerHostPreference(String host) {
  preferences.begin("serverInfo", false);
  preferences.putString("host", host);
  preferences.end();
}

// Set wifi preferences
void setCredentialPreferences(String ssid, String password) {
  preferences.begin("credentials", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
}

void setWifiPasswordPreference(String password) {
  preferences.begin("credentials", false);
  preferences.putString("password", password);
  preferences.end();
}

void setWifiSSIDPreference(String ssid) {
  preferences.begin("credentials", false);
  preferences.putString("ssid", ssid);
  preferences.end();
}

String getSSIDPreference() {
  preferences.begin("credentials", false);
  String ssid = preferences.getString("ssid", "");
  preferences.end();
  return ssid;
}

String getPasswordPreference() {
  preferences.begin("credentials", false);
  String password = preferences.getString("password", "");
  preferences.end();
  return password;
}

bool registerDevice() {
  // Your Domain name with URL path or IP address with path
  // Important to prefix IP with "http://"
  // Only use "regular" url for https (when using secureClient raw without httpClient)
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;

    // Set up json payload with device name
    singleArgJson["deviceName"] = device_name;
    singleArgJson["localIP"] = WiFi.localIP();
    singleArgJson["mac"] = WiFi.macAddress();
    String jsonString;
    serializeJson(singleArgJson, jsonString);

    // Update with new IP, if it changes
    http.begin(serverHost + "/registerDevice");
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(jsonString);                                        //Make the request

    if (httpCode > 0) { //Check for the returning code
        Serial.println("Device registered on server successfully.");
        return true;
    } else {
      Serial.println("Server unreachable.");
      return false;
    }

    http.end(); //Free the resources
  }
}

void sendNotification() {
  // Your Domain name with URL path or IP address with path
  // Important to prefix IP with "http://"
  // Only use "regular" url for https (when using secureClient raw without httpClient)
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http;

    // Set up json payload with device name
    singleArgJson["name"] = device_name;
    String jsonString;
    serializeJson(singleArgJson, jsonString);

    // Update with new IP, if it changes
    http.begin(serverHost + "/sendNotification");
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(jsonString);                                        //Make the request

    if (httpCode > 0) { //Check for the returning code
        String payload = http.getString();
        Serial.println(httpCode);
        Serial.println(payload);
      }

    else {
      Serial.println("Error on HTTP request");
    }

    http.end(); //Free the resources
  }
}

void handleButtonPress() {
  if (!digitalRead(BUTTON_PIN)) { // Button is pressed

    if (buttonActive == false) {
      buttonActive = true;
      buttonTimer = millis();
    }

    if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) { //long press start
      longPressActive = true;
    }
    // Delay to not register 2 button presses within 150ms
    delay(150);
  } else {
    if (buttonActive == true) {

      if (longPressActive == true) { // long press release
        longPressActive = false;
        Serial.println("Long press");
      } else { //short press release
        Serial.println("Short press");
        sendNotification();
      }
      buttonActive = false;
    }
  }
}

// Handle root url (/)
void handle_root() {
  Serial.println("RECEIVED REQUEST ('/')");
  server.send(200, "text", "HELLO FROM ESP32");
}

// Handle heartbeat ("/heartbeat")
void handle_heartbeat() {
  server.send(200);
}

// Handle POST ("/led")
void handle_setLed() {
  // Ideally, update this to "application/json", but no idea how
  String body = server.arg("plain");
  deserializeJson(receivedRgbJson, body);

  // Get values from payload
  int red_value = receivedRgbJson["red"];
  int green_value = receivedRgbJson["green"];
  int blue_value = receivedRgbJson["blue"];

  // Set new led colors (will automatically be used in next iteration loop)
  led_red = red_value;
  led_green = green_value;
  led_blue = blue_value;

  server.send(200);
}

// Handle POST ("/setState")
void handle_setState() {
  // Ideally, update this to "application/json", but no idea how
  String body = server.arg("plain");
  deserializeJson(singleArgJson, body);

  // Get values from payload
  bool isSolid = singleArgJson["isSolid"];

  // Set new value
  isBreathing = !isSolid;

  // Send response
  server.send(200);
}

// Handle POST ("/setCredentials")
void handle_setCredentials() {
  String body = server.arg("plain");
  deserializeJson(multiArgJson, body);

  if (multiArgJson.containsKey("ssid")) {
    // Get value from payload
    String ssid = multiArgJson["ssid"];
    // TODO: switch to credential function
    setWifiSSIDPreference(ssid);
  }

  if (multiArgJson.containsKey("password")) {
    // Get value from payload
    String password = multiArgJson["password"];
    setWifiPasswordPreference(password);
  }

  // Send response
  server.send(200);
}

// Handle POST ("/setHost")
void handle_setHostAddress() {
  String body = server.arg("plain");
  deserializeJson(multiArgJson, body);

  if (multiArgJson.containsKey("host")) {
    // Get value from payload
    String host = multiArgJson["host"];
    setServerHostPreference(host);
  }

  // Send response
  server.send(200);
}

// Handle POST ("/toggleState")
void handle_toggleState() {
  // Toggle state
  isBreathing = !isBreathing;

  // Send response
  server.send(200);
}

void handle_setName() {
  String body = server.arg("plain");
  deserializeJson(multiArgJson, body);

  if (multiArgJson.containsKey("name")) {
    // Get value from payload
    String name = multiArgJson["name"];
    Serial.println("Name from json: " + name);
    setDeviceNamePreference(name);
  }

  // Send response
  server.send(200);
}

void handle_getName() {
  StaticJsonDocument<100> nameJson;
  String prefName = getDeviceNamePreference();
  Serial.println("Pref name: " + prefName);
  nameJson["name"] = prefName;
  String jsonString;
  serializeJson(nameJson, jsonString);
  // server.sendContent("test");
  server.send(200, "text", jsonString);
}
