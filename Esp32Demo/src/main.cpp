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
#define SERVICE_UUID "12a59900-17cc-11ec-9621-0242ac130002" // UART service UUID
#define CHARACTERISTIC_UUID_RX "12a59e0a-17cc-11ec-9621-0242ac130002"
#define CHARACTERISTIC_UUID_TX "12a5a148-17cc-11ec-9621-0242ac130002"

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

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string rxValue = pCharacteristic->getValue(); // 接收信息

    if (rxValue.length() > 0)
    {
      const uint8_t *data = reinterpret_cast<const uint8_t *>(rxValue.data());

      for (size_t i = 0; i < rxValue.length(); i++)
      {
        if (data[i] < 0x10)
          Serial.print("0"); // 补零
          Serial.print(data[i], HEX);
          Serial.print(' ');
        if (i == 0 && data[i] == 0x03)
        {
          byte hexData[] = {0x12, 0xAB, 0xCD}; // 十六进制报文数据
          pTxCharacteristic->setValue(hexData, sizeof(hexData)); // 设置要发送的字节数组
          pTxCharacteristic->notify(); // 广播
        }
      }
      Serial.println();
    }
  }
};

void setup()
{
  Serial.begin(115200);

  // 创建一个 BLE 设备
  BLEDevice::init("UART_BLE");

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