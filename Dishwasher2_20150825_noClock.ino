/*
// Dishwasher Cycle Test Program
//
// July 21, 2010 - Dennis Robertson
//
// This program is meant to run the dishwasher in the lab for a set number of
// cycles automatically.
//
// There are a few key EE things that are implemented in this program which
// be looked into if you're not familiar with them: interrupts, debouncing,
// EEPROM, and pull-down resistors.
//
// This program also utilizes serial communications to an LCD.
//
//
// Modified July 23, 2015 - Chris Sebesta
//
// Updated syntax to modern Arduino syntax (removed things like BYTE, serial.print updated to serial.write where required
// Updated to the newer digital whirlpool dishwasher - required hardware modification and reproduction
// of the required membrane keyboard circuits (input COM3D, H", and E
//
//August 25, 2015
//Changed time duration of the wash cycle
//Adding functionality  to display minutes remaining for the test
*/

#include <EEPROM.h>

// Pinouts

int ledPin = 13;         // LED on board the Arduino
int handlePin = 10;      // Pin to fake the opening and closing of the door
int startPin = 12;       // Start button on dishwasher
int soapPin = 11;        // Pin that controls the soap pump
int upPin = 2;           // Increment cycle count button
int dnPin = 3;           // Decrement cycle count button
int goPin = 8;           // Start button on control box

// Global Variables

int loc_flag = 0;        // Location flag, used to tell display what is going on
int time_left=0;        // time left in the current cycle
int loop_delay = 50;     // Delay for for loops that look for up/down button presses [ms]

long abs_cyc = 0;         // absolute cycle count (odometer)
volatile int cyc2do = 5; // cycles to do

int time_handle = 500;   // time for door to be "open", [ms]
int time_soap = 3500;    // time for soap pump to be on, [ms]
int time_start = 3000;   // time for start button to be on, [ms] 
int time_run = 160;     // time for dishwasher cycle to take, [min] SEBESTA 2015-8-25 changed to 160 for new dishwasher
unsigned long min2ms = 60000;  // conversion from minutes to ms

int eeprom_loc = 0;      // location in EEPROM, there are 512 bytes of data, so: 0 <= eeprom_loc <= 511
int eeprom_data[512];    // value of EEPROM data as a function of location

// Debouncing Variables

volatile long lastDebounceTime = 0;     // the last time the interrupt was triggered
long debounceDelay = 200;               // the debounce time; decrease if quick button presses are ignored


// ========= Begin Program =========

void setup() {

  // Set up serial connection

  Serial.begin(9600);

  // Set up pinouts

  pinMode(handlePin, OUTPUT);
  pinMode(startPin, OUTPUT);
  pinMode(soapPin, OUTPUT);
  pinMode(upPin, INPUT);
  pinMode(dnPin, INPUT);
  pinMode(goPin, INPUT);

  digitalWrite(handlePin, LOW);
  digitalWrite(startPin, LOW);
  digitalWrite(soapPin, LOW);

  // Set up EEPROM data

  for (int i = 0; i <= 511; i++) {
    eeprom_data[i] = EEPROM.read(i);
  }

  for (int i = 0; i <= 511; i++) {
    abs_cyc = abs_cyc + eeprom_data[i];
  }

  // Set up VFD display

  Serial.write(0x0C);  //CLEAR SCREEN CMD

  Serial.write(0x1F);  //over write mode
  Serial.write(0x01);  //over write mode

  Serial.write(0x1F);  //CURSOR
  Serial.write(0x43);  //CURSOR
  Serial.write(00);  // NO CURSOR

  write2disp();
}

void loop() {

  // Wait for start button on controller

  while (digitalRead(goPin) == LOW) {
    delay(loop_delay);
    check_inputs();
    loc_flag = 1;
    write2disp();
  }

  // Begin the set of cycles

  while (cyc2do > 0) {
/* OPEN DOOR NO LONGER NECESSARY WITH NEW DISHWASHER, LEAVING CODE HERE INCASE NEEDED IN FUTURE - SEBESTA 8-25-2015
    // "Open door"

    check_inputs();

    loc_flag = 0;
    write2disp();

    for (int i = 0; i < (time_handle / loop_delay); i++) {
      digitalWrite(handlePin, LOW);
      check_inputs();
      delay(loop_delay);
    }

    digitalWrite(handlePin, HIGH);
*/
    // Pump Soap

    check_inputs();

    loc_flag = 2;
    write2disp();

    for (int i = 0; i < (time_soap / loop_delay); i++) {
      digitalWrite(soapPin, HIGH);
      check_inputs();
      delay(loop_delay);
    }
    digitalWrite(soapPin, LOW);

    // Press Start Button

    check_inputs();

    loc_flag = 3;
    write2disp();

    for (int i = 0; i < (time_start / loop_delay); i++) {
      digitalWrite(startPin, HIGH);
      check_inputs();
      delay(loop_delay);
    }
    digitalWrite(startPin, LOW);
    delay(500);
    for (int i = 0; i < (time_start / loop_delay); i++) {
      digitalWrite(startPin, HIGH);
      check_inputs();
      delay(loop_delay);
    }
    digitalWrite(startPin, LOW);

    // Let dishwasher run, wait a set amount of time

    check_inputs();

    loc_flag = 4;
    write2disp();

    for (int i = 0; i < ((time_run * min2ms) / loop_delay); i++) {
      check_inputs();
      delay(loop_delay);      
      //time_left = time_run - (i/min2ms*loop_delay);
      //write2disp();
    }

    // Increment Odometer

    odometer();

    check_inputs();

    // Decrement Count

    decrement();

    check_inputs();

    // Update Display


    write2disp();
  }
}

void increment() {

  // Same as increment, except it decrements cyc2do

  delay(debounceDelay);

  if (cyc2do < 99) {
    cyc2do++;
  }

  write2disp();  // Update the display
}

void decrement() {

  // Same as increment, except it decrements cyc2do

  delay(debounceDelay);

  if (cyc2do > 0) {
    cyc2do--;
  }

  write2disp();  // Update the display
}

void odometer() {

  // This function increments the value stored in the EEPROM

  int val = 0;  // value pulled from a particular location in EEPROM
  abs_cyc = 0;  // Reset the cycle count to zero.


  // Loop through the EEPROM and store it into the eeprom_data array,
  // Also, find the last position with data in it.

  // Note that by default, all locations in EEPROM have a value of 255.
  // In another sketch I overwrote them all to zero.

  for (eeprom_loc; eeprom_loc <= 511; eeprom_loc++) {
    eeprom_data[eeprom_loc] = EEPROM.read(eeprom_loc);

    // If the current location in EEPROM is blank, break the for loop

    if (EEPROM.read(eeprom_loc) == 0) {

      // Also, rewind the location of the first blank location back one
      // so we have the last location with data in it (except if this location
      // is zero (i.e. the entire EEPROM is blank), otherwise we would have a
      // negative location

      if (eeprom_loc != 0) {
        eeprom_loc--;
      }
      break;
    }
  }


  // Cycle through eeprom_data array and add up the values at all locations together
  // to get total count

  for (int i = 0; i <= eeprom_loc; i++) {
    abs_cyc = abs_cyc + eeprom_data[i];
  }


  // Increment absolute cycle count to be used elsewhere in the program (it is global)

  abs_cyc++;


  // If the current location reads 255, i.e. it is full, use the next location

  if (EEPROM.read(eeprom_loc) == 255) {
    eeprom_loc++;
  }


  // Read the value of the EEPROM at the last location with data in it,
  // increment it, then write the new data

  val = EEPROM.read(eeprom_loc);
  val++;
  EEPROM.write(eeprom_loc, val);
}

void write2disp() {


  Serial.write(0x1F);  //SET POS
  Serial.write(0x24);  //SET POS
  Serial.write(01);  // N column
  Serial.write(01);  // M row

  Serial.print("Cycles left:");

  Serial.write(0x1F);  //SET POS
  Serial.write(0x24);  //SET POS
  Serial.write(14);  // N column
  Serial.write(01);  // M row

  Serial.print(cyc2do);

  if (cyc2do < 10) {
    Serial.write(0x1F);  //SET POS
    Serial.write(0x24);  //SET POS
    Serial.write(15);  // N column
    Serial.write(01);  // M row

    Serial.write(0x7F);
    Serial.write(0x08);
    Serial.write(0x7F);
  }

  Serial.write(0x1F);  //SET POS
  Serial.write(0x24);  //SET POS
  Serial.write(01);  // N column
  Serial.write(02);  // M row

  Serial.print("ABS:");

  Serial.write(0x1F);  //SET POS
  Serial.write(0x24);  //SET POS
  Serial.write(06);  // N column
  Serial.write(02);  // M row

  Serial.print(abs_cyc);

  Serial.write(0x1F);  //SET POS
  Serial.write(0x24);  //SET POS
  Serial.write(14);  // N column
  Serial.write(02);  // M row

  switch (loc_flag) {
    case 0:
      Serial.print(" *DOOR*");
      break;
    case 1:
      Serial.print("*READY*");
      break;
    case 2:
      Serial.print(" *SOAP*");
      break;
    case 3:
      Serial.print("*START*");
      break;
    case 4:
      Serial.print("  *RUN*");
      //Serial.print("       ");
      //Serial.write(0x1F);  //SET POS
      //Serial.write(0x24);  //SET POS
      //Serial.write(14);  // N column
      //Serial.write(02);  // M row      
      //Serial.print(time_left);
      //Serial.print("%07d", time_left);
      
      //Serial.print(time_left); //SEBESTA 2015-8-25 adjust display output to show how many minutes are left in the current cycle
      break;
  }
}

void check_inputs() {
  if (digitalRead(upPin) == HIGH) {
    increment();
  }
  if (digitalRead(dnPin) == HIGH) {
    decrement();
  }
}
