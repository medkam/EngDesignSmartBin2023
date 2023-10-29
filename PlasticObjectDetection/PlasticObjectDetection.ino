/* Image detection code for Engineering Design 2023 group 39 project Smart Bin
This code uses an ai-thinker esp32cam and esp-now to perform image detection and send the results to a different esp

In order to make use of this code first collect a dataset of images that you want to train your image detection model on.
Upload these images to edge impulse and train an object detection model on it
Then download an arduino library of the trained model and include it here.

For using esp-now you will need to find the mac adress of the esp you are sending data TO and change the broadcastAddress below accordingly
*/
#define MAX_RESOLUTION_VGA 1

#include <esp32cam_trash_detection_in_bin_inferencing.h>
#include "esp32cam.h"
#include "esp32cam/tinyml/edgeimpulse/FOMO.h"
#include "esp_now.h"
#include "WiFi.h"

// The mac address of the esp data is sent to
// 34:85:18:7B:03:4C
uint8_t broadcastAddress[] = {0x34, 0x85, 0x18, 0x7B, 0x03, 0x4C};

typedef struct struct_message {
  int id;
  bool isPlastic;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// id that will be sent along with any message by this device
int id;

using namespace Eloquent::Esp32cam;

Cam cam;
TinyML::EdgeImpulse::FOMO fomo;

// Keep track of the last three types of materials detected, where true = plastic and false is something else.
// Use some simple bools to keep things simple for those with no programming experience
bool previousType;
bool oldType;
bool oldestType;

// Keep track of whether or not we have detected an object recently and how long ago it was
int loopsSinceObjectDetected;
int loopsSinceDataSent;
bool objectDetected;

bool espNowInitialised;

// Callback for data sending
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
    previousType = false;
    oldType = false;
    oldestType = false;
    loopsSinceObjectDetected = 0;
    loopsSinceDataSent = 0;
    objectDetected = false;
    id = 0;

    Serial.begin(115200);
    delay(3000);
    Serial.println("Init");

    // Set device as a Wi-Fi station
    WiFi.mode(WIFI_STA);

    // init ESP-NOW
    espNowInitialised = true;
    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initialising ESP-NOW.");
      espNowInitialised = false;
    }

    // setup the callback function for esp now
    esp_now_register_send_cb(onDataSent);

    // register peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer.");
      espNowInitialised = false;
    }

    // init the camera
    cam.aithinker();
    cam.highQuality();
    cam.highestSaturation();
    cam.vga();

    while (!cam.begin())
        Serial.println(cam.getErrorMessage());
}

void sendDataEspNow() {
  myData.id = id;
  myData.isPlastic = previousType && oldType && oldestType;

  Serial.println("Sending data.");

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.print("Data sent succesfully: isplastic: ");
    Serial.println(myData.isPlastic);
    // now that we've sent data we can reset
    objectDetected = false;
    loopsSinceObjectDetected = 0;
    loopsSinceDataSent = 0;
  } 
  else {
    Serial.println("Error sending the data");
    // Do not reset values and try again after this
  }
}

// if the esp now is initialised we send data through it, otherwise we print to the serial monitor what would've been sent
void sendData() {
  if (espNowInitialised) {
    sendDataEspNow();
  } else {
    Serial.println("esp now was not initialised so could not send any data when attempted.");
    Serial.print("Data that should've been sent: isPlastic:");
    Serial.println(previousType && oldType && oldestType);
  }

}

// global variable since we use it in a lambda expression
bool isNotPlastic;

void loop() {
    if (!cam.capture()) {
        Serial.println(cam.getErrorMessage());
        delay(1000);
        return;
    }

    // run FOMO model
    if (!fomo.detectObjects(cam)) {
        Serial.println(fomo.getErrorMessage());
        delay(1000);
        return;
    }

    // print found bounding boxes
    if (fomo.hasObjects()) {
        Serial.printf("Found %d objects in %d millis\n", fomo.count(), fomo.getExecutionTimeInMillis());
        // We detected an object
        objectDetected = true;
        if (objectDetected){
          loopsSinceObjectDetected++; // Since we guarantee that we send data and reset this to 0 we do not have to take into account what happens when this gets too big as it won't
        }
        // move the last types detected up one each
        oldestType = oldType;
        oldType = previousType;

  	    // check what the current type is
        // since we can have more than one object detected we check if any is NOT plastic
        isNotPlastic = false;
        fomo.forEach([](size_t ix, ei_impulse_result_bounding_box_t bbox) {
            Serial.print(" > BBox of label ");
            Serial.print(bbox.label);
            // Serial.print(" at (");
            // Serial.print(bbox.x);
            // Serial.print(", ");
            // Serial.print(bbox.y);
            // Serial.print("), size ");
            // Serial.print(bbox.width);
            // Serial.print(" x ");
            // Serial.print(bbox.height);
            Serial.println();


            if (!(bbox.label == "Bottle" || bbox.label == "Wrapper")) {
              isNotPlastic = true;
            }
        });
        // then we update the most recently detected type
        previousType = !isNotPlastic;
    }
    else {
        Serial.println("No objects detected");
    }

    // Update how long it has been since data was sent and cap how large it can get since we can have situations where objects are not detected for a long time
    loopsSinceDataSent++;
    if (loopsSinceDataSent >= 1000000) loopsSinceDataSent = 10;

    // if we've had at least 5 loops since the last object was detected for the first time then send data
    // but only send data if it has atleast been 10 loops since we've previously sent data
    if (loopsSinceObjectDetected >= 5 && loopsSinceDataSent >= 10) {
      sendData();
    }
}