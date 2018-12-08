
 /*
  Air Quality Measurement using Adafruit SGP30  
  Tx over LoRA using UCW LORAWAN library
  Copyright 2018 Unity{Cloud}Ware - UCW Industries Ltd. All rights reserved.
 */

#include "config.h"

/*
  Edit the config.h to configure the connection string to the UCW Platform
  and any additional configuration needed for WiFi, cellular, or ethernet
  clients.
 */
 
#include <Wire.h>
#include "Adafruit_SGP30.h"

Adafruit_SGP30 sgp; //SGP30 object

int counter = 0;

// Track if the current message has finished sending
bool dataSent;

//baseline values
uint16_t tvoc_base, eco2_base;

//check if baseline values have changed
bool baseChange = false;

void setup() {
  // Start the serial connection
  Serial.begin(9600);

  if (! sgp.begin()){
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
  
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
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }
  counter++;
  uint16_t TVOC = sgp.TVOC;
  uint16_t eCO2 = sgp.eCO2;
  delay(1000);
  
  byte myData[4];
  myData[0] = highByte(TVOC);
  myData[1] = lowByte(TVOC);
  myData[2] = highByte(eCO2);
  myData[3] = lowByte(eCO2);
  
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
  if (counter == 30) {
    counter = 0;

   uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
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
 
