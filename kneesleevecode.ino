#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


//BLE UUIDs initialization
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID_TX "12345678-1234-1234-1234-123456789013" 
#define CHARACTERISTIC_UUID_RX "12345678-1234-1234-1234-123456789014"


//BLE objects
BLEServer*         server        = nullptr;
BLECharacteristic* Tx        = nullptr;  //sends data out to sock device
BLECharacteristic* Rx        = nullptr;  //master receives sensor data
bool               deviceisconnected = false;
String             datareceived   = "";
bool               newdata = false;


//pins for flex sensor
const int flexsensorpins[5] = {36, 39, 34, 35, 32};


//storing for calibration
int flexcalibration[5]   = {0, 0, 0, 0, 0};
int fsrcalibration[6]    = {0, 0, 0, 0, 0, 0};
int fsrlandings[6]     = {0, 0, 0, 0, 0, 0};


char mode = 0;


//squatingcounts
int hyperflexcount= 0;
int squatinternalrotationcount = 0;


//client data
struct ClientData {
  int   fsr[6];
  float gx;
};


//BLE server callbacks (gathered from Google after failures with data processing)
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceisconnected = true;
    Serial.println("\nSock connected.");
  }
  void onDisconnect(BLEServer* server) override {
    deviceisconnected = false;
    Serial.println("Sock disconnected. Retrying…");
    server->startAdvertising();
  }
};


//RX callbacks, read from client (sock)
class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* character) override {
    datareceived    = character->getValue().c_str();
    newdata = true;
  }
};


//helper functions, some done with the help of Google (just troubleshooting)
//readsflexpins
void readFlexSensors(int out[5]) {
  for (int i = 0; i < 5; i++) out[i] = analogRead(flexsensorpins[i]);
}
//checks if each flex sensor exceeds the calibrated thresholds
int flexExceedCount(int current[5], int calib[5], int threshold) {
  int count = 0;
  for (int i = 0; i < 5; i++)
    if (current[i] > calib[i] + threshold) count++;
  return count;
}
//checks the flex sensors that are below the calibrated thresholds
int flexBelowCount(int current[5], int calib[5], int threshold) {
  int count = 0;
  for (int i = 0; i < 5; i++)
    if (current[i] < calib[i] - threshold) count++;
  return count;
}


//blocks execution until serial monitor is ready 
char waitForSerial() {
  while (!Serial.available()) delay(50);
  char c = Serial.read();
  while (Serial.available()) Serial.read();
  return c;
}


//receive client data; created with the help of Google, had to troublsehoot several issues with reading client data
ClientData requestdata() {
  ClientData d;
  memset(&d, 0, sizeof(d));


  if (!deviceisconnected) return d;


  //sending request to read client
  newdata = false;
  Tx->setValue("READ");
  Tx->notify();


  //wait 500 milliseconds for a reply
  unsigned long start = millis();
  while (!newdata && millis() - start < 500) delay(10);


  if (!newdata) return d;


  //parsing the FSR indices (gathered from AP Computer Science A knowledge)
  String line = datareceived;


  int fsrIdx = line.indexOf("FSR:");
  if (fsrIdx >= 0) {
    String fsrPart = line.substring(fsrIdx + 4);
    for (int i = 0; i < 6; i++) {
      d.fsr[i] = fsrPart.toInt();
      int comma = fsrPart.indexOf(',');
      if (comma >= 0) fsrPart = fsrPart.substring(comma + 1);
    }
  }


  int gxIdx = line.indexOf("GX:");
  if (gxIdx >= 0) d.gx = line.substring(gxIdx + 3).toFloat();


  return d;
}


//jumping calibration (if user chooses jumping as their exercise)
void jumpingCalibration() {
  Serial.println(“Jumping Calibration”);
  Serial.println("Stand with knees bent ~90 degrees. Click y when ready.");
  while (waitForSerial() != 'y');
//reads flex sensors to see 90 degree values, gauging where hyperflexion could be in the simulation stage
  readFlexSensors(flexcalibration);
  Serial.print("Calibration recorded.");
  for (int i = 0; i < 5; i++) { Serial.print(flexcalibration[i]); Serial.print(" "); }
  Serial.println();


//requesting FSR data for standing calibrations
  ClientData cd = requestdata();
  for (int i = 0; i < 6; i++) fsrcalibration[i] = cd.fsr[i];
  Serial.print("FSR recorded.");
  for (int i = 0; i < 6; i++) { Serial.print(fsrcalibration[i]); Serial.print(" "); }
  Serial.println();


//detects jumping simulations by identifying if the fsr calibration values are higher than the current simulation values by at least 30 units.
  Serial.println("Now jump.");
  bool jumped = false;
  while (!jumped) {
    ClientData live = requestdata();
    int lowCount = 0;
    for (int i = 0; i < 6; i++)
      if (live.fsr[i] < fsrcalibration[i] - 30) lowCount++;
    if (lowCount >= 4) { jumped = true; Serial.println("You jumped!"); }
    delay(30);
  }
//starts landing part of simulation, checks to see if the live FSR readings are greater than those that were calibrated.
//I created all of the logic of these simulations, but I had to use Google to help me learn some of the code and processes.
  Serial.println("Waiting for landing...");
  bool landed = false;
  while (!landed) {
    ClientData live = requestdata();
    int highCount = 0;
    for (int i = 0; i < 6; i++)
      if (live.fsr[i] > fsrcalibration[i]) highCount++;
    if (highCount >= 4) {
      for (int i = 0; i < 6; i++) fsrlandings[i] = live.fsr[i];
      landed = true;
      Serial.println("Landing detected!");
    }
    delay(30);
  }
//prints FSR values
  Serial.print("Landing FSR values: ");
  for (int i = 0; i < 6; i++) { Serial.print(fsrlandings[i]); Serial.print(" "); }
  Serial.println();
  Serial.println("Calibration complete.");
  delay(2000);
}


//jumping simulation
void jumpingSimulation() {
  Serial.println("Jumping Simulation");
  Serial.println("Ready to begin jumping simulation? Press y!");
  while (waitForSerial() != 'y');


  while (true) {
    //waiting for jump, helped out by the calibration system: seeing if the live readings are less than the calibration readings - 30
    bool jumpDetected = false;
    while (!jumpDetected) {
      ClientData live = requestdata();
      int lowCount = 0;
      for (int i = 0; i < 6; i++)
        if (live.fsr[i] < fsrcalibration[i] - 30) lowCount++;
      if (lowCount >= 4) { jumpDetected = true; Serial.println("Jump detected"); }
      delay(30);
    }


    //waiting for landing, using same strategy as landing in calibration.
    bool landDetected = false;
    int  landFSR[6]   = {0};
    float landGX      = 0;
    while (!landDetected) {
      ClientData live = requestdata();
      int highCount = 0;
      for (int i = 0; i < 6; i++)
        if (live.fsr[i] > fsrcalibration[i]) highCount++;
      if (highCount >= 4) {
        for (int i = 0; i < 6; i++) landFSR[i] = live.fsr[i];
        landGX = live.gx;
        landDetected = true;
        Serial.println("Land detected");
      }
      delay(30);
    }


    //check heel landings
    //if the user landed on their back heel, the back 2 FSRS (four and five index) should be higher than the other four (0, 1, 2, 3)
    bool heelLand = (landFSR[4] > landFSR[0] || landFSR[4] > landFSR[1] ||
                     landFSR[4] > landFSR[2] || landFSR[4] > landFSR[3] ||
                     landFSR[5] > landFSR[0] || landFSR[5] > landFSR[1] ||
                     landFSR[5] > landFSR[2] || landFSR[5] > landFSR[3]);
    if (heelLand) Serial.println("WARNING: Land on back heel detected");


    //check internal rotation (gx, gyroscopic x direction)
    //for this, I checked accleration; however, I might change this to a double-integrated distance function in the future.
    if (landGX <= 15.0) Serial.println("WARNING: Internal Rotation Detected");


    //checking hyperflexion (Google facilitated this portion, but I created the overall code logic)
    unsigned long beginningflex = millis();
    bool excessiveFlexPrinted = false;
    while (millis() - beginningflex < 3000) {
      int curFlex[5];
      readFlexSensors(curFlex);
      if (!excessiveFlexPrinted && flexExceedCount(curFlex, flexcalibration, 10) >= 3) {
        Serial.println("WARNING: Excessive Flexion Detected");
        excessiveFlexPrinted = true;
      }
      delay(40);
    }


    //waiting for everything to go back to normal (starting position)
    Serial.println("Waiting for knee to return...");
    bool kneeReturned = false;
    //checking to see if at least 3 flex sensors are below values that the 90 degree calibration read
    while (!kneeReturned) {
      int curFlex[5];
      readFlexSensors(curFlex);
      if (flexBelowCount(curFlex, flexcalibration, 10) >= 3) kneeReturned = true;
      delay(50);
    }


    Serial.println("Ready for another jump? Click y");
    while (waitForSerial() != 'y');
  }
}


//squatting calibration, reusing the 90-degree squat part to be able to detect hyperflexion
void squattingCalibration() {
  Serial.println("Squatting calibration");
  Serial.println("Bend knees to ~90 degrees and hold. Click y when ready.");
  while (waitForSerial() != 'y');


  readFlexSensors(flexcalibration);
  Serial.print("Flex calibration recorded.");
  for (int i = 0; i < 5; i++) { Serial.print(flexcalibration[i]); Serial.print(" "); }
  Serial.println();
  Serial.println("Calibration complete. Starting simulation.");
  delay(1000);
}


//squatting simulation
void squattingSimulation() {
  Serial.println("Squatting simulation");
  Serial.println("Start squatting!");
  //setting variables to detect the times the user hyperflexes their knee or rotates their knee internally
  hyperflexcount= 0;
  squatinternalrotationcount = 0;


  while (true) {
    int curFlex[5];
    readFlexSensors(curFlex);


    //checks hyperflexion in each squat; can put strain on your knees, especially with overly heavy weights.
    //used Google for syntax help, otherwise all logic was created on my own via a flowchart and written documentation
    if (flexExceedCount(curFlex, flexcalibration, 0) >= 3) {
      hyperflexcount++;
      Serial.print("Over 90 degrees detected. x");
      Serial.println(hyperflexcount);
      delay(1500);
      bool returned = false;
      while (!returned) {
        int tmp[5];
        readFlexSensors(tmp);
        if (flexBelowCount(tmp, flexcalibration, 0) >= 3) returned = true;
        delay(50);
      }
    }


    //checking internal rotation from client data
    ClientData cd = requestdata();
    if (cd.gx <= 15.0) {
      squatinternalrotationcount++;
      Serial.print("Internal Rotation Detected. x");
      Serial.println(squatinternalrotationcount);
      delay(500);
    }


    delay(40);
  }
}


//setup function
void setup() {
  Serial.begin(115200);
  delay(1000);


  //initializes BLE conenction
  BLEDevice::init("Knee");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());


  BLEService* service = server->createService(SERVICE_UUID);


  //TX characteristic, seeing if client can transmit information
  Tx= service->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  Tx->addDescriptor(new BLE2902());


  //RX characteristic, seeing if it can receive data from client TX
  Rx = service->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  Rx->setCallbacks(new RxCallbacks());


  service->start();
  //starting BLE advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();


  Serial.println("BLE started, 'Knee'. Client waiting.");
  while (!deviceisconnected) {
    delay(500);
  }


  //choosing a setting
  Serial.println("\nChoose a setting:");
  Serial.println("p = Jumping, plyometrics");
  Serial.println("s = Squatting with load");


  while (mode != 'p' && mode != 's') mode = waitForSerial();
  //directs user to certain code functions depending on “p” or “s” response
  if (mode == 'p') {
    jumpingCalibration();
    jumpingSimulation();
  } else {
    squattingCalibration();
    squattingSimulation();
  }
}


//loop function (not needed)
void loop() {
}

