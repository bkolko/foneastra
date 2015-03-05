#include <OneWire.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define P_SCK    (13)
#define P_HWRX   (0)
#define P_HWTX   (1)
#define P_INT0   (2)
#define P_INT1   (3)
#define P_D6     (4)    // note: confusing mislabeling, but that's how it's wired. Will be fixed in next spin
#define P_D5     (5)

#define P_1W     (6)
#define P_D7     (7)
#define P_SWRX   (8)
#define P_SWTX   (9)
#define P_SDSS   (10)
#define P_MOSI   (11)
#define P_MISO   (12)
#define P_SCK    (13)

#define P_SS1    (A0)
#define P_BUZZ   (A1)
#define P_BT_ONB (A2)
#define P_SPISS  (A3)
#define P_SDA    (A4)
#define P_SCL    (A5)
#define P_VBAT   (A6)
#define P_WALL   (A7)

#define ADP5062_WADDR (0x14)
#define ADP5062_RADDR (0x94)

#define DS1340_ADDR  (0x16)


SoftwareSerial swSer(P_SWRX,P_SWTX); // for debugging -- open a terminal on your PC to a serial device to see debug messages
OneWire        oW(P_1W);             // onewire object
unsigned char  addr[8];              // the address returned by the thermometer
unsigned char  inb[3];               // just a generic buffer

// the BT chip comes up in whatever speed it was last set to. Since I've been fooling around
// with different speeds, I don't know what speed it's going to be at when I reset. So,
// we just cycle through a bunch of them until commands actually work. Then we actually 
// set the baud rate we want and go from there.
unsigned long   possible_bauds[] = { 
  115200, 57600, 38400, 19200, 9600, 4800 };
#define BAUD_COUNT (6)



// routine to return the address of the last 1W device detected
void find_1W(void) {
  swSer.println(F("Scanning for 1WB devices."));
  // scan for 1W peripherals
  bool search_done = false;
  while (!search_done) {
    if (oW.search(addr)) {
      swSer.println(F("Found 1WB device."));
      for (unsigned char i=0;i<8;i++) {
       swSer.print(addr[i],HEX);
      }
      swSer.println();
      search_done = true;
    }
    else {
      swSer.println(F("No 1WB device found."));
    }
  }
 swSer.println(F("1WB can finished."));
};



// routine to read the config register of the charge control IC over i2c
// correct value for the ID register is 0x19.
void try_I2C(void) {
  // check out i2c by reading ID register of
  // ADP5062
  swSer.println(F("Trying to read the ADP5062 I2C bus..."));
  Wire.begin();
  for (unsigned char  k=0;k<3;k++) {
    Wire.beginTransmission(ADP5062_WADDR);
    Wire.write(0x0);
    Wire.endTransmission();
    
    Wire.requestFrom(ADP5062_RADDR,1);
    unsigned  char b = Wire.read();
    swSer.print(F("ADP5062 ID register returned "));
    swSer.println(b,HEX);
    delay(300);
    
  }
  /*
 Wire.beginTransmission(DS1340_ADDR);
   Wire.write(0);
   Wire.write(0); // second
   Wire.write(1); // minute
   Wire.write(2); // hour
   Wire.write(0);
   Wire.write(3); // day
   Wire.write(4); // month
   Wire.write(5); // year - 2000
   Wire.write(0);
   Wire.endTransmission();
   
   for (unsigned char k=0;k<5;k++) {
   Wire.beginTransmission(DS1340_ADDR);
   Wire.write(0);
   Wire.endTransmission();
   
   Wire.requestFrom(DS1340_ADDR, 7);
   unsigned char sec = Wire.read() & 0x7f;
   for (unsigned char l=0;k<6;l++) { 
   Wire.read();
   }
   swSer.print("DS1340 secs: ");
   swSer.println(sec);
   delay(1000); 
   }
   */
   swSer.println(F("Done with basic i2c test."));
};


// routine to just print out to SoftwareSerial whatever we received in
// the hardware serial port. Note that the buffer connected to the 
// Serial port is only 64 bytes, so if the two port speeds differe
// by a lot, you're going to get garbage if the message is longer 
// than 64 bytes.
void serial_slurp_bytes(void) {
  int ct = 0;
  Serial.flush();
  int b;
  while (b = Serial.available()) {
    swSer.print("(");
    swSer.print(b);
    swSer.print(") ");
    char x = Serial.read();
    swSer.println((char)x);
    if (ct < 3) { 
      inb[ct] = x; 
    };
    ct++;
  }
}

// routine to check command response. This is the only good response...
bool buf_is_AOK() {
  return ((inb[0] == 'A') &&
    (inb[1] == 'O') &&
    (inb[2] == 'K'));
};
bool buf_is_CMD() {
  return inb[0] == 'C';
  return ((inb[0] == 'C') &&
    (inb[1] == 'M') &&
    (inb[2] == 'D'));
};


// routine to try different speeds until we find one where we can
// communicate with the BT module. Then we use that to set the speed
// we ultimately want
bool bt_try_baud(unsigned long current) {
  swSer.print(F("attempting to enter command mode at "));
  swSer.print(current);
  swSer.print(F(" baud."));
  swSer.println("first enter command mode"); 
  Serial.begin(current);
  Serial.print(F("$$$"));
  delay(1000);
  serial_slurp_bytes();
  if (buf_is_CMD()) {
    swSer.println(F("now change baud to 115200")); 
    Serial.println(F("SU,115K"));
    Serial.flush();
    delay(300);
    serial_slurp_bytes();
    swSer.println(F("and now reboot the module"));
    Serial.println(F("R,1"));
    delay(7000);
    serial_slurp_bytes();
    //echo_mode();
    return true;
  }
  swSer.println(F("no dice."));
  return false;
}

// note: never returns
void echo_mode() {
  char b[256];
  int bp = 0;

  while (1) {
    for (int rep=0;rep<5;rep++) {
      bp = 0;
      while (Serial.available() && (bp<256)) { 
        b[bp++] = Serial.read(); 
      }
      for (int ip=0;ip<bp;ip++) { 
        swSer.write(b[ip]); 
      }
    }
    while (swSer.available()) { 
      Serial.write(swSer.read()); 
    }
  }
}


// routine to fire up BT and try it out...
void try_BT(void) {

  // turn it on...
  digitalWrite(P_BT_ONB,0);
  swSer.println(F("turning on BT"));
  swSer.println(F("waiting for BT power to stabilize... probably not necessary"));
  delay(1000);

  bool working_baud = false;
  unsigned char bd_ct = 0;
  while (!working_baud && (bd_ct < BAUD_COUNT)) {
    working_baud = bt_try_baud(possible_bauds[bd_ct++]); 
  }

  if (working_baud) {
    swSer.println(F("good news! we were able to adjust BT baud rate!"));

    Serial.begin(115200);
    Serial.print(F("$$$"));
    delay(300);
    serial_slurp_bytes();

    // x command prints out the curent settings  
    Serial.println(F("x"));
    Serial.flush();
    delay(300);
    for (unsigned char o=0;o<10;o++) {
      serial_slurp_bytes();
    }
    // get out of command mode
    Serial.println(F("---"));
    delay(300);
    swSer.println(F("BT module comms set up and should be able to talk now"));
    delay(2000);
  } 
  else {
    swSer.println(F("ruh, roh. could not communicate with BT module."));
  }
}

// routines to open a file, write a bit to it, and then
// splat back the file
void test_SD(void) {
  swSer.println(F("trying uSD card"));
  if (!SD.begin(P_SDSS)) {
    swSer.println(F("Could not initialize SD reader."));
    return;
  }
  File oFile = SD.open("example.txt",FILE_WRITE);
  if (oFile) {
    oFile.println("This is some contents for example.txt");
    for (unsigned int l=0;l<100;l++) {
      oFile.println(l,HEX);
    }
    oFile.close();
  }

  File iFile = SD.open("example.txt",FILE_READ);
  if (iFile) {
    while (iFile.available()) {
      swSer.write(iFile.read());
    }
    iFile.close();
  }
}

// simple routine to read the 1W thermometer
float read_1W(void) {
  unsigned char data[9];
  swSer.print(F("(addr: "));
  for (unsigned char i=0;i<8;i++) {
    swSer.print(addr[i],HEX);
  }

  oW.reset();
  oW.select(addr);
  oW.write(0x44,1);
  delay(500);
  oW.reset();
  oW.select(addr);
  oW.write(0xbe);
  swSer.print(F(") (dbytes: "));
  for (unsigned char i=0;i<9;i++) {
    data[i] = oW.read();
    swSer.print(data[i],HEX);
  }
  short temp = (0xff & data[0]) | (0xff00 & (data[1] << 8));
  float ftemp = (float)temp * 0.0625;
  swSer.print(F(") (temp: "));
  swSer.print(ftemp);
  swSer.println(")");
  return ftemp;
}  

void test_Buzz(unsigned int t, unsigned int d) {
  pinMode(P_BUZZ,OUTPUT);
  for (unsigned int i=0;i<t;i++) {
    // buzzer wants a 2 kHz square wave. It's most efficient at that
    // freq. 1 kHz is actually low, so getting this freq right is important
    // for high volume. Probably best to use a PWM output, but sadly, 
    // buzzer is on an analog pin.
    digitalWrite(P_BUZZ,0);
    delay(d);
    digitalWrite(P_BUZZ,1);
    delay(d);
  }
  pinMode(P_BUZZ,INPUT_PULLUP);
};

void setup() {
  // Atmega pins default to inputs at reset
  // turn off BT
  pinMode(P_BT_ONB,OUTPUT);
  digitalWrite(P_BT_ONB,1);
  // set up the LED for output
  pinMode(P_D5,OUTPUT);
  // and the button for input
  pinMode(P_INT1,INPUT);
  delay(1000);
  
  swSer.begin(9600);  
  swSer.println(F("Setup starting!"));

  swSer.print(F("buzz!"));
  test_Buzz(25,1);
  swSer.println(F("buzz done."));


  test_SD();
  try_I2C();  
  find_1W(); 
  try_BT();
}



int16_t count = 0;

void loop() {
  // toggle some LED5  as we loop
  digitalWrite(P_D5,(count & 0x1));

  float temp = read_1W();

  if (digitalRead(P_INT1)) {
    swSer.println(F("button 1 was pressed"));
    Serial.println(F("button 1 was pressed"));
  }

  Serial.print(F("temp:"));
  Serial.println(temp);

  count++;
  delay(2000);  
}

