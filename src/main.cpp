/*
 * 
 *                                                          SEGRGB
 *                                
 *                                              Controls RGB LED strip with PWM 
 *                                                                                                 
 *                                                    - Seglectic Softworks 2019
 */

 
/****************************************************************************************************************************************************************
 *  
 *                                                          Libraries 📚
 *  
 ****************************************************************************************************************************************************************/ 

#include <Arduino.h>
#include <FS.h>                                     // SPIFFS File system https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
#include <ESP8266WiFi.h>                            // Main WiFi library https://github.com/esp8266/Arduino
#include <WiFiManager.h>                            // WiFi Manager for captive library and autoconnect https://github.com/tzapu/WiFiManager
#include <DNSServer.h>                              // ESP8266 DNS required for captive portal
#include <ESP8266WebServer.h>                       // Main web server library
#include <WiFiUDP.h>                                // UDP required for NTP & OTA updates
#include <NTPClient.h>                              // Network Time Protocol client
#include <ArduinoOTA.h>                             // Allows updating firmware wirelessly https://docs.platformio.org/en/latest/platforms/espressif8266.html#over-the-air-ota-update






/****************************************************************************************************************************************************************
 *  
 *                                                          Web Server File Handling 📂
 *  
 ****************************************************************************************************************************************************************/ 

ESP8266WebServer server(80);                             //Create web server

/*            
 *             getContentType()         
 *            
 *    Function to return 'Content-Type'
 *    based on file extension for
 *    proper client formatting
 */
String getContentType(String filename) { 
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

/*            
 *            handleFileRead()
 *             
 *    Sends the right file to the client
 *    and sends the file else returns false
 */
bool handleFileRead(String path) { 
  Serial.println("Serving file: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file (LOCAL LINKS TO FOLDERS MUST END WITH / )
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    server.streamFile(file, contentType);               // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  return false;                                         // If the file doesn't exist, return false
}


/****************************************************************************************************************************************************************
 *  
 *                                                      RGB LED GPIO Setup 🚥
 *  
 ****************************************************************************************************************************************************************/ 

String LightMode = "colorSet";
String Version = "0.8-OTA";

//RGB Pins
int rPin = 16;
int gPin = 5;
int bPin = 4;

//Destination to interp colors to
float dR = 0;
float dG = 0;
float dB = 0;

// Holds actual current color value
float R = 0;
float G = 0;
float B = 0;

//Color Interpolation strength modifier
float iPower = 0.3;

// Send RGB Data to lights
// ex: http://192.168.1.123/set?r=100&g=100&b=255
void RGBSet(){
  if(!server.arg("r") || !server.arg("g") || !server.arg("b")){
    server.send(200,"text/plain","Insufficient Color Parameters");
    return;
  }
  dR = server.arg("r").toInt();
  dG = server.arg("g").toInt();
  dB = server.arg("b").toInt();

  String response = "Lights set to: ";
  response+= server.arg("r")+", ";
  response+= server.arg("g")+", ";
  response+= server.arg("b")+" ";
  server.send(200, "text/plain", response);

  Serial.print("Color set to: ");
  Serial.print(server.arg("r")+", ");
  Serial.print(server.arg("g")+", ");
  Serial.println(server.arg("b") );
}

// Interpolate colors to desired values
// and write values to GPIO pins.
void colorSet(){
  R += (dR-R)*iPower;
  G += (dG-G)*iPower;
  B += (dB-B)*iPower;
  analogWrite( rPin, (int)R );
  analogWrite( gPin, (int)G );
  analogWrite( bPin, (int)B );
}


//Function to make lights pulsate red
void heatPulse(){

}

/****************************************************************************************************************************************************************
 *  
 *                                                          NTP & Alarm Setup ⏰
 *  
 ****************************************************************************************************************************************************************/ 

WiFiUDP ntpUDP;                                                    // Configure NTP Client with -14400 seconds                                     
NTPClient timeClient(ntpUDP,"pool.ntp.org",-14400,86400000);     // offset for GMT-5 and 24h update interval (8.64e+7ms)
// NTPClient timeClient(ntpUDP,"pool.ntp.org",-14400,60);             // offset for GMT-5 and 60 second update interval

// Load alarm from file here instead
int alarmDay = 4;
int alarmHour = 10;
int alarmMinute = 0;
String alarmMode = "ARMED";      //Current alarm state; ARMED, ALARMING, DISARMED
float alarmColor[3] = {255, 255, 255};

// Returns true if alarm should be fired 
bool alarmCheck(){
  if(timeClient.getHours() == alarmHour && 
     timeClient.getMinutes() == alarmMinute &&
     timeClient.getDay() == alarmDay &&
     alarmMode == "ARMED"
){
    return true; 
    alarmMode = "ALARMING";     //Toggle on alarm, so it may slowly brighten
  }else{
    return false;
  }
}

// Performs alarm routine to brighten lights
void alarmUpdate(){

  // Serial.println(timeClient.getFormattedTime());

  if (alarmCheck()){
    Serial.println("Firing alarm!");
  }
  if(alarmMode == "ALARMING"){

      if(dR == alarmColor[0] &&  // Check if lights == dest colors
         dG == alarmColor[1] &&  //  and disarm.
         dB == alarmColor[2]
      ){alarmMode = "DISARMED";}         
    
      //This needs to interpolate colors to dest
      dR = alarmColor[0];
      dG = alarmColor[1];
      dB = alarmColor[2];
  }


}

/****************************************************************************************************************************************************************
 * 
 *                                                        Web Response Handlers 👋
 *     
 ****************************************************************************************************************************************************************/ 


/*            
 *            handleGeneric()
 *             
 *    Example response handler with a
 *    sample parameter check and response
 *    
 */

void getVersion(){
  server.send(200,"text/plain",Version);
}

void handleGeneric() { 
  String response = "Number of Parameters: ";                     // Create response string to send
  response += server.args();                                      // Get number of parameters
  response += "\n";                                               // New line (<br> for html response)
  for (int i = 0; i < server.args(); i++) {  
    response += "Arg #" + (String)i + " -> ";                     // Include the current iteration value
    response += server.argName(i) + ": ";                         // Get the name of the parameter
    response += server.arg(i) + "\n";                             // Get the value of the parameter
  } 
  if(server.arg("blep")){                                         // Check if parameter exists
    response+="Client sent a blep parameter!!!\n";
  }
  server.send(200, "text/plain", response);                       // Respond to the HTTP request with plaintext
}

//Append color data received to file
void saveColor(){
  String response = "Received save data\n__________________\n";
  
  if(server.arg("data")){
    response+=server.arg("data");                                 //Append data to plain text response
    Serial.println("Saving data: "+server.arg("data"));
  
    // Save data to file
    File save = SPIFFS.open("/savedColors.txt","a");              // Open save file in append mode
    if(!save){Serial.println("failed to open save file");}        // Check if file opened
    save.println( server.arg("data") );                           // Save color in data param
    save.close();                                                 // Close it out
    server.send(200,"text/plain",response);
  }
}

//Load savedColors.txt and send via plainText
void loadColors(){
  File load = SPIFFS.open("/savedColors.txt","r");                 // Load save file in read mode
  if(!load){Serial.println("failed to open save file");}           // Check if file opened
  server.streamFile(load, "text/plaintext");                       // And send it to the client as plain ol' text
  load.close();
}


void deleteColor(){
  File savedColors = SPIFFS.open("/savedColors.txt","r");                 // Load save file in read mode
  savedColors.close();
}

//Rebuild savedColors.txt when entry is deleted
void colorData(){
  String response = "Color data: \n";
  response+=server.arg("data");
  server.send(200,"text/plain",response);
}


/***************************************************************************************************************************************************************
 *
 * 
 *  
 *                                                          Main Setup 🌱
 * 
 * 
 *  
 ****************************************************************************************************************************************************************/ 

void setup() {
  Serial.begin(115200);

  //Setup WiFi Manager
  WiFiManager wifiManager;
  wifiManager.autoConnect("segSetup","seglectic");                //Autoconnect; else start portal with this SSID, P/W



 /*******************************
  * 
  *      Setup OTA Update
  * 
  *******************************/

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("segStripRGB");

  //Set OTA password
  ArduinoOTA.setPassword((const char *)"Seglectique");


  

  /*******************************
   * 
   *    Assign GET handlers
   * 
   *******************************/
  server.on("/generic",handleGeneric);
  server.on("/saveColor",saveColor);
  server.on("/loadColors",loadColors);
  server.on("/set",RGBSet);
  server.on("/version",getVersion);
  server.on("/colorData",colorData);
  
  //Serves file to client
  server.onNotFound([]() {                                        // If the client requests any URI
    if (!handleFileRead(server.uri()))                            // send it if it exists
      server.send(404, "text/plain", "404'd");                    // otherwise, respond with a 404 (Not Found) error
  });



  server.begin();                                                 // Starts file server
  SPIFFS.begin();                                                 // Starts local file system
  timeClient.begin();                                             // Starts NTP client
  ArduinoOTA.begin();                                             // Starts OTA service


 

}




/****************************************************************************************************************************************************************
 * 
 * 
 * 
 *                                                          Main Loop ➰
 * 
 * 
 *  
 ****************************************************************************************************************************************************************/ 

void loop() {
  server.handleClient();
  
  if(LightMode=="colorSet"){
    colorSet();
  } else if (LightMode == "heatPulse"){
    heatPulse();
  }

  //Needs to allow colorSet until ALARMING
  if(alarmMode == "ALARMING"){
    dR=500;
    dG=80;
    dB=980;
  }


  timeClient.update();
  ArduinoOTA.handle();
  
  // alarmUpdate();

  delay(20);

}