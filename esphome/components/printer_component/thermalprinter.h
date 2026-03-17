/*******************************************
 *
 * Name.......:  Epson TM-T88II Library
 * Version....:  0.6.0
 * Description:  A Library to control an Epson TM-T88II thermal printer (microprinter) 
 *               by an arduino via serial connection
 * Project....:  https://github.com/signalwerk/thermalprinter
 * License....:  You may use this work under the terms of either the MIT License or 
                 the GNU General Public License (GPL) Version 3
 * Keywords...:  thermal, micro, receipt, printer, serial, tm-t88, tm88, tmt88, epson
 * ********************************************/
// reference: 
//https://www.pos.com.au/Help-SP/PrinterCommandsforEscPOSNonGraph.html
// https://download4.epson.biz/sec_pubs/pos/reference_en/escpos/ref_escpos_en/receipt_with_barcode.html
#ifndef thermalprinter_h
#define thermalprinter_h

#include "Arduino.h"
#include <WiFi.h>
#include <vector>

#include "esphome/core/component.h"
#include <AsyncTCP.h>
#include <HardwareSerial.h>
#define DEBUG_ENABLE false
namespace esphome {
namespace thermalprinter {
static std::vector<AsyncClient*> clients; // a list to hold all clients

// public uart::UARTDevice, 
class Epson : public Print, public Component {
public:
  
  Epson();
  

size_t write(uint8_t c);
size_t writeBytes(const char* inData,int length);

void dump_config() override;

void start();

int getStatus();
void boldOff();
void boldOn();
void characterSet(uint8_t n);
void defaultLineSpacing();
void doubleHeightOff();
void doubleHeightOn();
void feed(uint8_t n);
void feed();
void speed(int speed);
void letterSpacing(int spacing);
void lineSpacing(uint8_t n);
void reverseOff();
void reverseOn();
void underlineOff();
void underlineOn();
void justifyLeft();
void justifyCenter();
void justifyRight();
void barcodeHeight(uint8_t n);
void barcodeWidth(uint8_t n);
void barcodeNumberPosition(uint8_t n);
void printBarcode(uint8_t m,uint8_t n);
void cut();
void printString(const char* text);
void logWrapback(const char* text);
int configureImagePage(const bool highDensity,const uint32_t width,const uint32_t height);
int configureImage(const bool highDensity,const uint32_t width,const uint32_t height);
void finishImage();
void printImageLine(const char* line_buffer, const int line_length,const bool highDensity);
void printLogo(int logoNum);

// void initTCP(Epson printer);
// void handleNewClient(void* arg, AsyncClient* client);
// void handleError(void* arg, AsyncClient* client, int8_t error);
// void handleData(void* arg, AsyncClient* client, void *data, size_t len);
// void handleDisconnect(void* arg, AsyncClient* client);
// void handleTimeOut(void* arg, AsyncClient* client, uint32_t time);

void startTCPServer();
bool isAvailable();
void listenOnTCPServer();
void disconnectTCPClient();
void stopTCPServer();
bool connected();
bool hasData();
char read();
int read(char * const line_buffer, int buf_size);
private:  
WiFiServer tcpServer;
WiFiClient tcpClient;
bool serverStarted = false;
bool clientConnected = false;
bool imagePageMode = false;
uint32_t currentImageWidth = 0;
uint32_t currentImageHeight = 0;
int currentImageDensity = 1;

// HardwareSerial printerSerial;

};

}
}
#endif