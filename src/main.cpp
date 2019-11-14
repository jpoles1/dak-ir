#include <string>
using namespace std;

#include <Arduino.h>
#include <Servo.h>
#include <Wire.h>
#include <SparkFun_APDS9960.h>
#include <WiFi.h>
#include "esp_wpa2.h"
#include <ArduinoWebsockets.h>
using namespace websockets;
#include <HTTPClient.h>

//Private Config
#include "config.h"

// Pins
#define APDS9960_SDA 21
#define APDS9960_SCL 22
#define APDS9960_INT 23

#define SERVO_PIN 27

// Global Variables
SparkFun_APDS9960 apds = SparkFun_APDS9960();
WebsocketsClient wsClient;
WiFiClient client;
Servo switchServo;
int isr_flag = 0;

int pos = 0;
const int offPosition = 10;
const int neutralPosition = 60;
const int onPosition = 100;
void switchNeutral(){
    switchServo.attach(SERVO_PIN);
    switchServo.write(neutralPosition);
    delay(500);
    switchServo.detach();
}
void switchOn(){
    switchServo.attach(SERVO_PIN);
    for(pos = neutralPosition; pos < onPosition; pos += 1){
        switchServo.write(pos);
        delay(15);
    }
    for(pos = onPosition; pos>=neutralPosition; pos-=1){                                
        switchServo.write(pos);
        delay(15);
    } 
    switchServo.detach();
}
void switchOff(){
    switchServo.attach(SERVO_PIN);
    for(pos = neutralPosition; pos>=offPosition; pos-=1){                                
        switchServo.write(pos);
        delay(15);
    }
    for(pos = offPosition; pos < neutralPosition; pos += 1){
        switchServo.write(pos);
        delay(15);
    }
    switchServo.detach();
}

void IRAM_ATTR interruptRoutine() {
    isr_flag = 1;
}

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
        delay(500);
        Serial.print(".");
        counter++;
        if(counter>=60){ //after 30 seconds timeout - reset board
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
        if(deviceName == "lightswitch" && fname == "power"){
            if(command == "off"){
                switchOff();
            }
            if(command == "on"){
                switchOn();
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

void handleGesture() {
    if ( apds.isGestureAvailable() ) {
        switch ( apds.readGesture() ) {
        case DIR_UP:
            Serial.println("UP");
            switchOn();
            sendPowerCommands("on");
            break;
        case DIR_DOWN:
            Serial.println("DOWN");
            switchOff();
            sendPowerCommands("off");
            break;
        case DIR_LEFT:
            Serial.println("LEFT");
            break;
        case DIR_RIGHT:
            Serial.println("RIGHT");
            break;
        case DIR_NEAR:
            Serial.println("NEAR");
            break;
        case DIR_FAR:
            Serial.println("FAR");
            break;
        default:
            Serial.println("NONE");
        }
    }
}

void setup() {
    Serial.begin(115200);

    switchNeutral();

    //pinMode(ERROR_LED_PIN, OUTPUT);
    //digitalWrite(ERROR_LED_PIN, HIGH);

    //Setup I2C
    Wire.begin(APDS9960_SDA, APDS9960_SCL);

    // Set interrupt pin as input
    pinMode(APDS9960_INT, INPUT);

    // Initialize interrupt service routine
    attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);

    // Initialize APDS-9960 (configure I2C and initial values)
    if ( apds.init() ) {
        Serial.println(F("APDS-9960 initialization complete"));
    } else {
        Serial.println(F("Something went wrong during APDS-9960 init!"));
    }

    // Start running the APDS-9960 gesture sensor engine
    if ( apds.enableGestureSensor(true) ) {
        Serial.println(F("Gesture sensor is now running"));
    } else {
        Serial.println(F("Something went wrong during gesture sensor init!"));
    }
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
    if( isr_flag == 1 ) {
        detachInterrupt(APDS9960_INT);
        handleGesture();
        isr_flag = 0;
        attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
    }
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