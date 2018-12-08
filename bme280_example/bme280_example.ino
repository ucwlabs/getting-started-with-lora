
 /*
  Measuring Temperature(C), Relative Himidity(%) and Pressure(mmHg) using UCW LORAWAN library
  Copyright 2018 Unity{Cloud}Ware - UCW Industries Ltd. All rights reserved.
 */

#include "config.h"

/*
  Edit the config.h to configure the connection string to the UCW Platform
  and any additional configuration needed for WiFi, cellular, or ethernet
  clients.
 */
 
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 9
#define BME_CS 10

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

// Track if the current message has finished sending
bool dataSent;

void setup() {
  // Start the serial connection
  Serial.begin(9600);

  //initialise BME sensor
  bool status = bme.begin();  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // LoRaWAN setup
  lora_wan.initABP(NWKSKEY, APPSKEY, DEVADDR);
  //lora_wan.initOTAA(APPEUI, APPKEY, DEVEUI); //for OTAA
  
  lora_wan.channelConfig(true); //only for ABP
}

void loop() {
  readData();
}

void readData(){
  // Ensure there is not a current TX/RX job running
  if (LMIC.opmode & (1 << 7)) {
    // Something already in the queque
    return;
  }

  //payload
  int16_t t = bme.readTemperature() * 100;
  int16_t h = bme.readHumidity() * 100;
  int16_t p = bme.readPressure()/3386.39 * 100;

  byte myData[6];
  myData[0] = highByte(t);
  myData[1] = lowByte(t);
  myData[2] = highByte(h);
  myData[3] = lowByte(h);
  myData[4] = highByte(p);
  myData[5] = lowByte(p);

  //Prepare upstream data transmission at the next possible time.
  LMIC_setTxData2(1, myData, sizeof(myData), 0);
  Serial.println(F("Packet queued"));

  // Wait for the data to send or timeout after 1min
  int sinceSend = 0;
  dataSent = false;
  while (!dataSent && sinceSend < 60000) {
    os_runloop_once();
    sinceSend++;
    delay(1);
  }
}

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch(ev) {
    case EV_JOINING:
      Serial.println(F("EV_JOINING"));
      break;

    case EV_JOINED:
      Serial.println(F("EV_JOINED"));

      // Disable link check validation (automatically enabled
      // during join, but not supported by TTN at this time).
      LMIC_setLinkCheckMode(0);
      break;
      
    case EV_JOIN_FAILED:
      Serial.println(F("EV_JOIN_FAILED"));
      break;

    case EV_REJOIN_FAILED:
      Serial.println(F("EV_REJOIN_FAILED"));
      break;
      break;

    case EV_TXCOMPLETE:
      dataSent = true;
      Serial.println(F("transmission completed"));
      //check if data received from server
      if (LMIC.dataLen) {
        // data received in rx slot after tx
        Serial.print(F("Received "));
        Serial.print(LMIC.dataLen);
        Serial.print(F(" bytes of payload: 0x"));
        for (int i = 0; i < LMIC.dataLen; i++) {
          if (LMIC.frame[LMIC.dataBeg + i] < 0x10) {
            Serial.print(F("0"));
          }
          //print received data
          Serial.println(LMIC.frame[LMIC.dataBeg + i], HEX);
        }
      }
      break;

    case EV_LINK_DEAD:
      Serial.println(F("EV_LINK_DEAD"));
      lora_wan.initABP(NWKSKEY, APPSKEY, DEVADDR);
      //lora_wan.initOTAA(APPEUI, APPKEY, DEVEUI);  
      break;

    default:
      Serial.println(F("Unknown event"));
      break;
  }
}
 
