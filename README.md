# stabiliknee-detecting-improper-knee-movements
MIT Maker Portfolio Github Repository Containing All of StabiliKNEE's Major Parts

This project features StabiliKNEE, something I designed that consists of a knee sleeve and a sock (with embedded circuits) that flags when a user could be making an improper movement during exercise that puts them at risk for a knee injury. Overall, it acts as if it were a personal trainer without the high cost, and it aims to maximize the positive outcomes an athlete can get out of their training before matches. Small improper movements, like landing on the heel of your foot in a jump, can weaken the athlete's knee without them noticing and make them more susceptible to injury during a match; I made StabiliKNEE so _something_ will notice.

Circuitry Components (major ones, amplifiers/voltage regulators not included): 
ESP32 Devkit C (x2) with antennae
Adafruit LSM6DSO32 6-DoF Accelerometer + Gyroscope
Sparkfun Flex Sensor 4.5”
Amazon Force Sensitive Resistor Thin Film Pressure Sensor MD30-60 0~30KG Diameter 30mm Resistance-type Thin Film Pressure Sensor Force Sensing Resistor

This device has 2 code files, one for the knee sleeve and one for the sock, and they are connected via Bluetooth-Low-Energy (BLE). The knee sleeve takes all of the information from itself and the sock and prints it to the serial monitor. 

Please feel free to view the circuitry diagrams and overall CAD files/images that went into making StabiliKNEE! They are all in this Github repository.
