#include <ESP8266WiFi.h>                  // Install esp8266 by ESP8266 Community version 2.6.3
#include <ArduinoJson.h>                  // Install Library by Benoit Blanchon version 5.13.5
#include <PubSubClient.h>                 // Install Library by Nick O'Leary version 2.7.0

WiFiClient espClient;
PubSubClient client(espClient);

uint32_t current_time;
uint32_t previous_time;
uint32_t chart_time;
uint32_t led_time;

bool led_state = false;

#define PAYLOAD_LEN  100
char payload_get[PAYLOAD_LEN];
bool mqtt = false;

#define SERIAL_LEN   1000
char text[SERIAL_LEN];

char ssid[] = "samikro";
char pass[] = "samikroid";

#define CHART_DELAY     300000      // 300000 = 5 menit

#define MQTT_ID         "21e88721-5cc8-4eac-8b1b-4644e69b165b"

#define MQTT_BROKER     "broker.emqx.io"            //
#define MQTT_PORT       1883                        //
#define MQTT_USERNAME   ""                          // Change to your Username from Broker
#define MQTT_PASSWORD   ""                          // Change to your password from Broker
#define MQTT_TIMEOUT    10
#define MQTT_QOS        0
#define MQTT_RETAIN    false

#define MQTT2_BROKER     "mqtt.thingspeak.com"      //
#define MQTT2_PORT       1883                       //
#define MQTT2_USERNAME   "mwa0000022317560"                // Change to your Username from Broker
#define MQTT2_PASSWORD   "20UKGW1QZ0GBNQ19"         // Change to your password from Broker MQTT API Key
#define MQTT2_TIMEOUT    2
#define MQTT2_QOS        0
#define MQTT2_RETAIN     false

void callback(char* topic, byte* payload, unsigned int length) { //A new message has been received
  memcpy(payload_get, payload, length);
  mqtt = true;
}

/*
 *  enum rst_info->reason {
 *    0 : REASON_DEFAULT_RST, normal startup by power on
 *    1 : REASON_WDT_RST, hardware watch dog reset 
 *    2 : REASON_EXCEPTION_RST, exception reset, GPIO status won't change 
 *    3 : REASON_SOFT_WDT_RST, software watch dog reset, GPIO status won't change 
 *    4 : REASON_SOFT_RESTART, software restart ,system_restart , GPIO status won't change 
 *    5 : REASON_DEEP_SLEEP_AWAKE, wake up from deep-sleep 
 *    6 : REASON_EXT_SYS_RST, external system reset 
 *  };
 */
rst_info *myResetInfo;

void setup(){
  delay(300);
  Serial.begin(9600);

  initLed();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);                            delay(10);
  ESP.wdtFeed();                                  yield();

  led_time = millis();

  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    yield();

    if((millis()-led_time) > 1000){          // toggle led setiap detik
      toggleLed();

      led_time = millis();
    }

    if(WiFi.status() == WL_CONNECTED){
      break;
    }
  }
  
  if(WiFi.status() == WL_CONNECTED){
    connectMqtt();
  }

  chart_time = millis() + CHART_DELAY;
  led_time = millis();
}

void loop(){
  if(WiFi.status() == WL_CONNECTED){

    if((millis() - led_time) > 200){
      toggleLed();

      led_time = millis();
    }

    if((millis()-chart_time) >= CHART_DELAY){
      publishChart();

      chart_time = millis();
    }

    if(mqtt){
      mqtt = false;
      
      uint8_t n=0;
      for(n=0; n<3; n++){
        Serial.println(payload_get);
        if(waitSerialApp()){
          break;
        }
      }
      
      clearDataMqtt();
    }
  }

  client.loop();
}

void publishChart(){
  bool chartIsConnected = false;
  uint8_t n = 0;

  client.disconnect();
  client.setServer(MQTT2_BROKER, MQTT2_PORT);

  for(n=0; n<MQTT2_TIMEOUT; n++){

    if(client.connect(MQTT_ID, MQTT2_USERNAME, MQTT2_PASSWORD)){
      chartIsConnected = true;

      break;
    }
  }

  if(chartIsConnected){
    for(n=0; n<3; n++){
      Serial.println("{\"op\":\"data\",\"cmd\":\"get\"}");
      if(waitSerialChart()){
        break;
      }
    }
  }

  connectMqtt();
}

bool connectMqtt(){
  client.disconnect();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  mqtt = false;

  uint8_t i;
  for(i = 0; i < MQTT_TIMEOUT; i++) {
      
    if( client.connect(MQTT_ID, MQTT_USERNAME, MQTT_PASSWORD) ){
      delay(500);
      if(client.subscribe("samikro/cmd/project/2", MQTT_QOS)){
        return true;
        break;
      }
    }

    delay(500);                                 
  }
  return false;
}

void clearDataMqtt(){
  uint8_t n;
  for(n=0; n<PAYLOAD_LEN; n++){
    payload_get[n] = 0;
  }
}

void clearDataSerial(){
  uint16_t n;
  for(n=0; n<SERIAL_LEN; n++){
    text[n] = 0;
  }
}

/***** Serial ******/
bool waitSerialChart(){
  float field1=0;
  float field2=0;
  uint8_t field3=0;
  bool onOff = false;
  StaticJsonBuffer<SERIAL_LEN> JSONBuffer;

  if(waitSerial()){
    JsonObject& root = JSONBuffer.parseObject(text);
    if(root.success()){
      field1 = root["arus"];
      field2 = root["tegangan"];
      field3 = root["energy"];

      clearDataSerial();
      sprintf(text,"field1=%.1f&field2=%.2f&field3=%d", field1, field2, field3);
      client.publish("channels/1370408/publish/RVPIHDSVYN7U18U3",text,false);     // write api key
      clearDataSerial();
      return true;
    }
  }
  return false;
}

bool waitSerialApp(){
  if(waitSerial()){
    client.publish("samikro/data/project/2", text,false);
    clearDataSerial();
    return true;
  }
  return false;
}

bool waitSerial(){
  uint16_t n = 0;
  bool hasData = false;

  clearDataSerial();

  previous_time = millis();
  do{
    if(Serial.available() > 0){
      delay(10);
      break;
    }
  }while((millis() - previous_time) <= 1000);

  while(Serial.available() > 0){
    byte d = (char) Serial.read();
    text[n] = d;
    n++;
    hasData = true;
    delay(1);
  }

  return hasData;
}

/***** LED Setting *****/

void initLed(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);      // default mati
  led_state = false;
}

void toggleLed(){
  if(led_state == false){
    digitalWrite(LED_BUILTIN, LOW);     // nyalakan LED
    led_state = true;
  }else{
    digitalWrite(LED_BUILTIN, HIGH);    // matikan LED
    led_state = false;
  }
}

void setLed(bool state){
  if(state){    digitalWrite(LED_BUILTIN, LOW);     }// nyalakan LED
  else{    digitalWrite(LED_BUILTIN, HIGH);    }// matikan LED

  led_state = state;
}
