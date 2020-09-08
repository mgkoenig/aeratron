/*************************************************************************
 * 
 * AERATRON WEB INTERFACE
 * ----------------------
 * 
 * A simple web interface that provides a remote control functionality 
 * for an AERATRON fan (model AE2+). This application enables the same 
 * features as the original RF remote control (model AE+) 
 * of the manufacturer. 
 * 
 * Used hardware: ESP32 development board
 *                433MHz transmitter module
 * 
 * Author: Matthias Koenig, 2020
 * 
 */

/**********************************************************
 * INCLUDES
 *********************************************************/
#include "WiFi.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "time.h"


/**********************************************************
 * DEFINES
 *********************************************************/
#define VERSION_MAJOR     2
#define VERSION_MINOR     6
#define VERSION_PATCH     0

#define FAN_ADDRESS       0xF0    // depending on the dip switches of the original remote control
#define DATA_PIN          27      // gpio 27

#define GMT_OFFSET        0       // given in seconds (3600 for GMT+1)
#define DAYLIGHT_OFFSET   0       // given in seconds (3600 for +1h summer time)


/**********************************************************
 * GLOBALS
 *********************************************************/
const char* ssid      = "YOUR_SSID";
const char* password  = "YOUR_PASSWORD";
const char* ntpServer = "pool.ntp.org";

AsyncWebServer server(80);

char fan_ctrl;
char light_ctrl;


/**********************************************************
 * ENUMS
 *********************************************************/
enum light_state {
  LIGHT_STATE_ON  = 0xDF,
  LIGHT_STATE_OFF = 0x00 
};

enum fan_speed {
  FAN_SPEED_OFF = 0x00,
  FAN_SPEED_1   = 0x01,
  FAN_SPEED_2   = 0x02,
  FAN_SPEED_3   = 0x03,
  FAN_SPEED_4   = 0x04,
  FAN_SPEED_5   = 0x05,
  FAN_SPEED_6   = 0x06,
  FAN_SPEED_ON  = 0x07
};

enum fan_direction {
  FAN_DIRECTION_LEFT = 0x00,
  FAN_DIRECTION_RIGHT = 0x20
};


/**********************************************************
 * FUNCTION PROTOTYPES
 *********************************************************/
void set_light(enum light_state);
void set_speed(enum fan_speed);
void set_direction(enum fan_direction);
void send_command();
void printLocalTime();
String get_ui_date();


/**********************************************************
 * WEB PAGE
 *********************************************************/
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html> 
<html> 
<meta name="viewport" content="width=device-width, initial-scale=1">
<style> 
  .btn_state { 
    background-color: #ddd; 
    border: none; 
    color: black; 
    padding: 10px 20px; 
    text-align: center; 
    text-decoration: none; 
    display: inline-block; 
    margin: 4px 2px; 
    margin-bottom: 10px;
    cursor: pointer; 
    border-radius: 16px; 
    width: 166px; 
  } 
  .btn_state:hover { 
    background-color: #f1f1f1; 
  }      
  .btn_speed { 
    background-color: #ddd; 
    border: none; 
    color: black; 
    padding: 10px 20px; 
    text-align: center; 
    text-decoration: none; 
    display: inline-block; 
    margin: 4px 2px; 
    cursor: pointer; 
    border-radius: 16px; 
    width: 53px; 
  } 
  .btn_speed:hover { 
    background-color: #f1f1f1; 
  } 
</style>
            
<head> 
  <title>Aeratron Fan Control</title> 
</head> 

<body>
<center>
  <h2>Aeratron Fan Control</h2>
  <p><small>&copy; MGKOENIG 2020<br>(Version %UI_VERSION%)<br>%UI_DATE%</small></p><br>
  %CONTROL_PANEL%
</center>
</body>
</html>
)rawliteral";

String panel_builder(const String& var){
  //Serial.println(var);

  if(var == "UI_DATE"){
    String ui_date = "";

    ui_date = get_ui_date();

    return ui_date;
  }

  if(var == "UI_VERSION"){
    String ui_version = "";
    
    ui_version += VERSION_MAJOR;
    ui_version += ".";
    ui_version += VERSION_MINOR;
    ui_version += " | ";
    ui_version += "<a href=\"/changelog\">Changelog</a>";

    return ui_version;
  }
  
  if(var == "CONTROL_PANEL"){
    String panel = "";

    if ((fan_ctrl & 0x0F) == FAN_SPEED_OFF) {
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/fan/on';\">Fan ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/0';\">Fan OFF</button><br>";
    } else {
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/on';\">Fan ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/fan/0';\">Fan OFF</button><br>";
    }
    
    panel += '\n';
    if (light_ctrl == LIGHT_STATE_ON) {
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light/on';\">Light ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/light/off';\">Light OFF</button><br>";
    } else {
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/light/on';\">Light ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light/off';\">Light OFF</button><br>"; 
    }
    
    panel += '\n';
    if ((fan_ctrl & 0xF0) == FAN_DIRECTION_LEFT) {
        panel += "<button class=\"btn_state\" title=\"Summer Season\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/left';\">Rotate Left</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" title=\"Winter Season\" onclick=\"window.location.href='/fan/right';\">Rotate Right</button><br>";
    } else {
        panel += "<button class=\"btn_state\" title=\"Summer Season\" onclick=\"window.location.href='/fan/left';\">Rotate Left</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" title=\"Winter Season\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/right';\">Rotate Right</button><br>";
    }
    
    panel += '\n';
    if ((fan_ctrl & 0x0F) == FAN_SPEED_1) {
        panel += "<button class=\"btn_speed\" title=\"55rpm (4.4W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/1';\">1</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"55rpm (4.4W)\" onclick=\"window.location.href='/fan/1';\">1</button>";     
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_2) {
        panel += "<button class=\"btn_speed\" title=\"85rpm (5.6W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/2';\">2</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"85rpm (5.6W)\" onclick=\"window.location.href='/fan/2';\">2</button>";          
    }     
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_3) {
        panel += "<button class=\"btn_speed\" title=\"110rpm (7.6W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/3';\">3</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"110rpm (7.6W)\" onclick=\"window.location.href='/fan/3';\">3</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_4) {
       panel += "<button class=\"btn_speed\" title=\"130rpm (10.1W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/4';\">4</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"130rpm (10.1W)\" onclick=\"window.location.href='/fan/4';\">4</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_5) {
       panel += "<button class=\"btn_speed\" title=\"155rpm (13W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/5';\">5</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"155rpm (13W)\" onclick=\"window.location.href='/fan/5';\">5</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_6) {
       panel += "<button class=\"btn_speed\" title=\"185rpm (17.3W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/6';\">6</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"185rpm (17.3W)\" onclick=\"window.location.href='/fan/6';\">6</button>";          
    }   
    
    return panel;
  }
  return String();
}


/**********************************************************
 * SETUP
 *********************************************************/
void setup(){
  char header[50];
  uint8_t attempts = 0;
  Serial.begin(115200);

  snprintf(header, 50, "Aeratron Remote Web Client (Firmware: %d.%d)", VERSION_MAJOR, VERSION_MINOR);
  Serial.println( header );
  Serial.println( "==========================================" );
  Serial.println( "" );

  // initialize internal FS
  Serial.print("Mounting file system.. ");
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS.");
    return;
  }
  Serial.println(" Done.");

  Serial.print("Initialize state variables.. ");
  fan_ctrl = 0x00;
  light_ctrl = 0x00;
  Serial.println(" Done.");

  // Init pin
  Serial.print("Initialize output pins.. ");
  pinMode(DATA_PIN, OUTPUT);
  Serial.println(" Done.");

  Serial.print("Looking for favicon.. ");
  if (SPIFFS.exists("/favicon.png")) {
      Serial.println(" Found.");
  }
  else 
    Serial.println(" Error: Favicon not found!");

  Serial.print("Looking for touchicon.. ");
  if (SPIFFS.exists("/touchicon.png")) {
      Serial.println(" Found.");
  }
  else 
    Serial.println(" Error: Touchicon not found!");
 
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
 
  while ((WiFi.status() != WL_CONNECTED) && (attempts < 10)) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED){
    Serial.println("Resetting ESP...");
    ESP.restart();
  }
  Serial.println(" Done.");

  Serial.print("Server address: ");
  Serial.println(WiFi.localIP());

  // init and get the time
  configTime(GMT_OFFSET, DAYLIGHT_OFFSET, ntpServer);
  printLocalTime();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "Main Page.");
    request->send_P(200, "text/html", index_html, panel_builder);
  });
 
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png", "image/png");
  });

  server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/touchicon.png", "image/png");
  });

  server.on("/changelog", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/changelog.html", "text/html");
  });
  
  server.on("/fan/on", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_ON);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/0", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_OFF);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/1", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_1);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/2", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_2);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/3", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_3);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/4", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_4);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/5", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_5);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/6", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_6);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/left", HTTP_GET, [](AsyncWebServerRequest *request){
    set_direction(FAN_DIRECTION_LEFT);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/fan/right", HTTP_GET, [](AsyncWebServerRequest *request){
    set_direction(FAN_DIRECTION_RIGHT);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/light/on", HTTP_GET, [](AsyncWebServerRequest *request){
    set_light(LIGHT_STATE_ON);
    request->send_P(200, "text/html", index_html, panel_builder);
  });

  server.on("/light/off", HTTP_GET, [](AsyncWebServerRequest *request){
    set_light(LIGHT_STATE_OFF);
    request->send_P(200, "text/html", index_html, panel_builder);
  });
  
  server.begin();
}


/**********************************************************
 * LOOP
 *********************************************************/
void loop() {
  wl_status_t wifi_state;
  uint8_t err_cnt = 0;

  //Serial.print("Checking WiFI state... ");
  wifi_state = WiFi.status();  

  switch (wifi_state)
  {
    case WL_CONNECTED: err_cnt = 0; 
    case WL_NO_SHIELD:
    case WL_IDLE_STATUS:
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED: 
    case WL_CONNECT_FAILED: break;
    case WL_CONNECTION_LOST:
    case WL_DISCONNECTED: err_cnt++; break;
    default: break;
  }

  // No connection for more than 20 sec. Restart ESP.
  if (err_cnt > 4)
  {
    Serial.println("An error occurred. Resetting ESP!");
    ESP.restart();
  }

  //Serial.print("Error count: ");
  //Serial.println(err_cnt);
  delay(5000);  
}


/**********************************************************
 * FUNCTIONS
 *********************************************************/
void set_light(enum light_state ls)
{
  light_ctrl = ls;
  send_command();
}

void set_direction (enum fan_direction fd)
{
  fan_ctrl &= 0x0F;
  fan_ctrl |= fd;  
  send_command();
}

void set_speed (enum fan_speed fs)
{
  static enum fan_speed prev_speed = FAN_SPEED_1;

  switch (fs)
  {
    case FAN_SPEED_1:
    case FAN_SPEED_2:
    case FAN_SPEED_3:
    case FAN_SPEED_4:
    case FAN_SPEED_5:
    case FAN_SPEED_6: prev_speed = fs; break;
    case FAN_SPEED_ON: fs = prev_speed; break;
    case FAN_SPEED_OFF: break;  
  }

  fan_ctrl &= 0xF0;
  fan_ctrl |= fs;  
  
  send_command();
}

void send_command ()
{
  int i, j, k, l;
  char fan_command[3];

  fan_command[0] = FAN_ADDRESS;
  fan_command[1] = fan_ctrl;
  fan_command[2] = light_ctrl;

  // repeat each message 3x7 times
  for (l=0; l<3; l++) {
    for (k=0; k<7; k++) {
        // Start bit
        digitalWrite(DATA_PIN, LOW);          // sets the pin on
        delayMicroseconds(500);     
        digitalWrite(DATA_PIN, HIGH);         // sets the pin off
        delayMicroseconds(1000);    
        
        for (j=0; j<3; j++) {                 // for each command byte
          for (i=7; i>=0; i--) {              // each bit within the current byte (MSB first)
            if (fan_command[j] & (1 << i)) {  // sending a logic 1
              digitalWrite(DATA_PIN, LOW);      
              delayMicroseconds(500);     
              digitalWrite(DATA_PIN, HIGH);     
              delayMicroseconds(1000);    
            }
            else {                            // or sending a logic 0
              digitalWrite(DATA_PIN, LOW);      
              delayMicroseconds(1000);     
              digitalWrite(DATA_PIN, HIGH);     
              delayMicroseconds(500);   
            }
          }
        }
    
        digitalWrite(DATA_PIN, LOW); 
        delay(6);  
      }
      delay(10);  
    }
}

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

String get_ui_date()
{
  char timeStringBuff[50];
  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)){
    String err = "No time available";
    return err;
  }
  else
  {
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %d. %B %Y", &timeinfo);
    String time_string(timeStringBuff);
    return time_string;
  }
}
