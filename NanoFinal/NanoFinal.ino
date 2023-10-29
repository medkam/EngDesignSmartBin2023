#include <esp_now.h>
#include <WiFi.h>

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;
  bool isCorrectTrashType;
}struct_message;

// Create a struct_message called myData
struct_message myData;

// Create an array with all the structures
// id's are set in setup()
// board1 = plastic detection
//        id = 0
struct_message boardsStruct[1];

bool espNowInitialised;

bool recentlyReceivedData;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  recentlyReceivedData = true;

  // Copy the incoming data into our local struct
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("Board ID %u: %u bytes\n", myData.id, len);
  // Update the structures with the new incoming data
  boardsStruct[myData.id].isCorrectTrashType = myData.isCorrectTrashType;

  // And print what data was received
  Serial.print("Received data from board nr: ");
  Serial.println(myData.id);
  Serial.print("With message, detected trash is of correct type? ");
  Serial.println(myData.isCorrectTrashType);
  Serial.println();
}

int motorStepControlPin = 6;
int motorDirectionControlPin = 7;
int correctLedPin = 3;
int incorrectLedPin = 2;

// variables to keep track of time so we do not have to use a blocking call anywhere
unsigned long correctLedStartTime;
unsigned long incorrectLedStartTime;
unsigned long motorStartTime;

// keep track of whether the leds and motor are on
bool motorIsOn;
bool correctLedOn;
bool incorrectLedOn;

// determine how long the leds should be on and how many steps the motor should take
int ledTimeToBeOn = 3000;

// supporting variables for the motor
int motorStepCount;
int motorDirection; // 1 for one direction, -1 for the other direction
bool motorResetting; // if the motor is moving back into the reset position keep track of it

void initEspNow() {
  espNowInitialised = true;
  //Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    // if it fails print, do beware that at that point we will not receive any data
    Serial.println("Error initializing ESP-NOW");
    espNowInitialised = false;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  // if the espnow wasn't initialised properly do not attempt this to prevent potential errors
  if (espNowInitialised) {
    esp_now_register_recv_cb(OnDataRecv);
  }
  recentlyReceivedData = false;
}

void initPins() {
  // Initialise all motor control matters
  pinMode(motorDirectionControlPin, OUTPUT);
  pinMode(motorStepControlPin, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  // Make sure the motor is not moving and set the step amount by pulling low on the ms pins of the motor easydriver (pins 10 and 11)
  digitalWrite(motorStepControlPin, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);
  // set the supporting variables for the motor
  motorStartTime = millis();
  motorIsOn = false;
  motorStepCount = 17;
  motorDirection = 0;
  motorResetting = false;

  // setup the led pins
  pinMode(correctLedPin, OUTPUT);
  pinMode(incorrectLedPin, OUTPUT);
  digitalWrite(correctLedPin, LOW);
  digitalWrite(incorrectLedPin, LOW);
  // set the supporting variables for the leds
  correctLedStartTime = millis();
  incorrectLedStartTime = millis();
  correctLedOn = false;
  incorrectLedOn = false;
}
 
void setup() {
  //Initialize Serial Monitor
  Serial.begin(115200);
  
  // Initialise all EspNow matters
  //Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  initEspNow();

  initPins();
}

// move the motor into one or the other direction
// can say direction = 1 is one way and direction = -1 for the other way
// though that has to be determined
void moveMotor(int direction) {
  // move the motor in the given direction
  if (direction == 1) digitalWrite(motorDirectionControlPin, HIGH);
  else if (direction == -1) digitalWrite(motorDirectionControlPin, LOW);

  // write to the step control for a short moment then turn it off again to move the motor a short distance
  // wait for the delay between high and low here as we do not want to accidentally leave that too high
  // however for the delay between low and high we wait for the next call of this function to prevent long blocking calls
  // TODO adjust the 50 and 35 below accordingly for finetuning of the motor
  if (millis() - motorStartTime >= 50) {
    digitalWrite(motorStepControlPin, HIGH);
    delay(35);
    digitalWrite(motorStepControlPin, LOW);
    motorStartTime = millis();
    motorStepCount++;
  }

  // if the motorStepCount exceeded the stepCount needed reverse it to move back into position
  // however if we were already moving back into position we know the motor is done moving
  if (motorStepCount >= 25) {
    motorStepCount = 0;
    if (motorResetting) {
      motorResetting = false;
      motorIsOn = false;
    } else {
      motorDirection *= -1;
      motorResetting = true;
    }
  }
}

// switch given led to either LOW or HIGH output
// it is assumed that the given pin is in OUTPUT mode
void switchLedState(uint8_t pin, uint8_t state) {
  digitalWrite(pin, state);
}

// Handle what happens when we receive data
// when connecting multiple boards that can send data change this into a loop
void onChangedData() {
  int pinToChange = 0;
  bool isCorrectTrashType = boardsStruct[0].isCorrectTrashType;
  if (isCorrectTrashType) {
    // change the led on pin 18
    Serial.println("Turning on led one");
    pinToChange = correctLedPin;
    correctLedOn = true;
    correctLedStartTime = millis();
    // set motor direction
    // TODO configure the 1 and -1 below for motorDirection to fit the actual situation
    motorDirection = 1;
  } else {
    // change the led on pin 19
    Serial.println("Turning on led two");
    pinToChange = incorrectLedPin;
    incorrectLedOn = true;
    incorrectLedStartTime = millis();
    // set motor direction
    motorDirection = -1;
  }
  // indicate whether sorting was correct using the leds
  switchLedState(pinToChange, HIGH);
  // indicate to the motor that it should start moving
  motorIsOn = true;

  // now that we've handled the data once again wait for it to change
  recentlyReceivedData = false;
}
 
void loop() {
  // if we recently received data handle it
  // we do it separately from the callback function to prevent unexpected behaviour
  if (recentlyReceivedData && !motorIsOn) {
    onChangedData();
    Serial.println("Called onChangedData.");
  }

  // move the motor if needed
  if (motorIsOn) {
    moveMotor(motorDirection);
  }

  // check if we need to turn off a led
  if (correctLedOn && millis() - correctLedStartTime >= ledTimeToBeOn) {
    correctLedOn = false;
    switchLedState(correctLedPin, LOW);
  }
  if (incorrectLedOn && millis() - incorrectLedStartTime >= ledTimeToBeOn) {
    incorrectLedOn = false;
    switchLedState(incorrectLedPin, LOW);
  }

  // small delay so the microcontroller doesn't overheat when nothing needs to happen
  delayMicroseconds(10);

}