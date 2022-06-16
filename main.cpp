//=========================
// Libraries
//=========================
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <HX711.h>
#include <Wire.h>

//=========================
// Compiler Constants
//=========================
// SAMPLE Service
#define SAMPLE_SERVICE_UUID "cb0f22c6-1000-4737-9f86-1c33f4ee9eea"
#define SAMPLE_LOAD_CELLS_CHARACTERISTIC_UUID "cb0f22c6-1001-41a0-93d4-9025f8b5eafe"

//=========================
// Device Instantiations
//=========================
// HX711 Load Cell Amplifier
HX711 scale;

//=========================
// Global Variables
//=========================
float calibration_factor = -24000; // Follow the SparkFun guide to get this value
bool client_is_connected = false;
// Load Cell Amplifier Pins
const int HX711_DOUT = GPIO_NUM_5;
const int HX711_CLK = GPIO_NUM_4;
// BLE Server
BLEServer *pServer;
// Characteristics: Load Cells
BLECharacteristic *loadCellCharacteristic;

//=========================
// State Machine Flags
//=========================
bool load_cell_sampling_enabled = false;

//=========================
// Function Headers
//=========================
void notifyWeight(void);
void setupBLEServer(void);
void setupSampleService(void);
void setupAdvertisementData(void);
void setupLoadCells(void);
void stateMachine(void);

//=========================
// Classes
//=========================
// BLE Callbacks
class BaseBLEServerCallbacks : public BLEServerCallbacks
// Callback triggered when a client device connects or disconnects
{
  void onConnect(BLEServer *pServer)
  {
    client_is_connected = true;
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer *pServer)
  {
    client_is_connected = false;
    Serial.println("Device disconnected");
    // Restart advertising
    pServer->getAdvertising()->start();
  }
};

class SampleLoadCellCallback : public BLECharacteristicCallbacks
{
  void onRead(BLECharacteristic *pCharacteristic)
  {
    Serial.println("SampleLoadCellCallback->onRead: Called");
    float weight = scale.get_units();
    Serial.print("Weight: ");
    Serial.print(weight, 1);
    Serial.println(" kg");
    std::string result = "{weight: " + std::to_string(weight) + "}";
    pCharacteristic->setValue(weight);
  }
};

class LoadCellDescriptorCallback : public BLEDescriptorCallbacks
// Callback triggered when loadcell descriptor listener is attached/removed
// Enables or disables sampling and notification of weight
{
  void onWrite(BLEDescriptor *pDescriptor)
  {
    Serial.println("LoadCellDescriptorCallback->onWrite: Called");
    u_int8_t desc = (*(pDescriptor->getValue()));
    Serial.println(std::to_string(desc).c_str());
    if (desc == 1)
    {
      Serial.println("Notify on");
      load_cell_sampling_enabled = true;
    }
    else
    {
      Serial.println("Notify off");
      load_cell_sampling_enabled = false;
    }
  }
};

void setup()
{
  // Setup USB Serial
  Serial.begin(115200);

  // Setup HX711 and Load Cells
  Serial.println("--Setting up HX711--");
  setupLoadCells();

  // Setup BLE
  Serial.println("--Setting up BLE Server--");
  setupBLEServer();
  setupSampleService();
  setupAdvertisementData();

  Serial.println("--Setup Complete--");
}

void loop()
{
  // ------------------
  // State Machine
  // ------------------
  if (client_is_connected)
  // Do nothing if no client is connected
  {
    stateMachine();
  }
}

void stateMachine(void)
{
  if (load_cell_sampling_enabled)
  {
    notifyWeight();
  }
}

void notifyWeight(void)
{
  float weight = scale.get_units(5);
  loadCellCharacteristic->setValue(weight);
  loadCellCharacteristic->notify();
}

void setupBLEServer(void)
{
  BLEDevice::init("BLE_SERVER");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BaseBLEServerCallbacks());

  BLEAddress mac_address = BLEDevice::getAddress();
  std::string address = mac_address.toString();
  Serial.println("BLE server setup: SUCCESS");
  Serial.print("MAC: ");
  Serial.println(address.c_str());
}

void setupSampleService(void)
{
  BLEService *sampleService = pServer->createService(SAMPLE_SERVICE_UUID);

  // Weight/Load Cell Sample Characteristic
  loadCellCharacteristic = sampleService->createCharacteristic(
      SAMPLE_LOAD_CELLS_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_NOTIFY);
  loadCellCharacteristic->setCallbacks(new SampleLoadCellCallback());
  loadCellCharacteristic->setValue("PENDING");

  // -- create CCC descriptors for notification service and listener callbacks --

  // Load Cell CCC descriptor
  BLEDescriptor *pLoadCellCCCDescriptor = new BLEDescriptor((uint16_t)0x2902);
  pLoadCellCCCDescriptor->setCallbacks(new LoadCellDescriptorCallback());
  loadCellCharacteristic->addDescriptor(pLoadCellCCCDescriptor);

  sampleService->start();

  Serial.println("setupSampleService setup: SUCCESS");
}

void setupAdvertisementData(void)
{
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  BLEAdvertisementData advertisementData;
  // Set properties of advertisement data
  pAdvertising->setAdvertisementData(advertisementData);
  pAdvertising->start();
}

void setupLoadCells(void)
{
  pinMode(HX711_CLK, OUTPUT);
  pinMode(HX711_DOUT, INPUT);

  scale.begin(HX711_DOUT, HX711_CLK);
  scale.set_scale(calibration_factor);
  scale.tare(); // Reset the scale to 0

  long zero_factor = scale.read_average(); // Get a baseline reading
  Serial.println("setupLoadCells setup: SUCCESS");
}
