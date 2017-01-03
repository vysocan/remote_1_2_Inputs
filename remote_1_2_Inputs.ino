// Remote node for RFM69, with 4x relay
// v.1.20
//
//                  +-\/-+
//            PC6  1|    |28  PC5 (AI 5)
//      (D 0) PD0  2|    |27  PC4 (AI 4)
//      (D 1) PD1  3|    |26  PC3 (AI 3)
//      (D 2) PD2  4|    |25  PC2 (AI 2)
// PWM+ (D 3) PD3  5|    |24  PC1 (AI 1)
//      (D 4) PD4  6|    |23  PC0 (AI 0)
//            VCC  7|    |22  GND
//            GND  8|    |21  AREF
//            PB6  9|    |20  AVCC
//            PB7 10|    |19  PB5 (D 13)
// PWM+ (D 5) PD5 11|    |18  PB4 (D 12)
// PWM+ (D 6) PD6 12|    |17  PB3 (D 11) PWM
//      (D 7) PD7 13|    |16  PB2 (D 10) PWM
//      (D 8) PB0 14|    |15  PB1 (D 9)  PWM
//                  +----+
#include <DigitalIO.h>
#include <SPI.h>
#include <RFM69.h>
#include <RFM69_ATC.h>
#include <avr/eeprom.h> // Global configuration for in chip EEPROM

#define NODEID      3                  // My address
#define NETWORKID   100                // Match network ID to geteway
#define GATEWAYID   1                  // This does not change
#define FREQUENCY   RF69_868MHZ        // Match this with the version of your Moteino! (others: RF69_433MHZ, RF69_868MHZ)
#define KEY         "ABCDABCDABCDABCD" // Has to be same 16 characters/bytes on all nodes, not more not less!
#define ACK_TIME    30                 // # of ms to wait for an ack
//#define ENABLE_ATC                   // Comment out this line to disable AUTO TRANSMISSION CONTROL, Not needed for DC powered
#define ATC_RSSI    -75

#define VERSION    120  // Version of EEPROM struct
#define REG_LEN    21   // size of one conf. element
#define REG_REPEAT 10   // repeat sending

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

// Global variables
int8_t  i;
uint8_t pos;
uint8_t count;
uint8_t radioLength;
uint8_t msg[REG_LEN+1];

struct config_t {
  uint16_t version;
  char     reg[REG_LEN * 4]; // Number of elements on this node
} conf; 

// Float conversion 
union u_tag {
    uint8_t  b[4]; 
    float    fval;
} u;

DigitalPin<A0>  pinOUT0(OUTPUT);
DigitalPin<A1>  pinOUT1(OUTPUT);
DigitalPin<A2>  pinOUT2(OUTPUT);
DigitalPin<A3>  pinOUT3(OUTPUT);

// Registration
void send_conf(){ 
  delay(NODEID*100); // Wait some time to avoid contention
  pos = 0; 
  Serial.print(F("Conf "));
  while (pos < sizeof(conf.reg)) {
    msg[0] = 'R'; // Registration flag
    for (uint8_t ii=0; ii < REG_LEN; ii++){ 
      msg[1+ii] = conf.reg[pos+ii];
    }
    Serial.print(radio.sendWithRetry(GATEWAYID, msg, REG_LEN + 1, REG_REPEAT));
    Serial.print(F("-"));
    pos+=REG_LEN;
  }
  Serial.println(F(" sent"));
}
// Set defaults on first time
void setDefault(){ 
  conf.version = VERSION;   // Change VERSION to take effect
  conf.reg[0]  = 'I';       // Input
  conf.reg[1]  = 'D';       // Digital
  conf.reg[2]  = 0;         // Local address
  conf.reg[3]  = B00000000; // Default setting
  conf.reg[4]  = B00011110; // Default setting, group=16, disabled
  for (uint8_t ii=0; ii < 17; ii++){ conf.reg[5+ii] = 0;} // Placeholder for name
  conf.reg[21] = 'I';       // Input
  conf.reg[22] = 'D';       // Digital
  conf.reg[23] = 1;         // Local address
  conf.reg[24] = B00000000; // Default setting
  conf.reg[25] = B00011110; // Default setting, group=16, disabled
  for (uint8_t ii=0; ii < 17; ii++){ conf.reg[26+ii] = 0;} // Placeholder for name
  conf.reg[42] = 'I';       // Input
  conf.reg[43] = 'D';       // Digital
  conf.reg[44] = 2;         // Local address
  conf.reg[45] = B00000000; // Default setting
  conf.reg[46] = B00011110; // Default setting, group=16, disabled
  for (uint8_t ii=0; ii < 17; ii++){ conf.reg[47+ii] = 0;} // Placeholder for name
  conf.reg[63] = 'I';       // Input
  conf.reg[64] = 'D';       // Digital
  conf.reg[65] = 3;         // Local address
  conf.reg[66] = B00000000; // Default setting
  conf.reg[67] = B00011110; // Default setting, group=16, disabled
  for (uint8_t ii=0; ii < 17; ii++){ conf.reg[68+ii] = 0;} // Placeholder for name
}

void checkRadio(){
  // Look for incomming transmissions
  if (radio.receiveDone()) {
    radioLength = radio.DATALEN; 
    if (radio.ACKRequested()) { 
      delay(5); // wait after receive to settle transmiiter, we need this delay or gateway will not see ACK!!!
      radio.sendACK();
      Serial.print(F("ACK:"));
    }
    for (uint8_t i=0; i < radioLength; i++){ Serial.print((char)radio.DATA[i], HEX); Serial.print("-"); }; Serial.println(F("<"));
    // Commands from gateway
    if ((char)radio.DATA[0] == 'C') {
      Serial.print(F("C:"));
      Serial.println(radio.DATA[1],HEX);
      delay(200); // give Gateway some time
      if (radio.DATA[1] == 1) send_conf(); // Request for registration
    }
    // Configuration change 
    if ((char)radio.DATA[0] == 'R') { 
      Serial.print(F("R:"));
      // Replace part of conf string with new paramters.
      pos = 0; 
      while (((conf.reg[pos] != radio.DATA[1]) || (conf.reg[pos+1] != radio.DATA[2]) || (conf.reg[pos+2] != radio.DATA[3])) && (pos < sizeof(conf.reg))) {
        pos += REG_LEN; // size of one conf. element
      }
      if (pos < sizeof(conf.reg)) {
        Serial.println(pos);
        msg[0] = 'R';
        for (uint8_t ii=0; ii < radioLength-1; ii++){
          conf.reg[pos+ii] = radio.DATA[1+ii];
          msg[1+ii]        = radio.DATA[1+ii];
          Serial.print(msg[1+ii], HEX); Serial.print(F("-"));
        }
        // Save it to EEPROM
        conf.version = VERSION;
        eeprom_update_block((const void*)&conf, (void*)0, sizeof(conf)); // Save current configuration
        delay(100); // give Gateway some time
        radio.sendWithRetry(GATEWAYID, msg, radioLength, REG_REPEAT); // Send it back for reregistration
      }
    }
    // Data to relayes
    if ((char)radio.DATA[0] == 'I') { // Input
      Serial.print(F("I:"));
      Serial.print((byte)radio.DATA[1]); 
      Serial.print(F(":"));
      u.b[0] = radio.DATA[2]; u.b[1] = radio.DATA[3]; u.b[2] = radio.DATA[4]; u.b[3] = radio.DATA[5]; 
      Serial.print((byte)u.fval);
      Serial.println();
      switch((byte)radio.DATA[1]){
        case 0: pinOUT0.write((byte)u.fval); break;
        case 1: pinOUT1.write((byte)u.fval); break; 
        case 2: pinOUT2.write((byte)u.fval); break; 
        case 3: pinOUT3.write((byte)u.fval); break; 
      }
    }
  }
}

void setup() {
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.encrypt(KEY);
  #ifdef ENABLE_ATC
    radio.enableAutoPower(ATC_RSSI);
  #endif

  eeprom_read_block((void*)&conf, (void*)0, sizeof(conf)); // Read current configuration
  if (conf.version != VERSION) setDefault();
   
  Serial.begin(9600); 
  count = 0;

  delay(2000);
  send_conf(); 
}

void loop() {
  checkRadio();
}
