#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "common.h"

uint8_t txValue = 0;
BLEServer *pServer = NULL;                   // BLEServer指针 pServer
BLECharacteristic *pTxCharacteristic = NULL; // BLECharacteristic指针 pTxCharacteristic
bool deviceConnected = false;                // 本次连接状态
bool oldDeviceConnected = false;             // 上次连接状态

// See the following for generating UUIDs: https://www.uuidgenerator.net/
#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb" // UART service UUID
#define CHARACTERISTIC_UUID_RX "0000ffe2-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_TX "0000ffe1-0000-1000-8000-00805f9b34fb"

uint8_t headHeight = 0;
uint8_t seatHeight = 0;
uint8_t drag = 0;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

typedef struct
{
  int drag;
  int headHeight;
  int seatHeight;
  int vibrate;
} DeviceSettings;

DeviceSettings emptySettings = {0, 0, 0, 0}; // 定义一个特殊的空值对象

DeviceSettings deviceSettings;

DeviceSettings parseSettings(const uint8_t byteArray[], size_t length)
{
  DeviceSettings settings;
  if (byteArray[0] != 0xf9)
  {
    return emptySettings;
  }

  if (byteArray[1] != 0xc2)
  {
    return emptySettings;
  }
  settings.drag = byteArray[2];
  settings.headHeight = (int)byteArray[3] + ((int)byteArray[4] << 8);
  settings.seatHeight = (int)byteArray[5] + ((int)byteArray[6] << 8);
  settings.vibrate = byteArray[7];
  return settings;
}

// 写一个函数用于封包，返回字节数组用于ble发送
uint8_t *buildStatusCode()
{
  Serial.println("buildStatusCode start");

  uint8_t *byteArray = (uint8_t *)malloc(20);
  byteArray[0] = 0xf9;
  byteArray[1] = 0xc5;
  // 创建一个10000-30000的随机整数
  int press1 = random(10000, 30000);
  // 用byteArray[3]存储这个整数的低字节，byteArray[2]存储这个整数的高字节
  byteArray[2] = press1 & 0xff;
  byteArray[3] = (press1 >> 8) & 0xff;
  int press2 = random(10000, 30000);
  byteArray[4] = press2 & 0xff;
  byteArray[5] = (press2 >> 8) & 0xff;
  int press3 = random(10000, 30000);
  byteArray[6] = press3 & 0xff;
  byteArray[7] = (press3 >> 8) & 0xff;
  int press4 = random(10000, 30000);
  byteArray[8] = press4 & 0xff;
  byteArray[9] = (press4 >> 8) & 0xff;
  Serial.println("buildStatusCode mid");

  int freq = random(10, 255);
  byteArray[10] = freq & 0xff;
  int resistance = random(5, 25);
  byteArray[11] = resistance & 0xff;
  int relativeDrag = 36;
  byteArray[12] = relativeDrag & 0xff;
  byteArray[13] = 0x00;
  int seatHeight = random(0, 777);
  byteArray[14] = seatHeight & 0xff;
  byteArray[15] = (seatHeight >> 8) & 0xff;
  int headHeight = random(0, 880);
  byteArray[16] = headHeight & 0xff;
  byteArray[17] = (headHeight >> 8) & 0xff;
  // 添加校验，把byteArray[1]到byteArray[17]的所有字节相加，取最后2位，比如如果相加结果是0x1234，那么校验结果就是0x34
  Serial.println("buildStatusCode check");

  int sum = 0;
  for (int i = 1; i < 18; i++)
  {
    sum += byteArray[i];
  }
  byteArray[18] = sum & 0xff;
  byteArray[19] = 0xfd;

  Serial.println("buildStatusCode end");

  return byteArray;
}

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue(); // 接收信息

    if (rxValue.length() > 0)
    {
      const uint8_t *data = reinterpret_cast<const uint8_t *>(rxValue.data());

      // 检查收到的报文是否是f9c401befd
      if (data[0] == 0xf9 && data[1] == 0xc4 && data[2] == 0x01 && data[3] == 0xbe && data[4] == 0xfd)
      {
        uint8_t *byteArray = buildStatusCode(); // 十六进制报文数据
        pTxCharacteristic->setValue(byteArray, 20);
        pTxCharacteristic->notify(); // 广播
        Serial.println(" free start ");
        free(byteArray);
        Serial.println(" free end ");
      }

      // 检查收到的rxValue长度是否为10
      if (rxValue.length() == 10 && data[1] == 0xc2)
      {
        parseSettings(data, rxValue.length());
      }

      for (size_t i = 0; i < rxValue.length(); i++)
      {
        if (data[i] < 0x10)
          Serial.print("0"); // 补零
        Serial.print(data[i], HEX);
        Serial.print(' ');
      }
      Serial.println();
    }
  }
};

void setup()
{
  Serial.begin(115200);

  // 创建一个 BLE 设备
  BLEDevice::init("SL010-000001");

  // 创建一个 BLE 服务
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); // 设置回调
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 创建一个 BLE 特征
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks()); // 设置回调

  pService->start();                  // 开始服务
  pServer->getAdvertising()->start(); // 开始广播
  Serial.println(" 等待一个客户端连接，且发送通知... ");
}

void loop()
{
  // deviceConnected 已连接
  if (deviceConnected)
  {
    // pTxCharacteristic->setValue(&txValue, 1); // 设置要发送的值为1
    // pTxCharacteristic->notify();              // 广播
    // txValue++;                                // 指针地址自加1
    // delay(200);                              // 如果有太多包要发送，蓝牙会堵塞
  }

  // disconnecting  断开连接
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);                  // 留时间给蓝牙缓冲
    pServer->startAdvertising(); // 重新广播
    Serial.println(" 开始广播 ");
    oldDeviceConnected = deviceConnected;
  }

  // connecting  正在连接
  if (deviceConnected && !oldDeviceConnected)
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}