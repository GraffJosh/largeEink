/*******************************************
 * see header-file for further informations
 ********************************************/

#include "Arduino.h"
#include "thermalprinter.h"
static const char LF = 0xA; // print buffer and line feed  
static const char ESC = 0x1B;
static const char GS = 0x1D;
    
namespace esphome {
namespace thermalprinter {

  
static const char *TAG = "printer_component.component";
Epson::Epson()
{
  // Serial1 = HardwareSerial::HardwareSerial(0);
  Serial1.begin( 115200, SERIAL_8N1, 20, 21, true );
  // this->_rxPin = rxPin;
  // this->_txPin = txPin;
  this->start();
}

void Epson::dump_config(){
    // ESP_LOG_INFO(TAG, "JPGIndustries Printer component");
    // int result = getStatus();
    // char string_buffer[(sizeof(result)) + 1];
    // memcpy(string_buffer, &result, sizeof(result));
    // string_buffer[sizeof(result)] = 0; // Null termination.
    // ESP_LOGCONFIG(TAG, string_buffer);
}
void Epson::start(){

  // pinMode(this->_txPin, OUTPUT);
  // pinMode(this->_rxPin, INPUT);  
  // this->_printer = new SoftwareSerial (this->_rxPin, this->_txPin);
  // this->_printer->begin(9600);
}

// query status of printer. when online returns value 22.
int Epson::getStatus(){
  Epson::write(0x10);    
  Epson::write(0x04);  
  Epson::write(1);
  int result;
  return result;
}

// int Epson::read(){
//     int result;
//     result = Serial1.read();
//     return result;
// }

void Epson::letterSpacing(int spacing){
  Epson::write(ESC);  
  Epson::write(0x20);
  Epson::write(spacing);    
}
void Epson::printLogo(int logoNum){
  Epson::justifyCenter();
  int logoIndex = 48+logoNum;
  //29  40  76  6  0  48  85  kc1  kc2  x  y
  Epson::write(29);  
  Epson::write(40);
  Epson::write(76);  
  Epson::write(6);  
  Epson::write(0);  
  Epson::write(48);  
  Epson::write(69);  
  Epson::write(48);  
  Epson::write(logoIndex);
  Epson::write(1);
  Epson::write(1);    
  Epson::justifyLeft();
}
// Print and feed n lines
// prints the data in the print buffer and feeds n lines
void Epson::feed(uint8_t n){
  Epson::write(ESC);  
  Epson::write(0x64);
  Epson::write(n);    
}

// Print one line
void Epson::feed(){
  this->feed(1);    
}

// Set line spacing
// sets the line spacing to n/180-inch
void Epson::lineSpacing(uint8_t n){
  Epson::write(ESC);  
  Epson::write(0x33);
  Epson::write(n);  
}

// Select default line spacing
// sets the line spacing to 1/6 inch (n=60). This is equivalent to 30 dots.
void Epson::defaultLineSpacing(){
  Epson::write(ESC);  
  Epson::write(0x32);
}

// Select an international character set
//  0 = U.S.A. 
//  1 = France 
//  2 = Germany 
//  3 = U.K. 
//  4 = Denmark I 
//  5 = Sweden 
//  6 = Italy 
//  7 = Spain 
//  8 = Japan 
//  9 = Norway 
// 10 = Denmark II 
// see reference for Details! 
void Epson::characterSet(uint8_t n){
  Epson::write(ESC);  
  Epson::write(0x52);
  Epson::write(n);  
}


void Epson::doubleHeightOn(){
  Epson::write(ESC);    
  Epson::write(0x21);  
  Epson::write(16);
}

void Epson::doubleHeightOff(){
  Epson::write(ESC);  
  Epson::write(0x21);    
  Epson::write(0);
}

void Epson::boldOn(){
  Epson::write(ESC);  
  Epson::write(0x21);    
  Epson::write(8);
}

void Epson::boldOff(){
  Epson::write(ESC);  
  Epson::write(0x21);    
  Epson::write(0);
}

void Epson::underlineOff() {
  Epson::write(ESC);  
  Epson::write(0x21);    
  Epson::write(0);
}
void Epson::underlineOn() {
  Epson::write(ESC);  
  Epson::write(0x21);    
  Epson::write(128);
}


// Turn white/black reverse printing mode on/off
void Epson::reverseOn() {
  Epson::write(GS);  
  Epson::write(0x42);    
  Epson::write(1);
}
  
void Epson::reverseOff() {
  Epson::write(GS);  
  Epson::write(0x42);    
  Epson::write(0);
}

void Epson::justifyLeft() {
  Epson::write(ESC);  
  Epson::write(0x61);    
  Epson::write(0);
}

void Epson::justifyCenter() {
  Epson::write(ESC);  
  Epson::write(0x61);    
  Epson::write(1);
}

void Epson::justifyRight() {
  Epson::write(ESC);  
  Epson::write(0x61);    
  Epson::write(2);
}
//n range 1-255
void Epson::barcodeHeight(uint8_t n) {
  Epson::write(GS);  
  Epson::write(0x68);    
  Epson::write(n);
}
//n range 2-6
void Epson::barcodeWidth(uint8_t n) {
  Epson::write(GS);  
  Epson::write(0x77);    
  Epson::write(n);
}
//n range 0-3
void Epson::barcodeNumberPosition(uint8_t n) {
  Epson::write(GS);  
  Epson::write(0x48);    
  Epson::write(n);
}
//m range 65-73 (code type)
//n (digit length)
void Epson::printBarcode(uint8_t m, uint8_t n) {
  Epson::write(GS);  
  Epson::write(0x6B);    
  Epson::write(m);
  Epson::write(n);
}

void Epson::cut() {
  Epson::write(GS);
  Epson::write('V');
  Epson::write(66);
  Epson::write(0xA); // print buffer and line feed
}
/// <summary>
/// Prints the image. The image must be 384px wide.
/// </summary>
/// <param name='image'>
/// Image to print.
/// </param>
int Epson::configureImagePage(const bool highDensity,const uint32_t width,const uint32_t height)
{
  imagePageMode = true;
  currentImageHeight = height;
  currentImageWidth = width;
  int configurationWidth = 0;
  int configurationHeight = 0;
  if (highDensity)
  {
    currentImageDensity = 33;
    configurationWidth = width*3;
    configurationHeight = height*2;
  }else{
    currentImageDensity = 0;
    configurationWidth = width;
    configurationHeight = height;
  }
  uint8_t dxL = configurationWidth & 255;
  uint8_t dxH = configurationWidth >> 8;
  uint8_t dyL = configurationHeight & 255;
  uint8_t dyH = configurationHeight >> 8;

  //trigger page mode.
  Epson::write(27);
  Epson::write(76);
  
  //begin set page size
  Epson::write(27);
  Epson::write(87);
  //start at 0,0
  Epson::write(0);
  Epson::write(0);
  Epson::write(0);
  Epson::write(0);

  //end at W, H (this is the split word paradigm again. Not sure about height configuration?)
  Epson::write(dxL);
  Epson::write(dxH);
  Epson::write(dyL);
  Epson::write(dyH);
  
  
  return configurationWidth;
  
}
int Epson::configureImage(const bool highDensity,const uint32_t width,const uint32_t height)
{
  imagePageMode = false;
  currentImageHeight = height;
  currentImageWidth = width;
  int configurationWidth = 0;
  int configurationHeight = 0;
  if (highDensity)
  {
    currentImageDensity = 33;
    configurationWidth = width*3;
    configurationHeight = height*3;
  }else{
    currentImageDensity = 0;
    configurationWidth = width;
    configurationHeight = height;
  }
  
  return configurationWidth;
  
}
void Epson::finishImage()
{
  Epson::write(12);
}
void Epson::printImageLine(const char* line_buffer, const int line_length, const bool highDensity)
{
  //parameters in the ESCPOS lib, these are a uint16 split in 2 denoting the 
  //  width of the line. 
  //align the line with our printer size.
  int current_width = (line_length);
  currentImageWidth = current_width;
  if(highDensity)
  {
    current_width = (line_length - (line_length % 3));
    currentImageWidth = current_width / 3;
  }
  uint8_t nL = currentImageWidth & 255;
  uint8_t nH = currentImageWidth >> 8;

  //enable unidirectional printing
  Epson::write(27);
  Epson::write(85);
  Epson::write(255);

  //set line spacing ?
  Epson::write(27);
  Epson::write(51);
  Epson::write(imagePageMode ? 24:0);
  
  //prepare for image
  Epson::write(27);//ESC
  Epson::write(42);//*
  Epson::write(highDensity ? 33 : 0);//changes the DPI (see manual)
  Epson::write(nL);//lower byte of the width
  Epson::write(nH);//upper byte of the width
  
  //write the data
  Epson::writeBytes(line_buffer, current_width);

  //newline to kick out the buffer
  Epson::write(13);
  Epson::write(10);

  //reset the unidirectional printing
  Epson::write(27);
  Epson::write(85);
  Epson::write(0);

  //reset the line spacing?
  Epson::write(27);
  Epson::write(51);
  Epson::write(24);

  //newline
  if(imagePageMode)
  {
    Epson::write(13);
    Epson::write(10);
  }
}

void Epson::speed(int inSpeed)
{
    Epson::write(GS);
    Epson::write(40);
    Epson::write(75);
    Epson::write(2);
    Epson::write(0);
    Epson::write(50);
    // char stringbuff[3];
    // itoa(inSpeed, stringbuff,10)
    if(inSpeed < 1)
    {
      inSpeed = 1;
    }
    if (inSpeed > 10)
    {
      inSpeed = 10;
    }
    Epson::write((uint8_t) inSpeed);
}

size_t Epson::write(uint8_t c) {
  Serial1.write(c);
  return 1;
}

size_t Epson::writeBytes(const char* inData,int length)
{
  size_t writtenBytes = 0;
  for (int i=0;i<length;i++){
    writtenBytes += Epson::write((uint8_t) inData[i]);
  }
  return writtenBytes;
}
// void Epson::printString(const char* text)
// {
//     // Traverse the string 
//   for (int i = 0;i<10; i++) { 
//     // Print current character 
//       Epson::write(text[i]); 
//   } 
//   Epson::write(LF);
// }

void Epson::logWrapback(const char* text)
{
  // ESP_LOGCONFIG(TAG, "wrapback: %s",text);
}


//TCP SERVER CODE.
//@TODO MOVE THIS TO ANOTHER CUSTOM COMPONENT
//This code integrates direct control over TCP commands.
//
//
//
void Epson::startTCPServer()
{
  if(serverStarted)
  {
    stopTCPServer();
  }
  if(DEBUG_ENABLE)
    Epson::print("Start TCP Server\n");
  tcpServer = WiFiServer(8888);
  tcpServer.begin();
  if(DEBUG_ENABLE)
    Epson::print("TCP Server started\n");
  serverStarted = true;
}

bool Epson::isAvailable()
{
  return serverStarted;
}
bool Epson::connected()
{
  if(serverStarted)
  {
    if(!clientConnected)
    {
      tcpClient = tcpServer.available();
    }
    if(tcpClient)
    {
      // tcpClient = &active_client;
      if (tcpClient.connected())
      {
        //this check effectively oneshots the debug statements
        if(!clientConnected)
        {
          if(DEBUG_ENABLE)
            Epson::print("TCP Client Connected\n");
          clientConnected = true;
        }
      }else{
        if(clientConnected)
        {
          if(DEBUG_ENABLE)
            Epson::print("TCP Client Disconnected\n");
          tcpClient.stop(); 
        }
        clientConnected = false;
      }
    }
  }
  return clientConnected;
}
bool Epson::hasData()
{
  if(tcpClient)
  {
    return (tcpClient.available());
  }else{
    return false;
  }
}
char Epson::read()
{
  return tcpClient.read();
}
int Epson::read(char * const line_buffer, int buf_size)
{
  int i=0;
  for(; i<buf_size;i++)
  {
    // if(Epson::hasData())
    // {
      line_buffer[i] = tcpClient.read();
    // }else{
    //   break;
    // }
  }
  return i;
}
void Epson::listenOnTCPServer()
{
  if (!serverStarted)
  {
    startTCPServer();
  }
  int i = 0;
  
  if(DEBUG_ENABLE)
    Epson::print("begin listen\n");

  while(i < 100)
  {
    // WiFiClient tcpClient = tcpServer.available();
    // Epson::print(tcpServer.available());
    // Epson::print("\n");
    // if (!tcpClient)
    // {
      // Epson::print(tcpClient.connected());
      // Epson::print("\nTCP Client didn't connect!\n");
    // }
    if (Epson::connected())
    {
      
      if(DEBUG_ENABLE)
        Epson::print("TCP Client Connected\n\n");
      this->tcpClient.connected();
      this->tcpClient.available();
      if(DEBUG_ENABLE)
        Epson::print("TCP Client is alive\n\n");

    }
    if (Epson::connected())
    {
      if(Epson::hasData())
      {
          // char c = this->tcpClient.read();
          Epson::print(Epson::read());
       }
      delay(100);
    }
    delay(100);
    i++;
  }
  
  if(DEBUG_ENABLE)
    Epson::print("End listen\n");
}
void Epson::disconnectTCPClient()
{
  if(tcpClient)
  {
    tcpClient.stop();
    // delete &tcpClient;
  }
  clientConnected = false;
}
void Epson::stopTCPServer()
{
  if(DEBUG_ENABLE)
    Epson::print("Stop TCP Server\n");
  tcpServer.stop();
  serverStarted = false;
}

// void Epson::initTCP(Epson printer)
// {
//   AsyncServer* server = new AsyncServer(8888); // start listening on tcp port 7050
// 	server->onClient((void*)&Epson::handleNewClient, server);
// 	server->begin();
// }
// /* server events */
// static void Epson::handleNewClient(void* arg, AsyncClient* client) {
// 	// Epson::printf("\n new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

// 	// add to list
// 	clients.push_back(client);
	
// 	// register events
// 	client->onData((void*)&Epson::handleData, NULL);
// 	client->onError((void*)&Epson::handleError, NULL);
// 	client->onDisconnect((void*)&Epson::handleDisconnect, NULL);
// 	client->onTimeout((void*)&Epson::handleTimeOut, NULL);
// }
// static void Epson::handleError(void* arg, AsyncClient* client, int8_t error) {
// 	// Epson::printf("\n connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
// }

// static void Epson::handleData(void* arg, AsyncClient* client, void *data, size_t len) {
// 	// Epson::printf("\n data received from client %s \n", client->remoteIP().toString().c_str());
// 	// Epson::printf((uint8_t*)data);

// 	// reply to client
// 	if (client->space() > 32 && client->canSend()) {
// 		char reply[32];
// 		// sprintf(reply, "this is from %s", SERVER_HOST_NAME);
// 		client->add(reply, strlen(reply));
// 		client->send();
// 	}
// }

// static void Epson::handleDisconnect(void* arg, AsyncClient* client) {
// 	// Epson::printf("\n client %s disconnected \n", client->remoteIP().toString().c_str());
// }

// static void Epson::handleTimeOut(void* arg, AsyncClient* client, uint32_t time) {
// 	// Epson::printf("\n client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
// }


}
}