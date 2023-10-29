# EngDesignSmartBin2023

Final Arduino sketches for the 2023 Engineering Design Smart Bin project

The Image collection sketch is only used to train a new image recognition model and not used within the final producct. Similarly the sketch to get the espnow mac adress is only used once to collect the mac adress of the Arduino nano esp (or other esp device) that will receive the data from the camera(s).

The image recognition code is used within the esp32cam and the nanofinal code is used in the Arduino nano esp.


To make use of the project follow the following procedure.

In case you want to train the image recogntion model further, upload the imagecollection sketch to the esp32cam and change the wifi ssid and password to your own wifi connection, this will allow you to connect to a server hosted by the espcam to collect images for your dataset. Once you've connected these images go to edge impulse and train an object detection model on it. (note that we use eloquent arduino library version 1.1.1 for this and the image recognition itself)

Included in the repository is the zip file for the trained image recognition model, simply add this as a library to Arduino manually to use the image recognition.

To use the image recognition simply upload the respective sketch to the espcam and set the macaddress of the Arduino nano esp (or other esp device) that you want to send data to (or simply read from the serial monitior), then no further input will be required from the user. Similarly the nano code can simply be uploaded and no further input should be required. See the final report for how to connect the different components to the nano physically.

The code base is designed to be easily expandeable with additional cameras and motors with mininal extra coding required. Simply setup additional cameras in the same way but change the id being sent along the espnow communication. Then within the Arduino nano it is a simple change to loop over a list of boards instead of over a select one and control motors based on which board in the list is being looped upon.
