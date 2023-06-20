#include <ESPmDNS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
// #include "preferences_access.ino"
#include <Preferences.h>

// Change this for every device
#define DEFAULT_DEVICE_NAME "PlantSense - Planty"

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

#define WIFI_CONNECTION_ATTEMPT_SECONDS 10

const String DEFAULT_SERVER_HOST = "https://plantsense.global.rwutscher.com";

#define BUILT_IN_LED_PIN 2

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

// Define Preferences
Preferences preferences;

// Server host address
String serverHost;
String deviceHost;
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
bool isSetupMode = true;
double intensity = 0; // between 0.0 and 1 ("brightness percentage")

// Long press config
bool buttonActive = false;
bool longPressActive = false;

long buttonTimer = 0;
long longPressTime = 500;

// Also used for WiFi access point,
String device_name;

void setup() {
  Serial.begin(115200);

  // Get host from preferences
  // setServerHostPreference("https://plantsense.global.rwutscher.com");
  // setServerHostPreference("http://192.168.1.208");
  serverHost = getServerHostPreference() + serverPrefix;
  Serial.println("Server host: " + serverHost);
  // setCredentialPreferences(WIFI_SSID, WIFI_PASSWORD);
  ssid = getSSIDPreference();
  password = getPasswordPreference();

  if (ssid == "" || password == "") {
    Serial.println("No wifi credentials were found.");
  }

  setDeviceHost();
  Serial.println("Device host: " + deviceHost);

  // Get or set deviceName (get if available, set if not)
  handleDeviceName();
  delay(150);

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
      Serial.println("Activated AP");
    } else {
      // WiFi.setHostname(test.c_str());
      SetUpMDNS();
      // scanNetworks();
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
  server.on("/tryCredentials", HTTP_POST, handle_tryCredentials);
  server.on("/deviceInfo", HTTP_GET, handle_getInfo);
  server.on("/deviceInfo", HTTP_POST, handle_setInfo);
  server.on("/setupComplete", HTTP_GET, handle_getSetupComplete);
  server.on("/setupComplete", HTTP_POST, handle_setSetupComplete);
  server.on("/networks", HTTP_GET, handle_getNetworks);

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
  if(!isSetupMode) {
    if(isBreathing ) {
      setColor(led_red,led_green,led_blue,intensity);
    } else {
      setColor(led_red,led_green,led_blue,FULL_BRIGHTNESS);
    }
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
    isSetupMode = false;
    return false;
  }

  // Open access point to configure device, if connection was not successful
  WiFi.disconnect();
  initAP();
  return true;
}

/**
 * Initialize with AP_STA (ap + station)
*/
void initAP() {
  // Set color to indicate setup mode
  isSetupMode = true;
  setColor(0,255,255, FULL_BRIGHTNESS);
  WiFi.mode(WIFI_AP_STA);
  Serial.println("Wifi now in AP_STA mode.");
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  delay(500);

  WiFi.softAPConfig(IP, IP, NMask);
}

void setDeviceHost() {
  String mac = WiFi.macAddress();
  mac.replace(":", "-");
  String host = HOST_PREFIX + mac;
  deviceHost = host;
}

void SetUpMDNS() {
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

bool isServerReachable() {
  HTTPClient http;
  http.begin(serverHost);
  int resCode = http.GET();
  if (resCode > 0) {
    return true;
  } else {
    return false;
  }
}

bool testCredentials(String ssid, String password) {
  WiFi.disconnect();
  // WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  // Check if connection to wifi is possible
  int i;
  bool isConnected = false;
  for (i = 0; i < WIFI_CONNECTION_ATTEMPT_SECONDS; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Successfully connected");
      isConnected = true;
      break;
    }
    delay(1000);
  }

  if(isConnected) { // If wifi is connected, test if server is reachable
    // bool isReachable = isServerReachable();
    // return isReachable;
    return true;
  } else {
    Serial.println("Connection was unsuccessful.");
    return false;
  }
}

void scanNetworks() {
  // WiFi.scanNetworks will return the number of networks found
  Serial.println("Starting scan...");
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
}

bool registerDevice() {
  // Your Domain name with URL path or IP address with path
  // Important to prefix IP with "http://"
  // Only use "regular" url for https (when using secureClient raw without httpClient)
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    StaticJsonDocument<100> registerJson;
    delay(50);
    HTTPClient http;

    Serial.println("Registering with host: " + deviceHost);
    Serial.println("Registering on host: " + serverHost);

    // Set up json payload with device name
    registerJson["deviceName"] = device_name;
    registerJson["host"] = deviceHost;
    String jsonString;
    serializeJson(registerJson, jsonString);

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
    singleArgJson["host"] = deviceHost;
    String jsonString;
    serializeJson(singleArgJson, jsonString);
    Serial.println("Sending notification with: " + serverHost);

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
        clearWifiPreferences();
        clearServerHostPreference();
        initAP();
      } else { //short press release
        Serial.println("Short press");
        sendNotification();
      }
      buttonActive = false;
    }
  }
}

void handleDeviceName() {
  String name = getDeviceNamePreference();
  if(name == "") {
    device_name = DEFAULT_DEVICE_NAME;
    Serial.println("New device name set: " + device_name);
  } else {
    Serial.println("Device name existed: " + name);
    device_name = name;
  }
}

/**
 *
 *
 * Preferences
 *
 *
*/

// Gets host preference from storage
String getServerHostPreference() {
  preferences.begin("serverInfo", false);
  String host = preferences.getString("host", DEFAULT_SERVER_HOST);
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

bool getDeviceSetupPreference() {
  preferences.begin("deviceInfo", false);
  bool isSetupComplete = preferences.getBool("setup", false);
  preferences.end();
  return isSetupComplete;
}

void setDeviceSetupPreference(bool isSetupComplete) {
  preferences.begin("deviceInfo", false);
  preferences.putBool("setup", isSetupComplete);
  preferences.end();
}

// Set host preference
void setServerHostPreference(String host) {
  preferences.begin("serverInfo", false);
  preferences.putString("host", host);
  preferences.end();
}

void clearServerHostPreference() {
  preferences.begin("serverInfo", false);
  preferences.clear();
  preferences.end();
  serverHost = DEFAULT_SERVER_HOST + serverPrefix;
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

void clearWifiPreferences() {
  preferences.begin("credentials", false);
  preferences.clear();
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

/**
 *
 *
 * HTTP endpoints
 *
 *
*/

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

void handle_setInfo() {
  String body = server.arg("plain");
  deserializeJson(multiArgJson, body);

  if (multiArgJson.containsKey("deviceName")) {
    // Get value from payload
    String deviceName = multiArgJson["deviceName"];
    device_name = deviceName;
    setDeviceNamePreference(deviceName);
  }

  // Send response
  server.send(200);
}

void handle_getInfo() {
  StaticJsonDocument<100> infoJson;
  infoJson["deviceName"] = device_name;
  infoJson["host"] = deviceHost;
  Serial.println("HOST: " + deviceHost);
  String jsonString;
  serializeJson(infoJson, jsonString);
  server.send(200, "text", jsonString);
}

void handle_getSetupComplete() {
  Serial.println("Received request setup");
  StaticJsonDocument<100> setupJson;
  bool isSetupComplete = getDeviceSetupPreference();
  setupJson["isComplete"] = isSetupComplete;
  String jsonString;
  serializeJson(setupJson, jsonString);
  server.send(200, "text", jsonString);
}


void handle_setSetupComplete() {
  String body = server.arg("plain");
  deserializeJson(singleArgJson, body);

  if (singleArgJson.containsKey("isComplete")) {
    // Get value from payload
    bool isComplete = singleArgJson["isComplete"];
    setDeviceSetupPreference(isComplete);
  }

  // Send response
  server.send(200);
}

void handle_getNetworks() {
  StaticJsonDocument<250> setupJson;
  // WiFi.scanNetworks will return the number of networks found
  // Returns number of found networks
  int n = WiFi.scanNetworks();

  // Make own json from string
  String networks = "[";
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    String entry;
    for (int i = 0; i < n; ++i) {
      entry = "{\"ssid\":\"";
      entry += WiFi.SSID(i);
      entry += "\",\"isEncrypted\":";
      entry += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
      entry += "}";
      networks += entry;
      if (i != n - 1) {
        networks += ",";
      }
    }
  }
  networks += "]";
  server.send(200, "text", networks);
}

void handle_tryCredentials() {
  String body = server.arg("plain");
  deserializeJson(singleArgJson, body);

  if (singleArgJson.containsKey("ssid") && singleArgJson.containsKey("password")) {
    // Get value from payload
    String ssid = singleArgJson["ssid"];
    String password = singleArgJson["password"];


    Serial.println("Ssid: " + ssid);
    Serial.println("pw: " + password);

    // Test credentials
    bool isSuccessful = testCredentials(ssid, password);

    if (isSuccessful) {
      Serial.println("Wifi now in station mode.");
      StaticJsonDocument<200> responseJson;
      responseJson["isValid"] = true;
      responseJson["host"] = deviceHost;
      responseJson["deviceName"] = device_name;

      String jsonString;
      serializeJson(responseJson, jsonString);

      server.send(200, "text", jsonString);

      // add delay so response can be sent out before wifi gets shut down
      delay(300);
      // After response was sent, switch over to station wifi, set up mdns and save credentials
      setCredentialPreferences(ssid, password);
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_MODE_STA);
      SetUpMDNS();
      isSetupMode = false;

      // TODO: set serverHost Preference

      // Register device on server
      registerDevice();
    } else {
      server.send(200, "text", "{\"isValid\": false}");
    }
  } else {
    server.send(400);
  }
}
