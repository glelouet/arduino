/*
 * MOTEINO.cpp
 *
 *  Created on: 24 févr. 2016
 *      Author: guillaume
 */

#include <Moteino.h>
#include <Arduino.h>

Moteino::Moteino():
  owire(ONEWIRE_PIN),
	flash(FLASH_PIN, 0xEF30)//0xEF30 for windbond 4mbit flash
{
}

Moteino::~Moteino(){
}

void Moteino::setup(){
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  init_EEPROM();

  // this order is important :
  // flash gives us its unique id so we can translate it to a *mac* address for the RF69
  // if ethernet is present then we are a gateway, so our IP on the RF69 network is 0
  // we then initialize the RF69 using those parameters
  init_flash();
  init_ethernet();
  init_RF69();
  if(rewrite_EEPROM) writeEEPROM();
}

boolean Moteino::debug(int lvl) {
  return params.debug>=lvl;
}

void Moteino::check_serial(){
  while(Serial.available()>0) {
    char cread = Serial.read();
    serial_buffer[serial_blength++]=cread;
    if(cread=='\n' || cread=='\r'){
      Serial.println();
      serial_buffer[serial_blength-1]='\0';
      if(serial_blength>1) handleSerialMessage(serial_buffer);
      serial_blength=0;
    } else if(cread==127 || cread==8){//backspace or del
      serial_blength-=2;
      if(serial_blength<0) serial_blength=0;
    } else {
      Serial.print(cread);
    }
    if(serial_blength>=SERIAL_BUFFER_SIZE) {
      serial_buffer[SERIAL_BUFFER_SIZE-1]='\0';
      Serial.println("error : serial buffer overflow, discarding serial buffer :");
      Serial.println(serial_buffer);
      serial_blength=0;
    }
  }
}

#ifdef  HANDLE_SERIAL_SHELL

void Moteino::printParams(){
  Serial.print("version=");Serial.println(params.version);
  Serial.print("debug=");Serial.println(params.debug);
  Serial.print("paired=");Serial.println(params.paired);
  Serial.print("rdGW=");Serial.println(params.rdGW);
  Serial.print("rdNet=");Serial.println(params.rdNet);
  Serial.print("rdCrypt=");Serial.println(params.rdCrypt);
  Serial.print("rdKey=");for(int i=0;i<CRYPT_SIZE;i++) {
    Serial.print(params.rdKey[i], DEC);
    Serial.print(' ');
  } Serial.println();
  Serial.print("ethMac=");for(int i=0;i<ETH_MAC_SIZE;i++) {
    Serial.print(params.ethMac[i], DEC);
    Serial.print(' ');
  } Serial.println();
}

void Moteino::handleSerialMessage(char *message) {
  if(strncmp(message, "rdnet=", strlen("rdnet="))==0) {
    uint8_t rdNet = atoi(message+strlen("rdnet="));
    Serial.print("set rdnet ");
    Serial.println(rdNet);
    changeRadioNet(rdNet);
  } else if(strcmp(message, "rd")==0) {
    Serial.print("rdnet=");
    Serial.println(params.rdNet);
    Serial.print("rdip=");
    Serial.println(params.rdIp);
    params.paired=false;
    radio_state=RADIO_IDLE;
  } else if(strcmp(message, "rdsnet")==0) {
    Serial.println("search rd net");
    params.paired=false;
    radio_state=RADIO_IDLE;
  } else if(strncmp(message, "rdip=", strlen("rdip="))==0) {
    uint8_t netIP = atoi(message+strlen("rdip="));
    Serial.print("set rdip ");
    Serial.println(netIP);
    params.rdIp=netIP;
    radio.setAddress(netIP);
  } else if(strcmp(message, "rdsip")==0) {
    Serial.println("acquiring radio IP");
    radioAcquireIP();
    radio_state=RADIO_GETIP;
  } else if(strcmp(message, "write")==0) {
    writeEEPROM();
    Serial.println("wr2EEPROM ok");
  } else if(strcmp(message, "load")==0) {
    loadEEPROM();
    Serial.println("loadEEPROM");
    printParams();
  } else if(strcmp(message, "params")==0) {
    printParams();
  } else if(strncmp(message, "dbg=", strlen("dbg="))==0) {
    int debug = atoi(message+strlen("dbg="));
    Serial.print("set dbg ");
    Serial.println(debug);
    params.debug=debug;
  } else if(strcmp(message, "pairon")==0) {
    pairOn();
  } else if(strcmp(message, "pairoff")==0) {
    pairOff();
  } else if(strncmp(message, "blink ", strlen("blink "))==0) {
    int time = atoi(message+strlen("blink "));
    ledBlink(time);
  } else if(strncmp(message, "flash ", strlen("flash "))==0) {
    int time = atoi(message+strlen("flash "));
    ledFlash(time);
  } else {
    Serial.print("discarding ");
    Serial.println(message);
  }
}

#else

void Moteino::printParams(){}

void Moteino::handleSerialMessage(char *message) {
    Serial.print("serialOFF");
    Serial.println(message);
}

#endif

void Moteino::init_EEPROM(){
  if(acquire_from_EEPROM && !loadEEPROM())
  writeEEPROM();
}

boolean Moteino::loadEEPROM(){
    if(debug(DEBUG_INFO)) Serial.println("acquiring parameters from EEPROM");
    // To make sure there are settings, and they are YOURS!
    // If nothing is found it will use the default settings.
    if (EEPROM.read(EEPROM_offset + 0) == VERSION[0]
      && EEPROM.read(EEPROM_offset + 1) == VERSION[1]
      && EEPROM.read(EEPROM_offset + 2) == VERSION[2]){
        for (unsigned int t=0; t<sizeof(params); t++)
          *((char*)&params + t) = EEPROM.read(EEPROM_offset + t);
    } else {
      if(debug(DEBUG_WARN)) Serial.println("incorrect version in EEPROM");
      return false;
    }
  return true;
}



void Moteino::writeEEPROM() {
  for (unsigned int t=0; t<sizeof(params); t++)
    EEPROM.update(EEPROM_offset + t, *((char*)&params + t));
}

void Moteino::init_flash(){
  if (flash.initialize()) {
    uint8_t* uniq_id = flash.readUniqueId();
    flashId=0;
    for (byte i=4;i<8;i++) {
      flashId=flashId<<8|uniq_id[i];
    }
    if(debug(DEBUG_INFO)) {
      Serial.print("flashId=");
      Serial.println(flashId);
    }
  }
  else
    Serial.println("SPI Flash Init FAIL!");
}

void Moteino::init_ethernet(){
  hasEthernet=digitalRead(ETHERNET_PIN);
  if(debug(DEBUG_FULL)) {
    Serial.print("ethernet presence : ");
    Serial.println(hasEthernet);
  }
  if(hasEthernet) {
      Serial.println("Start Ethernet");
      ethc.stop();

      if(debug(DEBUG_INFO)) Serial.println(F("Connecting Arduino to network..."));

      delay(4000);
      // Change the PIN used for the ethernet
      Ethernet.select(ETHERNET_PIN);


      // Connect to network amd obtain an IP address using DHCP
      if (Ethernet.begin(params.ethMac) == 0)
      {
        if(debug(DEBUG_WARN)) Serial.println(F("DHCP Failed, reset Arduino to try again"));
      }
      else
      {
        if(debug(DEBUG_INFO)) Serial.println(F("Arduino connected to network using DHCP"));
      }

      if(debug(DEBUG_INFO)) {
        Serial.print("My IP address: ");
        for (byte thisByte = 0; thisByte < 4; thisByte++) {
          // print the value of each byte of the IP address:
          Serial.print(Ethernet.localIP()[thisByte], DEC);
          Serial.print(".");
        }
        Serial.println();
      }
      W5100.setRetransmissionTime(0x07D0); //where each unit is 100us, so 0x07D0 (decimal 2000) means 200ms.
      W5100.setRetransmissionCount(3); //That gives me a 3 second timeout on a bad server connection.

      delay(100);
  }
}

///////////////////////////////////////////////////////////
// RF69
///////////////////////////////////////////////////////////

void Moteino::init_RF69(){
  if(hasEthernet) {
    if(acquire_RF69_IP && debug(DEBUG_WARN))
      Serial.println("warning : should discover IP but has ethernet chip so static IP");
  }
  if (params.paired){//we already have net
    if(hasEthernet){
      radio.initialize(RF69_433MHZ,gw_RF69, params.rdNet);
      radio_state=RADIO_TRANSMIT;
    } else {
      if(acquire_RF69_IP){
        radio.initialize(RF69_433MHZ,0,params.rdNet);
        radio.promiscuous();
        radio_state=RADIO_GETIP;
      } else {
        radio.initialize(RF69_433MHZ,hasEthernet?gw_RF69:1,params.rdNet);
        radio_state=RADIO_TRANSMIT;
      }
    }
  } else {
    radio.initialize(RF69_433MHZ,0,0);
    radio_state=RADIO_IDLE;
  }
  #ifdef IS_RFM69HW //only for RFM69HW
    radio.setHighPower();!
  #endif
  radio.enableAutoPower(-60);
}

void Moteino::pairOn(){
  pairing=true;
  radio.promiscuous();
}

void Moteino::pairOff(){
  pairing=true;
  radio.promiscuous(false);
}

boolean Moteino::rdScanNet(){
  if(radio_state==RADIO_IDLE){
    radio_scan_net=0;
    radio_state=RADIO_GETNET;
    last_scan=millis();
    radio.setNetwork(radio_scan_net);
    sendBCRF69(RD_NET_DISCO);
    if(debug(DEBUG_FULL)){
      Serial.print("test rd net ");
      Serial.println(radio_scan_net);
    }
  } else {//radio_state == RADIO_GETNET
    if (radio.receiveDone()) {
      ledCount(10, 100, true);
      return true;
    } else if(millis()-last_scan>scan_delay) {
      radio_scan_net++;
      radio.setNetwork(radio_scan_net);
      last_scan=millis();
      sendBCRF69(RD_NET_DISCO);
    }
  }
  return false;
}

boolean Moteino::scanNetIP(){

}


void Moteino::sendRF69(char *trame, byte targetId){
  int sendSize = strlen(trame);
  // Send a message to rf69_server
  uint8_t data[100];
  for (int i=0;i<100;i++)
    data[i]=trame[i];
  radio.sendWithRetry(targetId, data, sendSize);
}

void Moteino::sendBCRF69(char *data){
  radio.send(RF69_BROADCAST_ADDR, data, strlen(data));
}

void Moteino::changeRadioNet(uint8_t net){
  params.rdNet=net;
  radio.setNetwork(net);
  params.paired=true;
  if(acquire_RF69_IP) {
    radio_state = RADIO_GETIP;
  } else {
    radio_state = RADIO_TRANSMIT;
  }
}

void Moteino::radioAcquireIP(){
  params.rdIp=RF69_BROADCAST_ADDR;
  radio_state=RADIO_GETIP;
}

void Moteino::check_RF69(){
  switch (radio_state) {
    case RADIO_IDLE:
    case RADIO_GETNET :
      if(rdScanNet())
        if(acquire_RF69_IP){
          radio_state=RADIO_GETIP;
        } else {
          radio.setAddress(hasEthernet?gw_RF69:1);
          radio_state=RADIO_TRANSMIT;
          radio.promiscuous(false);
        };
      break;
    case RADIO_GETIP:if(scanNetIP()) radio_state=RADIO_TRANSMIT;break;
    case RADIO_TRANSMIT :
      if (radio.receiveDone()) {
        if(pairing && strcmp(RD_NET_DISCO, (char *)radio.DATA)==0) {
          Serial.println("pairing");
          sendBCRF69("pairingACK");
          ledCount(10,100,true);
        }else
        // check if radio received rom to write on the flash, then flash it
          CheckForWirelessHEX(radio, flash, true);
        } else {
          //Serial.println("no data transmitted");
        }
    }
    radioLed();
}

void Moteino::radioLed(){
  if(millis()>=radio_next_led){
    if(radio_state==RADIO_TRANSMIT && pairing){
      if(led_state==LED_OFF)
        ledBlink(radio_ledcount_duration);
    } else {
      int count = RADIO_TRANSMIT-radio_state+1;
      int period = 2*radio_ledcount_duration/(count*2);
      ledCount(count, period);
    }
    radio_next_led=millis()+radio_count_delay+radio_ledcount_duration;
  }
}

/////////////////////////////////////////////////
// DS18B20
/////////////////////////////////////////////////

// Fonction récupérant la température depuis le DS18B20
// Retourne true si tout va bien, ou false en cas d'erreur
boolean Moteino::getTemperatureDS18B20(float *temp){
	byte data[9], addr[8];
  // data : Données lues depuis le scratchpad
  // addr : adresse du module 1-Wire détecté

  if (!owire.search(addr)) { // Recherche un module 1-Wire
    owire.reset_search();    // Réinitialise la recherche de module
    return false;         // Retourne une erreur
  }

  if (OneWire::crc8(addr, 7) != addr[7]) // Vérifie que l'adresse a été correctement reçue
    return false;                        // Si le message est corrompu on retourne une erreur

  if (addr[0] != DS18B20_PIN) // Vérifie qu'il s'agit bien d'un DS18B20
    return false;         // Si ce n'est pas le cas on retourne une erreur

  owire.reset();             // On reset le bus 1-Wire
  owire.select(addr);        // On sélectionne le DS18B20

  owire.write(0x44, 1);      // On lance une prise de mesure de température
  delay(800);             // Et on attend la fin de la mesure

  owire.reset();             // On reset le bus 1-Wire
  owire.select(addr);        // On sélectionne le DS18B20
  owire.write(0xBE);         // On envoie une demande de lecture du scratchpad

  for (byte i = 0; i < 9; i++) // On lit le scratchpad
    data[i] = owire.read();       // Et on stock les octets reçus

  // Calcul de la température en degré Celsius
  *temp = ((data[1] << 8) | data[0]) * 0.0625;

  // Pas d'erreur
  return true;
}

////////////////////////////////////////////////////////////
// LED
////////////////////////////////////////////////////////////

void Moteino::ledBlink(unsigned long delay_ms) {
  digitalWrite(LED_PIN,HIGH);
  led_state=LED_BLK;
  led_nxtchange=millis()+delay_ms;
}

void Moteino::ledFlash(unsigned long delay_ms){
  digitalWrite(LED_PIN,HIGH);
  led_state=LED_FLS;
  led_nxtchange=millis()+delay_ms;
  led_swdelay=delay_ms;
}

void Moteino::ledCount(int nb, unsigned long delay_ms, boolean prio){
  if(nb<1) return;
  if(prio) led_state=LED_OFF;
  switch(led_state) {
    case LED_OFF :
      led_state=LED_CNT;
      led_remains=nb*2;
      led_swdelay = delay_ms/2;
      led_nxtchange=millis();
    break;
  }
}

void Moteino::check_led(){
  switch(led_state) {
    case LED_BLK:
      if(led_nxtchange<=millis()) {
        digitalWrite(LED_PIN,LOW);
        led_state=LED_OFF;
      }
    break;
    case LED_FLS:
      if(led_nxtchange<=millis()) {
        led_nxtchange+=led_swdelay;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      }
    break;
    case LED_CNT:
      if(led_nxtchange<=millis()) {
        led_nxtchange+=led_swdelay;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        led_remains--;
        if(led_remains<=0){
          led_state=LED_OFF;
          digitalWrite(LED_PIN,LOW);
        }
      }
    break;
  }
}

////////////////////////////////////////////////////////

void Moteino::loop() {
  check_serial();
  check_RF69();
  check_led();
}
