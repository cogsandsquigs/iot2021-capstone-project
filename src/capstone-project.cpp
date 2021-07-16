/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "/home/pop-os/iot-capstone/iot2021-capstone-project/src/capstone-project.ino"
#include "MQTT.h"
#include "oled-wing-adafruit.h"
#include "string"
void setup();
void loop();
void callback(char *topic, byte *payload, unsigned int length);
void onDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context);
void iTempThreadFunc(void *args);
double getLargest(double *arr, int size);
double getSmallest(double *arr, int size);
double averageArray(double *arr, int size);
void isr();
#line 4 "/home/pop-os/iot-capstone/iot2021-capstone-project/src/capstone-project.ino"
using namespace std;

SYSTEM_THREAD(ENABLED);

OledWingAdafruit display;

MQTT client("lab.thewcl.com", 1883, callback);

const BleUuid serviceUuid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
const BleUuid rxUuid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
const BleUuid txUuid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");

BleCharacteristic txCharacteristic("tx", BleCharacteristicProperty::NOTIFY, txUuid, serviceUuid);
BleCharacteristic rxCharacteristic("rx", BleCharacteristicProperty::WRITE_WO_RSP, rxUuid, serviceUuid, onDataReceived, NULL);

bool BLEConnected = false;

double bTemp; // brandon's room temp
double iTemp; // ian's (my) room temp
double kTemp; // karissa's room temp
double vTemp; // vishal's room temp

long vProx; // vishal's room prox
long vLux;  // vishal's room lux

long kAccel;

long vProxThresh = 0;
long vLuxThresh = 0;
long kAccelThresh = 0;

bool wasAPressed = false;
bool wasBPressed = false;
bool wasCPressed = false;

volatile bool interruptOccured = true;
String triggerInterruptReason;
pin_t triggerInterrupt = D6;
pin_t interruptLight = D7;

os_thread_t iTempThread;

pin_t tempSensor = A0;

void setup()
{
  Serial.begin(9600);

  display.setup();
  display.clearDisplay();
  display.display();

  pinMode(tempSensor, INPUT);
  pinMode(interruptLight, OUTPUT);
  attachInterrupt(D6, isr, RISING); // D6 will now generate an interrupt on the falling edge and will run the code in the isr

  BLE.on();
  BLE.addCharacteristic(txCharacteristic);
  BLE.addCharacteristic(rxCharacteristic);

  BleAdvertisingData data;
  data.appendServiceUUID(serviceUuid);
  BLE.advertise(&data);

  os_thread_create(&iTempThread, "iTempThread", OS_THREAD_PRIORITY_DEFAULT, iTempThreadFunc, NULL, 1024);
}

void loop()
{
  display.loop();
  if (client.isConnected())
  {
    client.loop();
  }
  else
  {
    client.connect(System.deviceID());
    client.subscribe("groupB/#");
  }

  if (!BLE.connected())
  {
    client.publish("groupB/triggerInterrupt", "Ian's Argon is not connected via BLE");
    delay(5000);
  }

  double tempArray[3]{bTemp, iTemp, vTemp};

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  if (display.pressedA())
  {
    wasAPressed = true;
    wasBPressed = false;
    wasCPressed = false;
  }
  else if (display.pressedB())
  {
    wasAPressed = false;
    wasBPressed = true;
    wasCPressed = false;
  }
  else if (display.pressedC())
  {
    wasAPressed = false;
    wasBPressed = false;
    wasCPressed = true;
  }

  if (interruptOccured)
  {
    display.println(triggerInterruptReason);
    display.display();
    delay(5000);
    digitalWrite(interruptLight, LOW);
    interruptOccured = false;
    display.clearDisplay();
  }
  else if (wasAPressed)
  {
    Serial.printlnf("%f, %f, %f", bTemp, iTemp, vTemp);
    display.printlnf("hottest: %f", getLargest(tempArray, 3));
    display.printlnf("coldest: %f", getSmallest(tempArray, 3));
    display.printlnf("average: %f", averageArray(tempArray, 3));
  }
  else if (wasBPressed)
  {
    display.printlnf("vishal's prox: %d", vProx);
    display.printlnf("vishal's lux: %d", vLux);
    if (vProx >= vProxThresh)
    {
      display.printlnf("prox >= than threshold %d", vProxThresh);
    }
    if (vLux >= vLuxThresh)
    {
      display.printlnf("lux >= than threshold %d", vLuxThresh);
    }
  }
  else if (wasCPressed)
  {
    display.printlnf("karissa's accel: %d", kAccel);
    if (kAccel >= kAccelThresh)
    {
      display.printlnf("accel >= than threshold %d", kAccelThresh);
    }
  }
  else
  {
    display.println("press a button to see details");
  }
  display.display();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  if (!interruptOccured)
  {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    if (String(topic) == "groupB/brandonTemp")
    {
      bTemp = stod(p);
    }
    else if (String(topic) == "groupB/karissaTemp")
    {
      kTemp = stod(p);
    }
    else if (String(topic) == "groupB/karissaAccel")
    {
      kAccel = stol(p);
    }
    else if (String(topic) == "groupB/vishalTemp")
    {
      vTemp = stod(p);
    }
    else if (String(topic) == "groupB/vishalProx")
    {
      vProx = stoul(p);
    }
    else if (String(topic) == "groupB/vishalLux")
    {
      vLux = stoul(p);
    }
    else if (String(topic) == "groupB/triggerInterrupt")
    {
      digitalWrite(interruptLight, HIGH);
      triggerInterruptReason = String(p);
    }
  }
}

void onDataReceived(const uint8_t *data, size_t len, const BlePeerDevice &peer, void *context)
{
}

void iTempThreadFunc(void *args)
{
  while (1)
  {
    uint64_t reading = analogRead(tempSensor);
    double voltage = (reading * 3.3) / 4095.0;
    double temperature = (voltage - 0.5) * 100;
    iTemp = (1.8 * temperature) + 32;
    delay(1000);
  }
}

double getLargest(double *arr, int size)
{
  double l = 0;
  for (int i = 0; i < size; i++)
  {
    if (arr[i] > l)
    {
      l = arr[i];
    }
  }
  return l;
}

double getSmallest(double *arr, int size)
{
  double s = 999999999;
  for (int i = 0; i < size; i++)
  {
    if (arr[i] < s)
    {
      s = arr[i];
    }
  }
  return s;
}

double averageArray(double *arr, int size)
{
  double sum = 0;
  for (int i = 0; i < size; i++)
  {
    sum += arr[i];
  }
  return (sum / size);
}

void isr()
{
  // your interrupt handler code here
  interruptOccured = true;
}
