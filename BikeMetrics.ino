#include <BlockDriver.h>
#include <FreeStack.h>
#include <MinimumSerial.h>
#include <SdFat.h>
#include <SdFatConfig.h>
#include <sdios.h>
#include <SysCall.h>

#define FS_NO_GLOBALS
// Error messages stored in flash.
#define error(msg) sd.errorHalt(F(msg))

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Ticker.h>
#include <FS.h>
#include <SPI.h>
//#include <SD.h>
#include <TinyGPS++.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>



// The TinyGPS++ object
TinyGPSPlus gps;

//Ticker used to call functions at given interval (declaring object 'timer')
Ticker timer;

//create instance of Temp sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// SSID and password for AP
char * ssid = "BikeMetrics";
char * password = "00001111";

float variableTime = 100;

//datalogging set to false until SD card recognised
const int chipSelect = 15;

//Web pages served true/false
bool indexServed = false;
bool cssServed = false;
bool jsServed = false;
bool bulmaServed = false;
bool inDataPage = false;
bool named = false;
bool card = false;
bool liveDisplay = false;

//GPS Variables
int gpsHrs;
int gpsMins;
int gpsSeconds;
int gpsDate;
char gpsDateSplit[9];
String dateHolder;
char gpsDateCompiled[9];
int gpsTime;
char gpsTimeSplit[9];
String timeHolder;
char gpsTimeCompiled[9];
float velocity;
int objectf;
int objectr;




//IMU variables
float LeanAngle;
float MaxLeanR = 0;
float MaxLeanL = 0;
float accel;
bool IMUFailure = false;

//bool to establish when SD card is being written to
bool writing = false;

//Define IMU samplerate
#define BNO055_SAMPLERATE_DELAY_MS (100)
Adafruit_BNO055 bno = Adafruit_BNO055();


// Running a web server
ESP8266WebServer server;

// Adding a websocket to the server
WebSocketsServer webSocket = WebSocketsServer(81);
WebSocketsServer webSocket2 = WebSocketsServer(82);

//datalogging variable
fs::File fsUploadFile;
char fileName[32];




SdFat sd;
SdFile myFile;
SdFile root;

void setup() {
  //start serial comms
  Serial.begin(9600);
  delay(500);

  //start temp sensors
  if (!mlx.begin())
  {
    Serial.println("MLX FAIL");
  } else {
    Serial.println("MLX OK");
  }

  delay(500);

  //start IMU
  if (!bno.begin())
  {
    Serial.println("IMU FAIL");
    IMUFailure = true;
  } else {
    Serial.println("IMU OK");
    IMUFailure = false;
  }
  //tells BNO055 to use external oscillator
  bno.setExtCrystalUse(true);

  delay(500);

  //start SPIFFS iot serve web pages
  if (SPIFFS.begin())
  {
    Serial.println("SPIFFS OK");
  } else {
    Serial.println("SPIFFS FAIL");
  }

  //start SD card if it exists
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    Serial.println("SD FAIL");
  } else {
    card = true;
    Serial.println("SD OK");
  }





  // Start AP
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();

  WiFiClient client;
  client.setNoDelay(true);

  //listen for server requests
  server.begin();
  server.on("/", serveIndexFile);
  server.on("/chart.min.js", serveJs);
  //server.on("/data", serveDataPage);
  server.on("/style.css", serveCSS);
  server.on("/bulma.min.css", serveBulma);
  server.on("/list", HTTP_GET, assignRoot);
  //  server.on("/rideList", printDirectory);
  //  server.on("/file", HTTP_POST, fileRequest);
  //  server.on("/dygraph.min.js", serveDygraphJs);
  //  server.on("/dygraph.min.css", serveDygraphCss);

  //Start websockets
  webSocket.begin();
  webSocket2.begin();
  webSocket2.onEvent(webSocketEvent);

  //call timer function
  attachTimer();

}


void loop() {



  //Server listen for clients
  server.handleClient();

  /*each call for a file detaches timer to give the
    server time to respond. only after serving all required files is data captured again*/
  if (indexServed == true && cssServed == true && jsServed == true && bulmaServed == true && inDataPage == false) {
    delay(0);
    indexServed = false;
    cssServed = false;
    jsServed = false;
    bulmaServed = false;
    attachTimer();
  } else {
    delay(0);
  }

  if (!inDataPage) {
    master();
  } else {}

}

void master() {

  //start websocket
  delay(0);
  webSocket.loop();
  webSocket2.loop();

  //Capture GPS data and call parseInfo func if exists
  while (Serial.available() > 0) {
    if (gps.encode(Serial.read()))
      parseInfo();
  }

}

//GPS parsing function
void parseInfo() {
  Serial.println("Parsed!");
  if (gps.time.isValid()) //Check Gps time is returning true before building filename
  {
    Serial.println("TimeValid!");
    
    gpsTime = gps.time.value();
    gpsDate = gps.date.value();
    
    gpsHrs = gps.time.hour() ;
    gpsMins = gps.time.minute() ;
    gpsSeconds = gps.time.second() ;

    if (named == false) {
      //make a string for filename from GPS time
      fileName[0] = '0' + gps.time.hour() / 10;
      fileName[1] = '0' + gps.time.hour() % 10;
      fileName[2] = '-';
      fileName[3] = '0' + gps.time.minute() / 10;
      fileName[4] = '0' + gps.time.minute() % 10;
      fileName[5] = '-' ;
      fileName[6] = '0' + gps.time.second() / 10;
      fileName[7] = '0' + gps.time.second() % 10;
      fileName[8] = '.' ;
      fileName[9] = 'c' ;
      fileName[10] = 's' ;
      fileName[11] = 'v' ;

      named = true;
      Serial.println(fileName);

    } else {}

  } else {
    Serial.println("GPS TIME FAIL");
  }

}


//GPS parsing function
//void parseInfo() {
//  if (gps.time.isValid())
//  {
//    gpsDate = gps.date.value();
//    dateHolder = String(gpsDate);
//    dateHolder.toCharArray(gpsDateSplit, 8);
//    gpsDateCompiled[0] = gpsDateSplit[0];
//    gpsDateCompiled[1] = gpsDateSplit[1];
//    gpsDateCompiled[2] = '/';
//    gpsDateCompiled[3] = gpsDateSplit[2];
//    gpsDateCompiled[4] = gpsDateSplit[3];
//    gpsDateCompiled[5] = '/';
//    gpsDateCompiled[6] = gpsDateSplit[4];
//    gpsDateCompiled[7] = gpsDateSplit[5];
//    gpsDateCompiled[8] = '\0';
//
//
//    gpsTime = gps.time.value();
//    timeHolder = String(gpsTime);
//    timeHolder.toCharArray(gpsTimeSplit, 8);
//    gpsTimeCompiled[0] = gpsTimeSplit[0];
//    gpsTimeCompiled[1] = gpsTimeSplit[1];
//    gpsTimeCompiled[2] = ':';
//    gpsTimeCompiled[3] = gpsTimeSplit[2];
//    gpsTimeCompiled[4] = gpsTimeSplit[3];
//    gpsTimeCompiled[5] = ':';
//    gpsTimeCompiled[6] = gpsTimeSplit[4];
//    gpsTimeCompiled[7] = gpsTimeSplit[5];
//    gpsTimeCompiled[8] = '\0';
//
//    gpsHrs = gps.time.hour() ;
//    gpsMins = gps.time.minute() ;
//    gpsSeconds = gps.time.second() ;
//
//    if (named == false) {
//      //make a string for filename
//      fileName[0] = '0' + gps.time.hour() / 10;
//      fileName[1] = '0' + gps.time.hour() % 10;
//      fileName[2] = '-';
//      fileName[3] = '0' + gps.time.minute() / 10;
//      fileName[4] = '0' + gps.time.minute() % 10;
//      fileName[5] = '-' ;
//      fileName[6] = '0' + gps.time.second() / 10;
//      fileName[7] = '0' + gps.time.second() % 10;
//      fileName[8] = '.' ;
//      fileName[9] = 'c' ;
//      fileName[10] = 's' ;
//      fileName[11] = 'v' ;
//      fileName[12] = '/' ;
//      fileName[13] = '0' ;
//
//      named = true;
//
//
//    }
//
//  } else {
//    Serial.println("GPS TIME FAIL");
//  }
//
//}



void getData() {
  if (gps.speed.isValid())
  {
    velocity = gps.speed.kmph();
  } else {
    Serial.println("GPS SPEED FAIL");
  }



  //Get Temp Data
  objectf = (mlx.readObjectTempC());
  objectr = ((objectf) - 2);

  //Get IMU Data
  sensors_event_t event;
  bno.getEvent(&event);

  //get Leanangle and determine max left and right
  LeanAngle = (event.orientation.y);
  if (LeanAngle > -5 && LeanAngle < 5) {
    LeanAngle = 0;
  } else {

  }
  if (event.orientation.y <= MaxLeanL) {
    MaxLeanL = (event.orientation.y);
  } else {}
  if (event.orientation.y >= MaxLeanR) {
    MaxLeanR = (event.orientation.y);
  } else {}

  //Determine accel
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);

  accel = (euler.y());

  if (liveDisplay) {

    String json = "{\"value\":"; //front tyre
    json += (objectf);
    json += ",";
    json += "\"value2\":"; // rear tyre
    json += (objectr);
    json += ",";
    json += "\"value3\":"; // Lean angle
    json += (LeanAngle);
    json += ",";
    json += "\"value4\":"; // Lean angle
    json += (MaxLeanL);
    json += ",";
    json += "\"value5\":"; // Lean angle
    json += (MaxLeanR);
    json += ",";
    json += "\"value6\":"; // Accel
    json += (accel);
    json += ",";
    json += "\"value7\":"; // long
    json += (gpsHrs);
    json += ",";
    json += "\"value8\":"; // lat
    json += (gpsMins);
    json += ",";
    json += "\"value9\":"; // lat
    json += (gpsSeconds);
    json += ",";
    json += "\"value10\":"; // lat
    json += (velocity);
    json += "}";
    webSocket.broadcastTXT(json.c_str(), json.length());

    json = "";

  } else {

  }

  writeToSD();
}

void writeToSD() {
  if (named && card) {

    String sdPayload;

    sdPayload += (gpsDate);
    sdPayload += (' ');
    sdPayload += (gpsTime);
    sdPayload += (',');
    sdPayload += (objectf);
    sdPayload += (',');
    sdPayload += (objectr);
    sdPayload += (',');
    sdPayload += (LeanAngle);
    sdPayload += (',');
    sdPayload += (accel);
    sdPayload += (',');
    sdPayload += (velocity);
    sdPayload += ("\n");

    myFile.open(fileName, O_RDWR | O_CREAT | O_AT_END);

    writing = true;

    myFile.print(sdPayload);

    Serial.println(sdPayload);

    // Force data to SD and update the directory entry to avoid data loss.

    myFile.flush();

    myFile.close();

    writing = false;

    sdPayload = "";

  } else {
    //WRITE TO ERROR LOG
  }
}


void attachTimer() {
  timer.attach_ms(variableTime, getData);
}


void serveIndexFile() {

  timer.detach();

  liveDisplay = true;

  fs::File file = SPIFFS.open("/index.html", "r");

  server.streamFile(file, "text/html");

  file.close();

  indexServed = true;

  inDataPage = false;

}



void serveCSS() {
  fs::File file = SPIFFS.open("/style.css", "r");

  server.streamFile(file, "text/css");

  file.close();

  cssServed = true;

}

void serveBulma() {
  fs::File file = SPIFFS.open("/bulma.min.css", "r");

  server.streamFile(file, "text/css");

  file.close();

  bulmaServed = true;

}

void serveJs() {
  fs::File file = SPIFFS.open("/chart.min.js", "r");

  server.streamFile(file, "application/javascript");

  file.close();

  jsServed = true;
}

//void serveDataPage() {
//
//  timer.detach();
//
//  if (writing) {
//
//    myFile.flush();
//
//    myFile.close();
//
//  } else {}
//
//  fs::File file = SPIFFS.open("/data.html", "r");
//
//  server.streamFile(file, "text/html");
//
//  file.close();
//
//
//}

//void serveRideList() {
//
//  timer.detach();
//
//  fs::File file = SPIFFS.open("/rideList.html", "r");
//
//  server.streamFile(file, "text/html");
//
//  file.close();
//
//  assignRoot();
//
//
//}

//void serveDygraphJs() {
//  fs::File file = SPIFFS.open("/dygraph.min.js", "r");
//
//  server.streamFile(file, "application/javascript");
//
//  file.close();
//}
//
//void serveDygraphCss() {
//  fs::File file = SPIFFS.open("/dygraph.min.css", "r");
//
//  server.streamFile(file, "text/css");
//
//  file.close();
//}

void assignRoot() {
  timer.detach();
  //inDataPage = true;
  printDirectory();
}

void printDirectory() {
  
  Serial.println("filelist requested");

  if (writing) {

    myFile.flush();

    myFile.close();

  } else {}
  
  root.open("/");

  myFile.rewind();

  String jsonList = "{";

  int i = 0;
  
  while (myFile.openNext(&root, O_RDONLY)) {

    i++;
    jsonList += "\"value";
    jsonList += i;
    jsonList += "\":\"";
    jsonList += myFile.printName();
    jsonList += "\",";

    myFile.close();
  }

  jsonList += "}";

  webSocket2.broadcastTXT(jsonList.c_str());

  jsonList = "";
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  if (payload[0] == 'R') {
    printDirectory();
  } else {}
}

//void fileRequest() {
//  if ( ! server.hasArg("csvfile")) {
//    server.send(400, "text/plain", "400: Invalid Request");
//    return;
//  }
//  if (server.hasArg("csvfile")) {
//    String csvFile = server.arg("csvfile");
//    File dlFile = SD.open(csvFile);
//
//    String buildPage;
//
//    buildPage += "<html>";
//    buildPage += "<a href='#' onclick='downloadCSV({ filename: \"data.csv\" });'>Download CSV</a>";
//    buildPage += "<head>";
//    buildPage += "<meta charset=\"utf-8\">";
//    buildPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
//    buildPage += "<title>BikeMetrics Ride Analysis</title>";
//    buildPage += "<script src=\"/dygraph.min.js\"></script>";
//    buildPage += "<script src=\"/dygraph.min.css\"></script>";
//    buildPage += "</head>";
//    buildPage += "<script>";
//
//    buildPage += "function downloadCSV(args) {";
//    buildPage += "var data, filename, link;";
//
//    buildPage += "var csv = document.getElementById(\"logData\").textContent;";
//    buildPage += "if (csv == null) return;";
//
//    buildPage += "filename = args.filename || 'export.csv';";
//
//    buildPage += "if (!csv.match(/^data:text\\/csv/i)) {";
//    buildPage += "csv = 'data:text/csv;charset=utf-8,' + csv;}";
//
//    buildPage += "data = encodeURI(csv);";
//
//    buildPage += "link = document.createElement('a');";
//    buildPage += "link.setAttribute('href', data);";
//    buildPage += "link.setAttribute('download', filename);";
//    buildPage += "link.click();}";
//
//    buildPage += "</script>";
//
//    server.sendContent (buildPage);
//
//    server.sendContent ("<div id=\"logData\" style =\"display: none\">");
//
//
//    if (dlFile) {
//      byte clientBuf[64];
//      int clientCount = 0;
//      while (dlFile.available()) {
//        if (clientCount >= 63) {
//          clientBuf[63] = '\0';
//          server.sendContent ((char*)clientBuf);
//          clientCount = 0;
//        }
//        clientBuf[clientCount] = dlFile.read();
//        clientCount++;
//
//      }
//      dlFile.close();
//
//      server.sendContent ("</div>");
//
//      String dygraph;
//
//      dygraph += "<div id = \"graphdiv\"> </div >";
//      dygraph += "<script type = \"text/javascript\">";
//      dygraph += "var csv = document.getElementById(\"logData\").textContent;";
//      dygraph += "g = new Dygraph(";
//
//      // containing div
//      dygraph += "document.getElementById(\"graphdiv\"),";
//
//      dygraph += "csv,";
//
//      dygraph += "{ labels: [\"x\", \"Front\", \"Rear\", \"Lean\", \"Accel\", \"Velocity\"] }";
//
//      dygraph += ");";
//      dygraph += "</script>";
//
//      server.sendContent (dygraph);
//
//      server.sendContent ("</html>");
//    }   //////ELSE STATEMENT TO GO HERE
//
//  }
//}
