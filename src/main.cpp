#include <DHT.h> // DHT-22 tenp/humidity sensor library
#include <WiFi.h>
#include <WebServer.h>
#include <Credentials.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

#define DHTPIN 42     // Pin where the DHT22 is connected
#define DHTTYPE DHT22 // DHT 22 (AM2302), AM2321

#define deviceName "Hydro-Unit" // Name of the device
#define accessToken ACCESS_TOKEN

#define NUM_LEDS 1
#define DATA_PIN 48

CRGB leds[NUM_LEDS];          // Define LED for FastLED. I don't know how to define a single one without the array.
CRGB lastColor = CRGB::Black; // use CRGB::Black for no color. Don't use FastLED.clear() to turn off the LED

DHT dht(DHTPIN, DHTTYPE);

WebServer server(80); // Create a web server object on port 80

unsigned long lastTime = 0; // Store the last time the data was sent

float temperature = 0; // Store the temperature value
float humidity = 0;    // Store the humidity value

float temp_calibration = -2.0; // Store the temperature calibration value

const char *ssid = MY_WIFI_SSID;
const char *password = MY_WIFI_PASS;

void handleDataRequest()
{
  if (!server.hasArg("token") || server.arg("token") != accessToken)
  {
    Serial.println("Unauthorized access attempt.");
    server.send(403, "text/plain", "Forbidden");
    return;
  }

  long now = millis();                                          // Get the current time
  float temperature = dht.readTemperature() + temp_calibration; // Read temperature in Celsius
  float humidity = dht.readHumidity();                          // Read humidity

  if (isnan(temperature) || isnan(humidity))
  { // Check if the readings are valid
    Serial.println("Failed to read from DHT sensor!");
    server.send(500, "text/plain", "Failed to read from DHT sensor!"); // Send error response
    return;
  }
  else
  {
    JsonDocument doc;
    doc["device_name"] = deviceName;  // Add device name to JSON
    doc["temperature"] = temperature; // Add temperature to JSON
    doc["temp_unit"] = "C";           // Add temperature unit to JSON
    doc["humidity"] = humidity;       // Add humidity to JSON

    String jsonData;              // Create a string to hold the JSON data
    serializeJson(doc, jsonData); // Serialize the JSON document to a string
    Serial.println(jsonData);     // Print the JSON data to the serial monitor
    Serial.println("Sending data to server...");
    server.send(200, "application/json", jsonData); // Send the JSON data to the client
    Serial.println("Data sent to server.");
    Serial.println("Request time taken: " + String(millis() - now) + " ms"); // Print the time taken to send the request
    Serial.println("Waiting for next request...");
    return;
  }
}

void setup()
{
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(4);
  leds[0] = CRGB::Yellow; // Shows Yellow while booting to check for hanging
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    leds[0] = CRGB::Red; // Helps show connection problems when not on serial monitor
    FastLED.show();
    delay(5000);
  }

  dht.begin(); // Initialize DHT sensor

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin(deviceName); // Start mDNS service

  server.on("/data", HTTP_GET, handleDataRequest); // Handle GET request to "/data"
  server.begin();                                  // Start the server
  Serial.println("HTTP server started");
  Serial.println("Waiting for requests...");
  leds[0] = CRGB::Green; // Blink 2 times fast when ready
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Black; // Set LED to black (off)
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Black; // Set LED to black (off)
  FastLED.show();
}

void loop()
{
  server.handleClient(); // Handle incoming client requests
}