
#ifndef Waveshare75Grayscale_h
#define Waveshare75Grayscale_h

#include "Arduino.h"
#include <WiFi.h>
#include <vector>

#include "esphome/core/component.h"
#include <AsyncTCP.h>
#define DEBUG_ENABLE false
namespace esphome {
namespace waveshare75Grayscale {

// public uart::UARTDevice, 
class Waveshare75Grayscale : public Print, public Component {
public:
  
  Waveshare75Grayscale();
  
private:
  uint8_t* currentImage;
  uint8_t canvasWidth;
  uint8_t canvasHeight;
  uint8_t imageDensity;
};

}
}
#endif