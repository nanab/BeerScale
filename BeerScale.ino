#include "HX711.h"
#include <ESP8266WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"
#include <ESP8266mDNS.h>
#include "ESP8266HTTPUpdateServer_edited.h"
#include <ArduinoJson.h>
#include <FS.h>
#include "ESP8266FtpServer.h"

//include webpages
#include "css.h"

//define text for webpage.
String pageHtmlLanguage = "sv-SE"; //html language for page
String pageTextSettings = "Inställningar"; //Settings
String pageTextGlassesLeft = "glas kvar";  //glasses left
String pageTextAmountLeft = "liter kvar"; //liters left
String pageTextBack = "Tillbaka"; //Back
String pageTextAdvanced = "Avancerat"; //Advanced settings
String pageTextSave = "Spara"; //Save
String pageTextCalibrate = "Kalibrering"; //Calibrate
String pageTextZeroValue = "Nollvärde"; //Zero value
String pageTextNewZeroValue = "Nytt nollvärde"; //New zero value
String pageTextActualWeight = "Aktuell vikt i gram"; //Actual weight in grams
String pageTextCalibrateFactor = "Kalibreringsfaktor"; //Calibration factor
String pageTextAmountWeightBeerGlass = "Antal gram öl per glas"; //Amount in grams of beer in a glas
String pageTextAmountWeightEmptyKeg = "Antal gram for tomt fat"; //Amount i grams of empty keg
String pageTextLabel = "Label"; //Beerlabel displayed on screen
String pageTextSaved = "Sparat!"; //Saved
 
 
//define settings file stored in flash
#define SCALE_CONFIG  "/scaleConfig.json"

// HX711 calibration values default values thats stored in settigns file on first start. 
float calibration_factor = -21300;
float zero_factor = -33872;

// Define our data and clock pins for scale
#define DOUT 14 // D5 maps to GPIO14
#define CLK 12  // D6 maps to GPIO12

//definde oled
#define OLED_RESET 1  // GPIO0
Adafruit_SSD1306 OLED(OLED_RESET);

// Initialize HX711 interface
HX711 scale(DOUT, CLK);

//Define webpage
const char* host = "BeerScale";
void handleRoot();
void handleCalibrate();
void handleSettings();
void calibratePlus100();
void calibratePlus1000();
void calibrateMinus100();
void calibrateMinus1000();
void getZeroFactor();
void handleSaveSettings();
void handleSaveCalibrate();

// Variable to store the HTTP request
String header;

//Define data deafult for settings file that will be created on first boot
float weightKeg = 1500; //keg empty weight in g
float beerWeightLiter = 1000;
float beerInGlasWeight = 300; //half pint weight in g
String beerLabel = "Beer Label";
float glasesLeft;
float beerLitersLeft;
float weightGrams;

//ftp server
FtpServer ftpSrv;

//Initilize webpage and updatepage
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// This is called if the WifiManager is in config mode (AP open)
void configModeCallback (WiFiManager *myWiFiManager) {
  
  OLED.clearDisplay();
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  OLED.setCursor(14,0);
  OLED.println("Access Point Mode");
  OLED.setCursor(35,10);
  OLED.println("BeerScaleAP");
  OLED.setCursor(35,20);
  OLED.println("192.168.4.1");
  OLED.display();
  
}

void setup() {
       
    //WiFi.hostname(host);
    Serial.begin(115200);
    Serial.println("Connecting");
    
    //oled boot text
    
    OLED.begin();
    delay(1000);
    OLED.clearDisplay();
    OLED.setTextSize(2);
    OLED.setTextColor(WHITE);
    OLED.setCursor(13,0);
    OLED.println("BeerScale");
    OLED.setTextSize(1);
    OLED.setCursor(40,20);
    OLED.println("Booting..");
    OLED.display();
    
    //WifiManager
    WiFiManager wifiManager;
    //
    //wifiManager.resetSettings();
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.autoConnect("BeerScaleAP");
    delay(2000);

    //Load config file. if it not exist creates it first then loads it.
    if (SPIFFS.begin()) {
      Serial.println("mounted file system");
      if (!SPIFFS.exists(SCALE_CONFIG)) {
        Serial.println("creating config file");
        saveConfigFile();
      }
      if (SPIFFS.exists(SCALE_CONFIG)) {
        // parse json config file
        File jsonFile = SPIFFS.open(SCALE_CONFIG, "r");
        if (jsonFile) {
          // Allocate a buffer to store contents of the file.
          size_t size = jsonFile.size();
          std::unique_ptr<char[]> jsonBuf(new char[size]);
          jsonFile.readBytes(jsonBuf.get(), size);
  
          DynamicJsonBuffer jsonBuffer;
          JsonObject& json = jsonBuffer.parseObject(jsonBuf.get());
          if (json.success()) {
            beerInGlasWeight = json["beerInGlasWeight"];
            weightKeg = json["weightKeg"];
            calibration_factor = json["calibration_factor"];
            zero_factor = json["zero_factor"];
            beerLabel = json["beerLabel"].as<String>();
            
          } else {
            Serial.println("failed to load json config");
          }
          jsonFile.close();
        } else {
          Serial.println("failed to load json config");
        }
      } else {
        Serial.println("No file exist");       
      }
    } else {
      Serial.println("failed to mount FS");
    }
    
    //Set scale offset and calibration values
    scale.set_scale(calibration_factor);
    scale.set_offset(zero_factor);

    
    MDNS.begin(host);

    //Define and start webpages 
    httpUpdater.setup(&server);
    server.on("/", handleRoot);
    server.on("/calibrate", handleCalibrate);
    server.on("/settings", handleSettings);
    server.on("/calibrateplus100", HTTP_GET, calibratePlus100);
    server.on("/calibrateplus1000", HTTP_GET, calibratePlus1000);
    server.on("/calibrateminus100", HTTP_GET, calibrateMinus100);
    server.on("/calibrateminus1000", HTTP_GET, calibrateMinus1000);
    server.on("/zerofactor", HTTP_GET, getZeroFactor);
    server.on("/savesettings", HTTP_POST, handleSaveSettings);
    server.on("/savecalibrate", HTTP_GET, handleSaveCalibrate);
    server.begin();
    MDNS.addService("http", "tcp", 80);
    ftpSrv.begin("admin", "admin"); // username, password for ftp. Set ports in ESP8266FtpServer.h (default 21, 50009 for PASV)
}

int updateOled(){
   
    weightGrams = scale.get_units() * 1000;
    float weightWhitoutKeg = weightGrams - weightKeg;
    glasesLeft = weightWhitoutKeg/beerInGlasWeight;
    beerLitersLeft = weightWhitoutKeg/beerWeightLiter;
    OLED.clearDisplay();
    OLED.drawRect(5, 0, 30, 10, WHITE);
    OLED.fillRect(5, 10, 30, 40, WHITE);
    OLED.drawLine(35, 10, 45, 10, WHITE);
    OLED.drawLine(45, 10, 45, 27, WHITE);
    OLED.drawLine(35, 27, 45, 27, WHITE);
    OLED.setTextSize(2);
    OLED.setTextColor(BLACK);
    OLED.setCursor(9,12);
    if (glasesLeft < 10){
      if (glasesLeft < 0){
        OLED.print(0);
        OLED.println(0);
      }else {
        OLED.print(0);
        OLED.println(glasesLeft, 0);
      }  
    } else {
      OLED.println(glasesLeft, 0);
    }
    OLED.setTextSize(1);
    OLED.setTextColor(WHITE);
    OLED.setCursor(60,5);    
    OLED.println(beerLabel);
    OLED.setTextSize(2);
    OLED.setTextColor(WHITE);
    OLED.setCursor(60,19);
    OLED.print(beerLitersLeft, 1);
    OLED.println("L");
    OLED.display(); //output 'display buffer' to screen   
      
}

void loop() {
    MDNS.update();
    server.handleClient();
    ftpSrv.handleFTP();
    updateOled();
}

void handleRoot() {
  String cssSite = CSS_page;
  String mainHead = "";
  String mainSite="";
  mainHead += "<!doctype html><html lang='" + pageHtmlLanguage + "'><head><meta http-equiv='refresh' content='2'>";
  mainSite += "<body><header><h1>BeerScale</h1></header>";
  mainSite += "<main><p>" + String(glasesLeft, 0) +" " + pageTextGlassesLeft + "</p>";
  mainSite += "<p>" + String(beerLitersLeft, 1) + " " + pageTextAmountLeft + "</p></main>";
  mainSite += "<footer><a href='/settings'><input type='button' value='" + pageTextSettings + "'></a></footer></body></html>";
  server.send(200, "text/html", mainHead + cssSite + mainSite);
}
void handleCalibrate() {
  String cssSite = CSS_page;
  String calSite = "";
  String calHead = "";
  calHead += "<!doctype html><html lang='" + pageHtmlLanguage + "'><head><meta http-equiv='refresh' content='2'>";
  calSite += "<body><header><h1>BeerScale</h1><h3>" + pageTextCalibrate + "</h3></header>";
  calSite += "<main><p>" + pageTextActualWeight + ": " + String(weightGrams, 0) + "</p><p>" + pageTextCalibrateFactor + ": " + String(calibration_factor, 0) + "</p>";
  calSite += "<section class='calibrate-buttons'><a href='/calibrateminus1000'><input type='button' value='-1000'></a><a href='/calibrateminus100'><input type='button' value='-100'></a><a href='/calibrateplus100'><input type='button' value='+100'></a><a href='/calibrateplus1000'><input type='button' value='+1000'></a></section>";
  calSite += "<p>" + pageTextZeroValue + ": " + String(zero_factor, 0) + "</p><a href='/zerofactor'><input type='button' value='" + pageTextNewZeroValue + "'></a><a href='/savecalibrate'><input type='button' value='" + pageTextSave + "'></a></main>";
  calSite += "<footer><a href='/'><input type='button' value='" + pageTextBack + "'></a><a href='/update'><input type='button' value='Firmware Update'></a></footer></body></html>";
  server.send(200, "text/html", calHead + cssSite + calSite);
}
void handleSettings(){
  String cssSite = CSS_page;
  String settSite = "";
  String settHead = "";
  settHead += "<!doctype html><html lang='" + pageHtmlLanguage + "'><head>";
  settSite += "<body><header><h1>BeerScale</h1><h3>" + pageTextSettings + "</h3></header>";
  settSite += "<main><form action='/savesettings' method='POST'><input type='number' name='glass' value='" + String(beerInGlasWeight, 0) + "'> " + pageTextAmountWeightBeerGlass + "<input type='number' name='keg' value='" + String(weightKeg, 0) + "'> " + pageTextAmountWeightEmptyKeg + "<input type='text' name='label' maxlength='11' value='" + beerLabel + "'> " + pageTextLabel + "<input type='submit' value='" + pageTextSave + "'></form></main>";
  settSite += "<footer><a href='/'><input type='button' value='" + pageTextBack + "'></a><a href='/calibrate'><input type='button' value='" + pageTextAdvanced + "'></a></footer></body></html>";  
  server.send(200, "text/html", settHead + cssSite + settSite);
  
}
void calibratePlus100() {
  calibration_factor += 100;
  scale.set_scale(calibration_factor);
  server.sendHeader("Location","/calibrate");
  server.send(303);  
}
void calibratePlus1000() {
  calibration_factor += 1000;
  scale.set_scale(calibration_factor);
  server.sendHeader("Location","/calibrate");
  server.send(303);  
}
void calibrateMinus100() {
  calibration_factor -= 100;
  scale.set_scale(calibration_factor);
  server.sendHeader("Location","/calibrate");
  server.send(303);  
}
void calibrateMinus1000() {
  calibration_factor -= 1000;
  scale.set_scale(calibration_factor);
  server.sendHeader("Location","/calibrate");
  server.send(303);  
}
void getZeroFactor(){
  scale.set_scale();
  scale.tare();
  zero_factor = scale.read_average();
  Serial.println(zero_factor, 0);
  scale.set_scale(calibration_factor);
  scale.set_offset(zero_factor);
  server.sendHeader("Location","/calibrate");
  server.send(303);
}
void handleSaveSettings(){
  String cssSite = CSS_page;
  String settSaveHead = "";
  String settSaveSite = "";
  settSaveHead += "<!doctype html><html lang='" + pageHtmlLanguage + "'><head>";
  settSaveSite += "<body><header><h1>BeerScale</h1><h3>" + pageTextSettings + "</h3></header>";
  
  if( ! server.hasArg("glass") || ! server.hasArg("keg") ||  ! server.hasArg("label") || server.arg("glass") == NULL || server.arg("keg") == NULL) { // If the POST request doesn't have username and password data
    settSaveSite += "<main>Errorl!</main>";
    settSaveSite += "<footer><a href='/settings'><input type='button' value='" + pageTextBack + "'></a></footer></body></html>";
    server.send(200, "text/plain", settSaveHead + cssSite + settSaveSite);         // The request is invalid, so send HTTP status 400
    return;
  } else {
    String tempweightKeg = server.arg("keg");
    String tempbeerInGlasWeight = server.arg("glass");
    String tempbeerLabel = server.arg("label");
    weightKeg = tempweightKeg.toFloat();
    beerInGlasWeight = tempbeerInGlasWeight.toFloat();
    beerLabel = tempbeerLabel;
    saveConfigFile();
    settSaveSite += "<main>" + pageTextSaved + "</main>";
    settSaveSite += "<footer><a href='/settings'><input type='button' value='" + pageTextBack + "'></a></footer></body></html>";
    server.send(200, "text/html", settSaveHead + cssSite + settSaveSite);
  }
}
void handleSaveCalibrate(){
  saveConfigFile();
  String cssSite = CSS_page;
  String calSaveHead = "";
  String calSaveSite = "";
  calSaveHead += "<!doctype html><html lang='" + pageHtmlLanguage + "'><head>";
  calSaveSite += "<body><header><h1>BeerScale</h1><h3>Kalibrering</h3></header>";
  calSaveSite += "<main>" + pageTextSaved + "</main>";
  calSaveSite += "<footer><a href='/calibrate'><input type='button' value='" + pageTextBack + "'></a></footer></body></html>";
  server.send(200, "text/html", calSaveHead + cssSite + calSaveSite);
}
void saveConfigFile(){
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["calibration_factor"] = calibration_factor;
  json["zero_factor"] = zero_factor;
  json["weightKeg"] = weightKeg;
  json["beerInGlasWeight"] = beerInGlasWeight;
  json["beerLabel"] = beerLabel;
  File configFile = SPIFFS.open(SCALE_CONFIG, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  json.prettyPrintTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}
