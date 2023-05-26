#include <ArduinoMqttClient.h>
#if defined(ARDUINO_SAMD_MKRWIFI1010) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_AVR_UNO_WIFI_REV2)
  #include <WiFiNINA.h>
#elif defined(ARDUINO_SAMD_MKR1000)
  #include <WiFi101.h>
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ARDUINO_PORTENTA_H7_M7) || defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_GIGA)
  #include <WiFi.h>
#endif

#include "arduino_secrets.h"
#include <Adafruit_Sensor.h>
#include <utility/wifi_drv.h>
#include "DHT.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
char username[] = HIVEMQ_USERNAME;
char password[] = HIVEMQ_PASSWORD;

WiFiSSLClient wifiClient;
MqttClient mqttClient(wifiClient);
DHT dht(1, DHT11);

void onMqttMessage(int messageSize);

const char broker[]    = BROKER;
int        port        = 8883;
const char willTopic[] = "home/will";
const char outTopic[]   = "telemetry/home/led";
const char inTopic[]  = "telemetry/home/living-room";

const long interval = 60000;
unsigned long previousMillis = 0;

int count = 0;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  
  pinMode(LED_BUILTIN, OUTPUT);
  dht.begin();
  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);

  WiFiDrv::analogWrite(25, 0);
  WiFiDrv::analogWrite(26, 255);
  WiFiDrv::analogWrite(27, 0);

  // attempt to connect to WiFi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    WiFiDrv::analogWrite(25, 255);
    WiFiDrv::analogWrite(26, 255);
    WiFiDrv::analogWrite(27, 0);
    // failed, retry
    Serial.print(".");
    delay(5000);
  }

  Serial.println("You're connected to the network");
  Serial.println();

  mqttClient.setId("MKRWiFi1010Client");
  mqttClient.setUsernamePassword(username, password);
  mqttClient.setCleanSession(false);

  String willPayload = "oh no!";
  bool willRetain = true;
  int willQos = 1;

  mqttClient.beginWill(willTopic, willPayload.length(), willRetain, willQos);
  mqttClient.print(willPayload);
  mqttClient.endWill();

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    WiFiDrv::analogWrite(25, 255);
    WiFiDrv::analogWrite(26, 255);
    WiFiDrv::analogWrite(27, 0);
    while (1);
  } else {
    WiFiDrv::analogWrite(25, 255);
    WiFiDrv::analogWrite(26, 0);
    WiFiDrv::analogWrite(27, 0);
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();
  
  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Subscribing to topic: ");
  Serial.println(outTopic);
  Serial.println();

  int subscribeQos = 1;

  mqttClient.subscribe(outTopic, subscribeQos);

  Serial.print("Waiting for messages on topic: ");
  Serial.println(outTopic);
  Serial.println();
  delay(2000);
  WiFiDrv::analogWrite(25, 255);
  WiFiDrv::analogWrite(26, 0);
  WiFiDrv::analogWrite(27, 0);
}

void loop() {
  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();

  unsigned long currentMillis = millis();
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    String payload;

    payload += "{\n";
    payload += "  \"Temperature\": ";
    payload += temp;
    payload += ",\n";
    payload += "  \"Humidity\": ";
    payload += humidity;
    payload += "\n";
    payload += "}";

    Serial.print("Sending message to topic: ");
    Serial.println(inTopic);
    Serial.print(payload);
    Serial.println(count);

    // send message, the Print interface can be used to set the message contents
    // in this case we know the size ahead of time, so the message payload can be streamed

    bool retained = false;
    int qos = 1;
    bool dup = false;

    mqttClient.beginMessage(inTopic, payload.length(), retained, qos, dup);
    mqttClient.print(payload);
    mqttClient.endMessage();

    Serial.println();

    count++;
  }
}

void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', duplicate = ");
  Serial.print(mqttClient.messageDup() ? "true" : "false");
  Serial.print(", QoS = ");
  Serial.print(mqttClient.messageQoS());
  Serial.print(", retained = ");
  Serial.print(mqttClient.messageRetain() ? "true" : "false");
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  String incPayload;

  // use the Stream interface to print the contents
  while (mqttClient.available()) {
    incPayload = mqttClient.readString();
    if (incPayload == "HIGH") {
      digitalWrite(LED_BUILTIN, HIGH);
    } 
    else if (incPayload == "LOW") {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  Serial.println();

  Serial.println();
}
