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

/************************************************
 * INCLUDES
 ***********************************************/
#include <WiFi.h>


/************************************************
 * DEFINES
 ***********************************************/
#define VERSION_MAJOR   1
#define VERSION_MINOR   0
#define VERSION_PATCH   0

#define FAN_ADDRESS     0xF0    // depending on the dip switches of the original remote control


/************************************************
 * GLOBALS
 ***********************************************/
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// the server
WiFiServer server(80);

// buffer to store the fan and light state
char fan_ctrl;
char light_ctrl;


/************************************************
 * ENUMS
 ***********************************************/
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


/************************************************
 * FUNCTION PROTOTYPES
 ***********************************************/
void set_light(enum light_state);
void set_speed(enum fan_speed);
void set_direction(enum fan_direction);
void send_command();


/************************************************
 * SETUP
 ***********************************************/
void setup()
{
    Serial.begin(115200);
    delay(10);

    // Init pin
    pinMode(27, OUTPUT);

    fan_ctrl = 0x00;
    light_ctrl = 0x00;

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.begin();

}

/************************************************
 * LOOP
 ***********************************************/
void loop(){
 WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /fan/0") >= 0) {
              Serial.println("Turn Fan OFF");
              set_speed(FAN_SPEED_OFF);

            } else if (header.indexOf("GET /fan/on") >= 0) {
              Serial.println("Set previous Fan Speed");
              set_speed(FAN_SPEED_ON);

            } else if (header.indexOf("GET /fan/1") >= 0) {
              Serial.println("Set Fan Speed 1");
              set_speed(FAN_SPEED_1);

            } else if (header.indexOf("GET /fan/2") >= 0) {
              Serial.println("Set Fan Speed 2");
              set_speed(FAN_SPEED_2);

            } else if (header.indexOf("GET /fan/3") >= 0) {
              Serial.println("Set Fan Speed 3");
              set_speed(FAN_SPEED_3);

            } else if (header.indexOf("GET /fan/4") >= 0) {
              Serial.println("Set Fan Speed 4");
              set_speed(FAN_SPEED_4);

            } else if (header.indexOf("GET /fan/5") >= 0) {
              Serial.println("Set Fan Speed 5");
              set_speed(FAN_SPEED_5);              

            } else if (header.indexOf("GET /fan/6") >= 0) {
              Serial.println("Set Fan Speed 6");
              set_speed(FAN_SPEED_6);
            
            } else if (header.indexOf("GET /fan/left") >= 0) {
              Serial.println("Set Fan Direction LEFT");
              set_direction(FAN_DIRECTION_LEFT);

            } else if (header.indexOf("GET /fan/right") >= 0) {
              Serial.println("Set Fan Direction RIGHT");
              set_direction(FAN_DIRECTION_RIGHT);
              
            } else if (header.indexOf("GET /light/on") >= 0) {
              Serial.println("Set Fan Light ON");
              set_light(LIGHT_STATE_ON);

            } else if (header.indexOf("GET /light/off") >= 0) {
              Serial.println("Set Fan Light OFF");
              set_light(LIGHT_STATE_OFF);
              
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html>");
            client.println("<html>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");            

            client.println("<style>");
            client.println(".btn_state {");
            client.println("  background-color: #ddd;");
            client.println("  border: none;");
            client.println("  color: black;");
            client.println("  padding: 10px 20px;");
            client.println("  text-align: center;");
            client.println("  text-decoration: none;");
            client.println("  display: inline-block;");
            client.println("  margin: 4px 2px;");
            client.println("  cursor: pointer;");
            client.println("  border-radius: 16px;");
            client.println("  width: 166px;");
            client.println("}");
            client.println(".btn_state:hover {");
            client.println("  background-color: #f1f1f1;");
            client.println("}");
            
            client.println(".btn_speed {");
            client.println("  background-color: #ddd;");
            client.println("  border: none;");
            client.println("  color: black;");
            client.println("  padding: 10px 20px;");
            client.println("  text-align: center;");
            client.println("  text-decoration: none;");
            client.println("  display: inline-block;");
            client.println("  margin: 4px 2px;");
            client.println("  cursor: pointer;");
            client.println("  border-radius: 16px;");
            client.println("  width: 50px;");
            client.println("}");
            client.println(".btn_speed:hover {");
            client.println("  background-color: #f1f1f1;");
            client.println("}");
            client.println("</style>");
            
            client.println("<head>");
            client.println("<title>Aeratron Fan Control</title>");
            client.println("</head>");
                        
            client.println("<body>");
            client.println("<center>");
            client.println("<h2>Aeratron Fan Control</h2>");
            client.println("<p><small>MGKOENIG 2020</small></p><br>");

            if ((fan_ctrl & 0x0F) == FAN_SPEED_OFF) {
                client.println("<button class=\"btn_state\" onclick=\"window.location.href='/fan/on';\">Fan ON</button>");
                client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/0';\">Fan OFF</button><br>");
            } else {
                client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/on';\">Fan ON</button>");
                client.println("<button class=\"btn_state\" onclick=\"window.location.href='/fan/0';\">Fan OFF</button><br>");

            } if (light_ctrl == LIGHT_STATE_ON) {
                  client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light/on';\">Light ON</button>");
                  client.println("<button class=\"btn_state\" onclick=\"window.location.href='/light/off';\">Light OFF</button><br>");
            } else {
                  client.println("<button class=\"btn_state\" onclick=\"window.location.href='/light/on';\">Light ON</button>");
                  client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/light/off';\">Light OFF</button><br>");
  
            } if ((fan_ctrl & 0xF0) == FAN_DIRECTION_LEFT) {
                  client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/left';\">Left (Summer)</button>");
                  client.println("<button class=\"btn_state\" onclick=\"window.location.href='/fan/right';\">Right (Winter)</button><br>");
            } else {
                  client.println("<button class=\"btn_state\" onclick=\"window.location.href='/fan/left';\">Left (Summer)</button>");
                  client.println("<button class=\"btn_state\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/right';\">Right (Winter)</button><br>");
  
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_1) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/1';\">1</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/1';\">1</button>");  
            
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_2) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/2';\">2</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/2';\">2</button>");  
                
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_3) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/3';\">3</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/3';\">3</button>");  
                
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_4) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/4';\">4</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/4';\">4</button>");  
                
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_5) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/5';\">5</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/5';\">5</button>");  
                
            } if ((fan_ctrl & 0x0F) == FAN_SPEED_6) {
                client.println("<button class=\"btn_speed\" style=\"background-color:#4CAF50\" onclick=\"window.location.href='/fan/6';\">6</button>");
            } else {
                client.println("<button class=\"btn_speed\" onclick=\"window.location.href='/fan/6';\">6</button>");  
                
            }
            
            client.println("</center>");
            client.println("</body>");
            client.println("</html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


/************************************************
 * FUNCTIONS
 ***********************************************/
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
  int i, j, k;
  char fan_command[3];

  fan_command[0] = FAN_ADDRESS;
  fan_command[1] = fan_ctrl;
  fan_command[2] = light_ctrl;

  // repeat each message 5 times
  for (k=0; k<10; k++) {
      // Start bit
      digitalWrite(27, LOW);      // sets the pin on
      delayMicroseconds(500);     
      digitalWrite(27, HIGH);     // sets the pin off
      delayMicroseconds(1000);    
      
      for (j=0; j<3; j++) {
        for (i=7; i>=0; i--) {
          if (fan_command[j] & (1 << i)) {
            digitalWrite(27, LOW);      
            delayMicroseconds(500);     
            digitalWrite(27, HIGH);     
            delayMicroseconds(1000);    
          }
          else {
            digitalWrite(27, LOW);      
            delayMicroseconds(1000);     
            digitalWrite(27, HIGH);     
            delayMicroseconds(500);   
          }
        }
      }
  
      digitalWrite(27, LOW); 
      delay(6);  
    }
}
