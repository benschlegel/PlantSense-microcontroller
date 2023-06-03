#include <WiFi.h>
#include <HTTPClient.h> 
#include <WebServer.h>
#include <WiFiClientSecure.h>

#define BUTTON_PIN 0
#define GREEN_PIN 27
#define BLUE_PIN 14
#define RED_PIN 12
#define FULL_BRIGHTNESS 1

#define DEFAULT_COLOR_R 255
#define DEFAULT_COLOR_G 0
#define DEFAULT_COLOR_B 255

#define BUTTON_PRESS_DELAY 200

#define WIFI_SSID "Ben's Hotspot"
#define WIFI_PASSWORD "pword1234"

#define WIFI_AP_SSID "PlantSense - Planty"
#define WIFI_AP_PASSWORD "QPLAF3JN2an"

// Currently unused (because not valid), could be replaced in the future
const char* ROOT_CA= "-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----\n";

WebServer server(80);  // Object of WebServer(HTTP port, 80 is default)
WiFiClientSecure secureClient;

// Handle root url (/)
void handle_root() {
  server.send(200, "text", "HELLO FROM ESP32");
}

void setup() {
  Serial.begin(115200);
  setColor(0,0,255,1);
  initWiFi();

  //Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  server.on("/", handle_root);

  server.begin();
  Serial.println("HTTP server started");

  // Set CA Cert for https (or setInsecure to not check for cert)
  secureClient.setInsecure();
  delay(100); 
}

int counting = 1; //used for breathing effect
bool isBreathing = true;
double intensity = 0; // between 0.0 and 1 ("brightness percentage")

void loop() {
  // 1 is pressed, 0 is not pressed
  int isPressed = !digitalRead(BUTTON_PIN);

  // If button is pressed, switch between modes
  if(isPressed) {
    isBreathing = !isBreathing;
    // Add delay to not trigger button press multiple times
    delay(BUTTON_PRESS_DELAY);
    sendRequest();
  } 

  // Use breathing effect or full brightness
  if(isBreathing) {
    setColor(DEFAULT_COLOR_R,DEFAULT_COLOR_G,DEFAULT_COLOR_B,intensity);
  } else {
    setColor(DEFAULT_COLOR_R,DEFAULT_COLOR_G,DEFAULT_COLOR_B,FULL_BRIGHTNESS);
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
// Alpha controls brightness
void setColor(int red, int green, int blue, double alpha) {
  analogWrite(RED_PIN,   255-(red   * alpha));
  analogWrite(GREEN_PIN, 255-(green * alpha));
  analogWrite(BLUE_PIN,  255-(blue  * alpha));
}

void initWiFi() {
  WiFi.mode(WIFI_AP_STA);
  // Set up Access Point (WiFi created by esp)
  Serial.println("\n[*] Creating ESP32 AP");
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);  /*Configuring ESP32 access point SSID and password*/
  Serial.print("[+] AP Created with IP Gateway ");

  // Connect to predefined hotspot/network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void sendRequest() {
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  // Important to prefix IP with "http://"
  // Only use "regular" url for https (when using secureClient raw without httpClient)
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status

    HTTPClient http;

    http.begin("http://192.168.1.236"); //Specify the URL
    int httpCode = http.GET();                                        //Make the request

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