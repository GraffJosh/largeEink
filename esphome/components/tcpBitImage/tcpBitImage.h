
#ifndef tcpBitImage_h
#define tcpBitImage_h

#include "Arduino.h"
#include <WiFi.h>
#include <vector>

#include "esphome/core/component.h"
#include <AsyncTCP.h>
#define DEBUG_ENABLE false
namespace esphome {
namespace tcpBitImage {

// public uart::UARTDevice, 
class TCPBitImage : public Print, public Component {
public:
  
  TCPBitImage();
  setImageParameters(const uint8_t width, const uint8_t height, const uint8_t density);
  setImageServer(String ip_address,uint16_t port);
  bool connectToServer();
  bool getImageReady();
  bool fetchImageData();
  ~TCPBitImage();


private:
  uint8_t* currentImage;
  uint8_t canvasWidth;
  uint8_t canvasHeight;
  uint8_t imageDensity;
  IPAddress serverIP;
  uint16_t serverPort;
  WiFiClient client;
  uint32_t bytesReady;
};

}
}
#endif