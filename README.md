# PlantSense microcontroller

This project contains the source code of the microcontroller of the [PlantSense](https://github.com/benschlegel/PlantSense-app) project. It is intended to be flashed on an `esp32` microcontroller.

## Setup

After cloning the project, open `plantsense_microcontroller/plantsense_microcontroller.ino` in `Arduino IDE`. Instructions on how to set up the `Arduino IDE` (download [here](https://www.arduino.cc/en/software)) to work with the `esp32` can be found [here](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/).

## Functionality

By default, the microcontroller stays in `setup`-mode. A wifi access point is opened that devices can connect to and set up credentials and a host.

Once configured, the `esp32` tries to connect to a wifi using stored credentials. When the connection is successful, it connects to the server specified by host and registers itself.

Additionally, it also hosts its own webserver on port `80`, that the server can use to send requests.

Full documentation coming soon.
