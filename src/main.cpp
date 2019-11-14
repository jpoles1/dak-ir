#include <string>
using namespace std;

#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <ArduinoWebsockets.h>
using namespace websockets;
#include <HTTPClient.h>

//Private Config
#include "config.h"

// Pins
#define IR_LED_PIN 27
#define IR_RECV_PIN 13

// Global Variables
WebsocketsClient wsClient;
WiFiClient client;
Servo switchServo;
int isr_flag = 0;

int pos = 0;
const int offPosition = 10;
const int neutralPosition = 60;
const int onPosition = 100;

std::vector<String> splitStringToVector(String msg, char delim){
  std::vector<String> subStrings;
  int j=0;
  for(int i =0; i < msg.length(); i++){
    if(msg.charAt(i) == delim){
      subStrings.push_back(msg.substring(j,i));
      j = i+1;
    }
  }
  subStrings.push_back(msg.substring(j,msg.length())); //to grab the last value of the string
  return subStrings;
}

void httpRequest(String apiURL){
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(apiURL);
        Serial.println("GET - " + apiURL);
        int httpCode = http.GET();
        Serial.println("Response Code - " + httpCode);
        if(httpCode != 200){
            //digitalWrite(ERROR_LED_PIN, HIGH);
            String payload = http.getString();
            Serial.println("GET Error: " + payload);
        }
        else {
            //digitalWrite(ERROR_LED_PIN, LOW);
        }
    }
    else {
        Serial.println("Disconnected, cannot send request!");
    }
}

void deviceCommandRequest(String device, String fname, String val){
    String reqURL = String(SERVER_URL) + "?room=" + SERVER_ROOMNAME + "&key=" + SERVER_KEY + "&device=" + device + "&fname=" + fname + "&val=" + val;
    httpRequest(reqURL);
}

//Wifi Util Functions
void wifiCheck(){
    if(WiFi.status() != WL_CONNECTED){
        Serial.println("Connecting to WiFi...");
    }
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        counter++;
        if(counter>= 15 * 60){ //after 15 min timeout - reset board
            ESP.restart();
        }
    }
}
void onMessage(WebsocketsMessage msg) {
    if (msg.data() == "ping") {
        //webSocketClient.sendData("pong");
    } else {
        std::vector<String> split_string = splitStringToVector(msg.data().c_str(), ':');
        String msgType = split_string[0];
        String deviceName = split_string[1];
        String fname = split_string[2];
        String command = split_string[3];
        if(deviceName == "computer" && fname == "power"){
            if(command == "off"){
                //switchOff();
            }
            if(command == "on"){
                //switchOn();
            }
        }
    }
}
void websocketLoad() {
    wifiCheck();
    String wsURL = String("/sync?room=") + SERVER_ROOMNAME + String("&key=") + SERVER_KEY;
    if (wsClient.connect(WEBSOCKET_URL, 80, wsURL)) {
        Serial.println("WS connected!");
    } else {
        Serial.println("WS connection failed!");
    }
    wsClient.onMessage(onMessage);
}

void sendPowerCommands(String powerSetting) {
    deviceCommandRequest("1", "power", powerSetting);
    deviceCommandRequest("2", "power", powerSetting);
    deviceCommandRequest("3", "power", powerSetting);
    deviceCommandRequest("4", "power", powerSetting);
}

void setup() {
    Serial.begin(115200);
    //Setup wifi
	delay(10);
    //uint8_t new_mac[8] = {0x39,0xAE,0xA4,0x16,0x1F,0x08};
    //esp_base_mac_addr_set(new_mac);
    WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
    WiFi.mode(WIFI_STA); //init wifi mode
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username --> identity and username is same
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config settings to default
    esp_wifi_sta_wpa2_ent_enable(&config); //set config settings to enable function
    WiFi.begin(AP_SSID);
    //WiFi.setHostname("ESPDAK");
    websocketLoad();
}

void loop() {
    if(WiFi.status() == WL_CONNECTED){
        if (wsClient.available()) {
            wsClient.poll();
        } else {
            websocketLoad();
        }
    } else {
        Serial.println("WiFi disconnected!");
        wifiCheck();
    }
    delay(200);
}