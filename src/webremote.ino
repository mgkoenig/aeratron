/**********************************************************
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
 *********************************************************/

/**********************************************************
 * INCLUDES
 *********************************************************/
#include "WiFi.h"
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "ArduinoJson.h"
#include "SPIFFS.h"
#include "time.h"


/**********************************************************
 * DEFINES
 *********************************************************/
#define VERSION_MAJOR     2
#define VERSION_MINOR     7
#define VERSION_PATCH     4

#define FAN_ADDRESS       0xF0
#define DATA_PIN          27
#define LOG_ENTRY_MAX     10

#define GMT_OFFSET        0       // given in seconds (3600 for GMT+1)
#define DAYLIGHT_OFFSET   0       // given in seconds (3600 for +1h summer time)

#define UINT32_MAX        ((uint32_t)-1)


/**********************************************************
 * GLOBALS
 *********************************************************/
const char* ssid      = "YOUR_SSID";
const char* password  = "YOUR_PASSWORD";
const char* ntpServer = "pool.ntp.org";

AsyncWebServer server(80);
StaticJsonDocument<2000> syslog;    // allocate memory for about 10 syslog entries
File syslogJsonFile;

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

enum time_format {
  TIMEFORMAT_DAY,
  TIMEFORMAT_DAYDATE,
  TIMEFORMAT_DATE, 
  TIMEFORMAT_DATE_SHORT,
  TIMEFORMAT_TIME, 
  TIMEFORMAT_DATETIME
};


/**********************************************************
 * FUNCTION PROTOTYPES
 *********************************************************/
void set_light(enum light_state ls);
void set_speed(enum fan_speed fs);
void set_direction(enum fan_direction fd);
void send_command();
String get_date(enum time_format tf);
void write_syslog(const char *evt);
void clear_syslog();
void write_logfile();


/**********************************************************
 * WEB INTERFACES
 *********************************************************/
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html> 
<html> 
<meta name="viewport" content="width=device-width, initial-scale=1">
            
<head> 
  <title>Aeratron Fan Control</title> 
  <link rel="stylesheet" type="text/css" href="styles.css">
</head> 

<body>
<center>
<h2>Aeratron Fan Control</h2>
<p><small>Version %UI_VERSION%<br>%UI_DATE%</small></p><br>
%CONTROL_PANEL%
</center>
</body>
</html>
)rawliteral";

const char syslog_html[] PROGMEM = R"rawliteral(<!DOCTYPE html> 
<html> 
<meta name="viewport" content="width=device-width, initial-scale=1">
            
<head> 
  <title>Aeratron Syslog</title> 
  <link rel="stylesheet" type="text/css" href="styles.css">
</head> 

<body>
<center>
<h2>System Log</h2><br>
%SYSLOG_TABLE%
</center>
</body>
</html>
)rawliteral";

/* FOR FUTURE USE
const char changelog_html[] PROGMEM = R"rawliteral(<!DOCTYPE html> 
<html> 
<meta name="viewport" content="width=device-width, initial-scale=1">
            
<head> 
  <title>Aeratron Syslog</title> 
  <link rel="stylesheet" type="text/css" href="styles.css">
</head> 

<body>
<center>
<h2>Changelog</h2><br>
%SYSLOG_TABLE%
</center>
</body>
</html>
)rawliteral";
*/


/**********************************************************
 * PAGE BUILDER
 *********************************************************/
String page_builder(const String& var){

  if(var == "UI_DATE"){
    String ui_date = "";

    ui_date += get_date(TIMEFORMAT_DATE_SHORT);
    ui_date += " | ";
    ui_date += "<a href=\"/syslog\">Syslog</a>";

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
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/fan-on';\">Fan ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-0';\">Fan OFF</button><br>";
    } else {
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-on';\">Fan ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/fan-0';\">Fan OFF</button><br>";
    }
    
    panel += '\n';
    if (light_ctrl == LIGHT_STATE_ON) {
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light-on';\">Light ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/light-off';\">Light OFF</button><br>";
    } else {
        panel += "<button class=\"btn_state\" onclick=\"window.location.href='/light-on';\">Light ON</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light-off';\">Light OFF</button><br>"; 
    }
    
    panel += '\n';
    if ((fan_ctrl & 0xF0) == FAN_DIRECTION_LEFT) {
        panel += "<button class=\"btn_state\" title=\"Summer Season\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-left';\">Rotate Left</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" title=\"Winter Season\" onclick=\"window.location.href='/fan-right';\">Rotate Right</button><br>";
    } else {
        panel += "<button class=\"btn_state\" title=\"Summer Season\" onclick=\"window.location.href='/fan-left';\">Rotate Left</button>";
        panel += '\n';
        panel += "<button class=\"btn_state\" title=\"Winter Season\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-right';\">Rotate Right</button><br>";
    }
    
    panel += '\n';
    if ((fan_ctrl & 0x0F) == FAN_SPEED_1) {
        panel += "<button class=\"btn_speed\" title=\"55rpm (4.4W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-1';\">1</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"55rpm (4.4W)\" onclick=\"window.location.href='/fan-1';\">1</button>";     
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_2) {
        panel += "<button class=\"btn_speed\" title=\"85rpm (5.6W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-2';\">2</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"85rpm (5.6W)\" onclick=\"window.location.href='/fan-2';\">2</button>";          
    }     
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_3) {
        panel += "<button class=\"btn_speed\" title=\"110rpm (7.6W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-3';\">3</button>";
    } else {
        panel += "<button class=\"btn_speed\" title=\"110rpm (7.6W)\" onclick=\"window.location.href='/fan-3';\">3</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_4) {
       panel += "<button class=\"btn_speed\" title=\"130rpm (10.1W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-4';\">4</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"130rpm (10.1W)\" onclick=\"window.location.href='/fan-4';\">4</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_5) {
       panel += "<button class=\"btn_speed\" title=\"155rpm (13W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-5';\">5</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"155rpm (13W)\" onclick=\"window.location.href='/fan-5';\">5</button>";          
    } 
    
    panel += '\n';    
    if ((fan_ctrl & 0x0F) == FAN_SPEED_6) {
       panel += "<button class=\"btn_speed\" title=\"185rpm (17.3W)\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan-6';\">6</button>";
    } else {
       panel += "<button class=\"btn_speed\" title=\"185rpm (17.3W)\" onclick=\"window.location.href='/fan-6';\">6</button>";          
    }   
    
    return panel;
  }

  if(var == "SYSLOG_TABLE"){
    uint8_t i, idx, cnt;
    String sl_table = "";

    idx = syslog["index"];
    cnt = syslog["evtcnt"];

    sl_table += "<table>";
    sl_table += "<tr>\n";
    sl_table += "<th>ID</th>\n";
    sl_table += "<th>Date</th>\n";
    sl_table += "<th>Event</th>\n";
    sl_table += "</tr>\n"; 

    if (cnt < LOG_ENTRY_MAX) {          // normal enumeration of the table
      for (i=0; i<LOG_ENTRY_MAX; i++) {
        sl_table += "<tr>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][i]["id"].as<int>();
        sl_table += "</td>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][i]["date"].as<char*>();
        sl_table += "</td>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][i]["event"].as<char*>();
        sl_table += "</td>\n";
        sl_table += "</tr>\n";     
      } 
    } 
    else {                              // take ring buffer enumeration into account
      for (i=0; i<LOG_ENTRY_MAX; i++) { 
        sl_table += "<tr>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][idx]["id"].as<int>();
        sl_table += "</td>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][idx]["date"].as<char*>();
        sl_table += "</td>\n";
        sl_table += "<td>";
        sl_table += syslog["entry"][idx]["event"].as<char*>();
        sl_table += "</td>\n";
        sl_table += "</tr>\n";   
        
        idx++;
        if (idx >= LOG_ENTRY_MAX)  
          idx = 0;
      } 
    }

    sl_table += "<tr>\n";
    sl_table += "<td colspan=\"3\"><a href=\"/clear_syslog\">Delete all entries</a></td>";
    sl_table += "</tr>\n";  
    sl_table += "</table>";

    return sl_table;
  }

  /* FOR FUTURE USE
  if(var == "CHANGELOG_TABLE"){
    uint8_t i;
    String cl_table = "";

    cl_table += "<table>";
    cl_table += "<tr>";
    cl_table += "<th>Version</th>";
    cl_table += "<th>Date</th>";
    cl_table += "<th>Author</th>";
    cl_table += "<th>Change</th>";
    cl_table += "</tr>";

    for (i=0; i<3; i++) {
      cl_table += "<tr>";
      cl_table += "<td>scsdc</td>";
      cl_table += "<td>sdsv</td>";
      cl_table += "<td>sdvdvdgv</td>";
      cl_table += "</tr>";
    }

    cl_table += "</table>";

    return cl_table;
  }
  */

  return String();
}


/**********************************************************
 * SETUP
 *********************************************************/
void setup(){
  char header[50];
  uint8_t attempts = 0;
  uint8_t log_index;
  uint32_t log_count;
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

  Serial.print("Checking for styles.css.. ");
  if (SPIFFS.exists("/styles.css")) {
      Serial.println(" Found.");
  }
  else 
    Serial.println(" Error: no stylesheet found!");

  Serial.print("Checking for syslog.json.. ");
  if (SPIFFS.exists("/syslog.json")) {
      Serial.println(" Found.");
  }
  else 
    Serial.println(" Error: syslog.json not found!");

  /* FOR FUTURE USE
  Serial.print("Checking for changelog.json.. ");
  if (SPIFFS.exists("/changelog.json")) {
      Serial.println(" Found.");
  }
  else 
    Serial.println(" Error: changelog.json not found!");
  */

  Serial.print("Loading syslog.json.. ");
  syslogJsonFile = SPIFFS.open("/syslog.json", "r");
  DeserializationError error = deserializeJson(syslog, syslogJsonFile);
  if (error)
    Serial.println(F("Failed to read syslog.json. No syslog available!"));
  syslogJsonFile.close();  
  Serial.println(" Done.");

  log_index = syslog["index"];
  log_count = syslog["evtcnt"];

  Serial.print("SysLog_Index = ");
  Serial.println(log_index);
  Serial.print("SysLog_EvtCnt = ");
  Serial.println(log_count);
 
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
  Serial.println(get_date(TIMEFORMAT_DAYDATE));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "Main Page.");
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/styles.css", "text/css");
  });
 
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.png", "image/png");
  });

  server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/touchicon.png", "image/png");
  });

  server.on("/syslog", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", syslog_html, page_builder);
  });
  
  server.on("/changelog", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/changelog.html", "text/html");
  });  

  server.on("/clear_syslog", HTTP_GET, [](AsyncWebServerRequest *request){
    clear_syslog();
    request->send(200, "text/plain", "All syslog entries deleted.");
  });
 
  server.on("/fan-on", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_ON);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-0", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_OFF);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-1", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_1);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-2", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_2);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-3", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_3);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-4", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_4);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-5", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_5);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-6", HTTP_GET, [](AsyncWebServerRequest *request){
    set_speed(FAN_SPEED_6);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-left", HTTP_GET, [](AsyncWebServerRequest *request){
    set_direction(FAN_DIRECTION_LEFT);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/fan-right", HTTP_GET, [](AsyncWebServerRequest *request){
    set_direction(FAN_DIRECTION_RIGHT);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/light-on", HTTP_GET, [](AsyncWebServerRequest *request){
    set_light(LIGHT_STATE_ON);
    request->send_P(200, "text/html", index_html, page_builder);
  });

  server.on("/light-off", HTTP_GET, [](AsyncWebServerRequest *request){
    set_light(LIGHT_STATE_OFF);
    request->send_P(200, "text/html", index_html, page_builder);
  });
  
  server.begin();

  write_syslog("Startup completed.");
}


/**********************************************************
 * LOOP
 *********************************************************/
void loop() {
  wl_status_t wifi_state;
  static uint8_t err_cnt = 0;

  // checking for the WiFi connection regularly 
  wifi_state = WiFi.status();  

  switch (wifi_state)
  {
    case WL_CONNECTED: err_cnt = 0; 
    case WL_NO_SHIELD:
    case WL_IDLE_STATUS:
    case WL_NO_SSID_AVAIL:
    case WL_SCAN_COMPLETED: 
    case WL_CONNECT_FAILED: break;
    case WL_CONNECTION_LOST: err_cnt++; write_syslog("Lost network connection."); break;
    case WL_DISCONNECTED: err_cnt++; write_syslog("Disconnected from network."); break;
    default: write_syslog("Undefined network state detected."); break;
  }

  // No connection for more than 20 sec. Restart ESP.
  if (err_cnt > 4)
  {
    err_cnt = 0;
    write_syslog("Permanent fault detected. Going to restart ESP.");
    ESP.restart();
  }

  delay(6000);  
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

String get_date(enum time_format tf)
{
  char timeStringBuff[50];
  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)){
    strcpy(timeStringBuff, "No time available");
  }
  else
  {
    switch (tf)
    {
      case TIMEFORMAT_DAY: strftime(timeStringBuff, sizeof(timeStringBuff), "%A", &timeinfo); break;
      case TIMEFORMAT_DAYDATE: strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %d. %B %Y", &timeinfo); break;
      case TIMEFORMAT_DATE: strftime(timeStringBuff, sizeof(timeStringBuff), "%d. %B %Y", &timeinfo); break;
      case TIMEFORMAT_DATE_SHORT: strftime(timeStringBuff, sizeof(timeStringBuff), "%d.%m.%Y", &timeinfo); break;
      case TIMEFORMAT_TIME: strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo); break;
      case TIMEFORMAT_DATETIME: strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo); break;
      default: strcpy(timeStringBuff, "Invalid time format");
    }       
  }
  String time_string(timeStringBuff);
  return time_string;
}

void write_syslog(const char *evt)
{
  uint8_t idx;
  uint32_t cnt;

  idx = syslog["index"];
  cnt = syslog["evtcnt"];
  cnt++;
  
  if (cnt >= UINT32_MAX)
  {
    cnt = 1;

    syslog["entry"][idx]["id"] = cnt;
    syslog["entry"][idx]["date"] = get_date(TIMEFORMAT_DATETIME);
    syslog["entry"][idx]["event"] = "Eventcounter overflow. Reset evtcnt to 1.";

    cnt++;
    idx++;
    if(idx >= LOG_ENTRY_MAX) {
      idx = 0;
    }
  }

  syslog["entry"][idx]["id"] = cnt;
  syslog["entry"][idx]["date"] = get_date(TIMEFORMAT_DATETIME);
  syslog["entry"][idx]["event"] = evt;

  idx++;
  if(idx >= LOG_ENTRY_MAX) {
    idx = 0;
  }
  
  syslog["index"] = idx;
  syslog["evtcnt"] = cnt;  
 
  write_logfile(); 
}

void clear_syslog()
{
    uint8_t i;

    syslog["index"] = 0;
    syslog["evtcnt"] = 0; 

    for (i=0; i<LOG_ENTRY_MAX; i++)
    {
      syslog["entry"][i]["id"] = 0;
      syslog["entry"][i]["date"] = "1970-01-01 00:00:00";
      syslog["entry"][i]["event"] = "no event";
    }

    write_syslog("All syslog entries deleted.");
    Serial.println("All syslog entries deleted!");  
}

void write_logfile()
{
  syslogJsonFile = SPIFFS.open("/syslog.json", "w");
  if (serializeJson(syslog, syslogJsonFile) == 0) {
    Serial.println(F("Failed to write logfile!"));
  }
  syslogJsonFile.close();  
}
