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
#define LIGHT_SENSE_PIN 18

CRGB leds[NUM_LEDS];          // Define LED for FastLED. I don't know how to define a single one without the array.
CRGB lastColor = CRGB::Black; // use CRGB::Black for no color. Don't use FastLED.clear() to turn off the LED

DHT dht(DHTPIN, DHTTYPE);

WebServer server(80); // Create a web server object on port 80

unsigned long lastTime = 0;    // Store the last time the data was sent
bool errorFlag = false;        // Flag to indicate if there was an error in the last request
float temp_calibration = -2.0; // Store the temperature calibration value

const char *ssid = MY_WIFI_SSID;
const char *password = MY_WIFI_PASS;

long lastRequestTime = 0; // Store the last request time

void blinkLED(CRGB color, int times, int delayMs)
{ // helper function to blink LED codes
  for (int i = 0; i < times; i++)
  {
    leds[0] = color;
    FastLED.show();
    delay(delayMs);
    leds[0] = CRGB::Black;
    FastLED.show();
    delay(delayMs);
  }
}

void handleSerialRequst()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim(); // clean up any whitespace

    if (command == "status")
    {
      Serial.println("----- SYSTEM STATUS -----");
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.isConnected() ? "Yes" : "No");

      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());

      Serial.print("Last request time: ");
      Serial.println(lastRequestTime);

      Serial.println("--------------------------");
    }
    else
    {
      Serial.println("Unknown command.");
    }
  }
}

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
  float lightSenseValue = analogRead(LIGHT_SENSE_PIN);          // Read light sensor value

  if (isnan(temperature) || isnan(humidity))
  { // Check if the readings are valid
    Serial.println("Failed to read from DHT sensor!");
    server.send(500, "text/plain", "Failed to read from DHT sensor!"); // Send error response
    errorFlag = true;                                                  // Set error flag
    return;
  }
  else if (isnan(lightSenseValue))
  {
    Serial.println("Failed to read from light sensor!");
    server.send(500, "text/plain", "Failed to read from light sensor!"); // Send error response
    errorFlag = true;                                                    // Set error flag
    return;
  }
  else
  {
    JsonDocument doc;
    doc["device_name"] = deviceName;                   // Add device name to JSON
    doc["temperature"] = temperature;                  // Add temperature to JSON
    doc["temp_unit"] = "C";                            // Add temperature unit to JSON
    doc["humidity"] = humidity;                        // Add humidity to JSON
    doc["light_sense_value"] = lightSenseValue / 4095; // Add light sensor value to JSON divided by 4095 for percentage
    doc["light_sense_unit"] = "%";                     // Add light sensor unit to JSON

    String jsonData;              // Create a string to hold the JSON data
    serializeJson(doc, jsonData); // Serialize the JSON document to a string
    Serial.println(jsonData);     // Print the JSON data to the serial monitor
    Serial.println("Sending data to server...");
    server.send(200, "application/json", jsonData); // Send the JSON data to the client
    Serial.println("Data sent to server.");
    Serial.println("Request time taken: " + String(millis() - now) + " ms"); // Print the time taken to send the request
    Serial.println("Waiting for next request...");
    lastRequestTime = millis(); // Update the last request time
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
  Serial.println("mDNS started @ " + String(deviceName) + ".local");

  server.on("/data", HTTP_GET, handleDataRequest); // Handle GET request to "/data"
  server.begin();                                  // Start the server
  Serial.println("HTTP server started");
  Serial.println("Waiting for requests...");
  blinkLED(CRGB::Green, 3, 200); // Blink green LED to indicate successful boot
}

void loop()
{
  server.handleClient(); // Handle incoming client requests
  handleSerialRequst();
  if (errorFlag)
  {
    blinkLED(CRGB::Red, 3, 200); // Blink red LED to indicate error
  }
}