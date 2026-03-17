/*******************************************
 * see header-file for further informations
 ********************************************/

#include "Arduino.h"
#include "tcpBitImage.h"
    
namespace esphome {
  namespace tcpBitImage {

  
  static const char *TAG = "tcpBitImage.component";

  TCPBitImage::TCPBitImage() 
  {
  }

  void TCPBitImage::setImageParameters(const uint8_t width, const uint8_t height, const uint8_t density)
  {
    this->canvasWidth = width;
    this->canvasHeight = height;
    this->imageDensity = density;
  }

  void TCPBitImage::setImageServer(String ip_address,uint16_t port)
  {
    this->serverIP.fromString(ip_address);
    this->serverPort = port;

  }
  bool TCPBitImage::connectToServer()
  {
    if(serverIP == IPAddress(0,0,0,0) || serverPort == 0)
    {
      if(DEBUG_ENABLE)
        Serial.println("TCPBitImage: Server IP or Port not set!");
      
    }
    else if(this->client.connected())
    {
      return true;
    }
    else
    {
      this->client.connect(this->serverIP, this->serverPort);
    }
    return this->client.connected();
  }
  void TCPBitImage::getImageReady()
  {
    if(!this->connectToServer())
    {
      return false;
    }
    this->client.println("{\"command\":\"get_image_ready\",\"width\":" + String(this->canvasWidth) + ",\"height\":" + String(this->canvasHeight) + ",\"density\":" + String(this->imageDensity) + "}");
    this->client.println();
    uint8_t timeout = 1000;
    uint8_t delayDuration = 10;
    while(!this->client.available() && timeout > 0)
    {
      delay(delayDuration);
      timeout = timeout - delayDuration;
    }
    if(!(timeout > 0))
    {
      if(DEBUG_ENABLE)
        Serial.println("TCPBitImage: Timeout waiting for server response!");
      return false;
    }

    std::String response = "";
    if (client.available()) {
      char c = client.read();
      response.append(c);
      Serial.println(c);
    }
    this->bytesReady = atoi(response.c_str());
    if(this->bytesReady > 0)
    {
      return true;
    }else{
      if(DEBUG_ENABLE)
        Serial.println("TCPBitImage: IMAGE NOT READY!");
      return false;
    }
  }
  void TCPBitImage::fetchImageData()
  {
    if(!this->connectToServer())
    {
      return false;
    }
    this->client.println("{\"command\":\"get_image_data\"}"); 
    this->client.println();
    uint8_t timeout = 5000;
    uint8_t delayDuration = 10;
    while(!this->client.available() && timeout > 0)
    {
      delay(delayDuration);
      timeout = timeout - delayDuration;
    }
    if(!(timeout > 0))
    {
      if(DEBUG_ENABLE)
        Serial.println("TCPBitImage: Timeout waiting for server response!");
      return false;
    }
    uint32_t bytesReceived = 0;
    while (bytesReceived < this->bytesReady)
    {
      if(this->client.available())
      {
        char c = this->client.read();
        this->currentImage[bytesReceived] = c;
        bytesReceived++;
      }
    }
    
    return true;
  }

  void TCPBitImage::~TCPBitImage() 
  {
  
  }

  }


}