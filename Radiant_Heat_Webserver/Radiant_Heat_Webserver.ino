/*
  Web Server that displays the radiant heat values
  http://207.136.204.147:46084/
  http://207.136.204.147:46084/?status=0  - historical
  http://207.136.204.147:46084/?status=1  - live
  
  
  tables with colored cells 
  http://www.utexas.edu/learn/html/colors.html
  http://www.quackit.com/html/examples/html_tables_examples.cfm
  

To Do: 
- On OneWire line, try using smaller resistor 1 to 2k, see if reading temps is more reliable
- look at these bug fixes http://www.pjrc.com/teensy/td_libs_OneWire.html#bugs
- put a 100ms delay between reads, if that doesn't help, try 1 sec
- Add I2C so you can send data to pachube via another arduino
- add icon that displays if heat is on or off.  Use img tag like you do with the graph
- Historical page doesn't work remotely, I think because it's losing the port info
  - Figure out how to use page names http://www.chipkin.com/arduino-ethernet-shield-a-better-webserver/
  - http://github.com/sirleech/Webduino
  - http://playground.arduino.cc/Code/WebServer
       http://arduino.cc/forum/index.php/topic,95113.0.html
-Replace radio button with two different HTML Pages that link to each other with a button or hyperlink


       
Good explanation of HTML Get with a checkbox
http://startingelectronics.com/tutorials/arduino/ethernet-shield-web-server-tutorial/web-server-LED-control/


PCB design specs
ICSP Pass through for Etherenet shield
Status LEDs
Could have LCD screen with what's happening


OneWire Config
There are 5 strands of one wire temp sensors between the floor joists; 60 sensor total.  Sketch doesn't work if you splice them all togehter onto 
one input pin, I don't know why. So I put each strand on it's own input pin.

There are 60 sensors (0-59), divided up into three main groups:
0-16: under kitchen and dining room.  17 sensors
17-40: Under mudroom, foyer, and main room, 24 sensors
41-59: Under hot tub, sauna, bathroom, 19 sensors

Strand A: 0-16
Strand B: 17-25
Strand C: 26-40
Strand D: 50-59
Strand E: 41-49


DS18B20 temp sensor Wiring:
Pin 1 - Gnd
Pin 2 - Data
Pin 3 - Vcc
Wire 4.7k reistor between data and Vcc

Sensors 41-59 are all from Newark and I do have some trouble reading them.  They don't always responnd to temperature requests



----- I/O -----
D0 - Rx
D1 - Tx
D2 - oneWire strand A
D3 - oneWire strand B
D4 - oneWire strand C
D5 - oneWire strand D & E
D6 - Optional - Zone 1 on/off
D7 - Optional - Zone 2 on/off

A0 - Thermistor - crawlspace
A1 - Thermistor - Outside
A2 - Thermistor - Living Room


Future I/0
Mega Pinout diagram http://arduino.cc/forum/index.php/topic,146511.0.html
D0-D1 - Tx/Rx
D2-D3 - status LEDs
D4 - SS for SD card
D5-D9 - 1Wire inputs
D10  - SS for Etherenet
D18-D19 - Tx2/Rx2
D20-D21 - I2C
D23-D27 - Thermocouple I/O
D50-D53 - SPI (Ethernet)

A1-A3 - Thermistor inputs


*/

#include <OneWire.h>             // Reference: http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h>   // Reference: http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
                                 // Reference: http://www.ay60dxg.com/doc/loguino/class_dallas_temperature.html
                                 // Wiki: http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
#include <SPI.h>                 // // Allows you to communicate with Ethernet shield http://arduino.cc/en/Reference/SPI
#include <Ethernet.h>            // Reference: http://arduino.cc/en/Reference/Ethernet

                                 
const uint8_t NUM_SENSORS = 60;   // Total number of onewire sensors

// Element location in temp[] and tempHist[] arrays
const uint8_t  OUTSIDE =    NUM_SENSORS + 1;  
const uint8_t  LIVINGRM =   NUM_SENSORS + 2;
const uint8_t  CRAWLSPACE = NUM_SENSORS + 3;


#define PRINT_DEBUG      // comment out to turn of printing to serial monitor

// Setup a oneWire instances to communicate OneWire devices in digital Pin #
OneWire oneWire_A(2); // strand is under kitchen and dining room, IDs 0-16
OneWire oneWire_B(3); // strand is under main room, IDs 17-25
OneWire oneWire_C(4); // strand is under main room, ID's 26-40
OneWire oneWire_D(5); // 2 strands (D & E) are in hot tub room and bathroom, IDs 41-59
//srg OneWire oneWire_E(?); // future when you split out strands D&E 


#define COSM_GRAPH "<img src=\"https://api.cosm.com/v2/feeds/4038/datastreams/5.png?width=800&height=300&colour=%23f15a24&duration=1week&title=Radiant%20Heat%20Temperature&stroke_size=2&show_axis_labels=true&detailed_grid=true&scale=auto&timezone=Eastern%20Time%20(US%20%26%20Canada)\">"

// Pass our oneWire references to Dallas Temperature. 
DallasTemperature sensors_A(&oneWire_A);
DallasTemperature sensors_B(&oneWire_B);
DallasTemperature sensors_C(&oneWire_C);
DallasTemperature sensors_D(&oneWire_D);

const uint8_t NUM_TEMPS =  NUM_SENSORS + 4;   // Total number of temperatures to rtrack in array

uint8_t temp[24][NUM_TEMPS];  // array to hold temp values for 24 hours.  First dim is hour, 2nd is temperature
                              // +3 is for Outside (+1), Living room(+2) and crawlspace(+3)
uint8_t hr = 0; // hour (military) for temp[][] array
bool displayLiveData = true;  // Flag set by radio button on web page, determines if live or historical data is displayed

// Define Thermistor Analog pins
#define INPUTPIN_CRAWLSPACE 0
#define INPUTPIN_OUTSIDE    1
#define INPUTPIN_LIVINGRM   2

// Thermocouple Pins
#define TC_AMP_DO     23   // DO (data out)
#define TC_AMP_CLK    25   // CLK (clock)
#define TC_AMP_CS_Z1  27   // CS for Zone 1 temp
#define TC_AMP_CS_Z2  29   // CS for Zone 2 temp
#define TC_AMP_CS_HW  31   // CS for Hot Water tank

const uint8_t SecondSectionOffset = 17; // Sensors for 2nd section in crawlspace start at 17
const uint8_t ThirdSectionOffset = 41;  // Sensors for 3rd section in crawlspace start at 41

// Suntec External IP 207.136.204.147:46084
byte mac[] = { 0x46, 0x46, 0x46, 0x00, 0x00, 0x0A };
IPAddress ip(192,168,46,84);  // Suntec
// IPAddress ip(192,168,216,80);  // Crestview
#define WANPORT "46084"  // Port forward used

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);

// See Google spreadsheet for sensor info and location
// https://docs.google.com/spreadsheet/ccc?key=0AipOz9H0GTbSdDRDdjFIX1RhQWM2SHcxQkQ3TE1CS2c#gid=0
// Also have PDF showing locations: Radiant Heat Layout.pdf 
static uint8_t tempSensors[NUM_SENSORS][8] = 
{ 
  { 0x28, 0x44, 0x8B, 0x2B, 0x04, 0x00, 0x00, 0x76 }, // 0 - First section under kitchen
  { 0x28, 0xAA, 0x38, 0x2C, 0x04, 0x00, 0x00, 0xCB }, 
  { 0x28, 0xA5, 0x4C, 0x2B, 0x04, 0x00, 0x00, 0x1B }, 
  { 0x28, 0x75, 0x0C, 0x2C, 0x04, 0x00, 0x00, 0xB0 }, 
  { 0x28, 0x58, 0x38, 0x2C, 0x04, 0x00, 0x00, 0xD7 }, 
  { 0x28, 0xDD, 0x3F, 0x2B, 0x04, 0x00, 0x00, 0x01 }, 
  { 0x28, 0x43, 0xC9, 0x2B, 0x04, 0x00, 0x00, 0x99 }, 
  { 0x28, 0x01, 0xEB, 0x2A, 0x04, 0x00, 0x00, 0x76 }, 
  { 0x28, 0x5B, 0x36, 0x2B, 0x04, 0x00, 0x00, 0xAA }, 
  { 0x28, 0x66, 0x3A, 0x2C, 0x04, 0x00, 0x00, 0xAA }, 
  { 0x28, 0x95, 0x1C, 0x2C, 0x04, 0x00, 0x00, 0xE5 }, 
  { 0x28, 0x36, 0x1E, 0x2C, 0x04, 0x00, 0x00, 0x63 }, 
  { 0x28, 0xD6, 0xDE, 0x2B, 0x04, 0x00, 0x00, 0xEE }, 
  { 0x28, 0xBC, 0x15, 0x2C, 0x04, 0x00, 0x00, 0x36 }, 
  { 0x28, 0x39, 0xFA, 0x2B, 0x04, 0x00, 0x00, 0x04 }, 
  { 0x28, 0x36, 0xC6, 0x2A, 0x04, 0x00, 0x00, 0x0A }, 
  { 0x28, 0x1F, 0x98, 0x2B, 0x04, 0x00, 0x00, 0x92 },  // 16 - Last section under sliding door by living room
  
  { 0x28, 0xD8, 0x44, 0x2B, 0x04, 0x00, 0x00, 0xF7 },  // 17- First section under mudroom
  { 0x28, 0x29, 0xDD, 0x2B, 0x04, 0x00, 0x00, 0xF6 }, 
  { 0x28, 0xEC, 0xE4, 0x2A, 0x04, 0x00, 0x00, 0x7A }, 
  { 0x28, 0x3F, 0xE5, 0xF9, 0x03, 0x00, 0x00, 0xD9 }, 
  { 0x28, 0xA6, 0xAA, 0x00, 0x04, 0x00, 0x00, 0xE0 }, 
  { 0x28, 0xCA, 0xD3, 0x00, 0x04, 0x00, 0x00, 0xC0 }, 
  { 0x28, 0xED, 0x9B, 0x00, 0x04, 0x00, 0x00, 0x24 }, 
  { 0x28, 0xB3, 0x9C, 0x00, 0x04, 0x00, 0x00, 0x48 }, 
  { 0x28, 0x4C, 0x98, 0x00, 0x04, 0x00, 0x00, 0x01 }, 
  { 0x28, 0x0A, 0xBD, 0x00, 0x04, 0x00, 0x00, 0xEC }, 
  { 0x28, 0x69, 0xB3, 0x00, 0x04, 0x00, 0x00, 0xD4 }, 
  { 0x28, 0x09, 0xCE, 0x00, 0x04, 0x00, 0x00, 0x96 }, 
  { 0x28, 0xAF, 0x96, 0x00, 0x04, 0x00, 0x00, 0xD3 }, 
  { 0x28, 0x64, 0xD7, 0x00, 0x04, 0x00, 0x00, 0x90 }, 
  { 0x28, 0x76, 0xE0, 0x00, 0x04, 0x00, 0x00, 0x70 }, 
  { 0x28, 0xC9, 0xC4, 0x00, 0x04, 0x00, 0x00, 0xB4 }, 
  { 0x28, 0xA7, 0xE1, 0xF9, 0x03, 0x00, 0x00, 0xD6 }, 
  { 0x28, 0x1B, 0xBC, 0x00, 0x04, 0x00, 0x00, 0x4D }, 
  { 0x28, 0x43, 0x01, 0xFA, 0x03, 0x00, 0x00, 0x71 }, 
  { 0x28, 0xBD, 0xA2, 0x00, 0x04, 0x00, 0x00, 0x7D }, 
  { 0x28, 0xDC, 0xB6, 0x00, 0x04, 0x00, 0x00, 0xEA }, 
  { 0x28, 0xE4, 0xE0, 0x00, 0x04, 0x00, 0x00, 0xAF }, 
  { 0x28, 0x8F, 0xD2, 0x00, 0x04, 0x00, 0x00, 0x93 }, 
  { 0x10, 0x7E, 0x77, 0x6A, 0x02, 0x08, 0x00, 0xFE }, // 40 - Last center section (by road)
  
  { 0x10, 0x5B, 0x89, 0x6A, 0x02, 0x08, 0x00, 0xA7 }, // 41 - First section under mudroom
  { 0x10, 0x3A, 0x8B, 0x6A, 0x02, 0x08, 0x00, 0xD0 }, 
  { 0x10, 0xA6, 0x75, 0x6A, 0x02, 0x08, 0x00, 0x18 }, 
  { 0x10, 0xF1, 0x7D, 0x6A, 0x02, 0x08, 0x00, 0x8D }, 
  { 0x10, 0xCA, 0x82, 0x6A, 0x02, 0x08, 0x00, 0x51 }, 
  { 0x10, 0xA8, 0x89, 0x6A, 0x02, 0x08, 0x00, 0x8C }, 
  { 0x10, 0xD2, 0x7B, 0x6A, 0x02, 0x08, 0x00, 0xFE }, 
  { 0x10, 0x46, 0x73, 0x6A, 0x02, 0x08, 0x00, 0xAD }, 
  { 0x10, 0xBC, 0x75, 0x6A, 0x02, 0x08, 0x00, 0x8C }, 
  { 0x10, 0x3F, 0x94, 0x6A, 0x02, 0x08, 0x00, 0x28 }, 
  { 0x10, 0x83, 0x81, 0x6A, 0x02, 0x08, 0x00, 0xFC }, 
  { 0x10, 0x1E, 0x97, 0x6A, 0x02, 0x08, 0x00, 0xE7 }, 
  { 0x10, 0x8D, 0x81, 0x6A, 0x02, 0x08, 0x00, 0xEF }, 
  { 0x10, 0x94, 0x7A, 0x6A, 0x02, 0x08, 0x00, 0xF4 }, 
  { 0x10, 0x54, 0x7C, 0x6A, 0x02, 0x08, 0x00, 0xF7 }, 
  { 0x10, 0x4E, 0x87, 0x6A, 0x02, 0x08, 0x00, 0xB5 }, 
  { 0x10, 0x4D, 0x83, 0x6A, 0x02, 0x08, 0x00, 0xF3 }, 
  { 0x10, 0x9F, 0x7B, 0x6A, 0x02, 0x08, 0x00, 0xC1 }, 
  { 0x10, 0x33, 0x93, 0x6A, 0x02, 0x08, 0x00, 0x04 }  // 59 - Last section, hot tub sliding door
};


//====================================================================================================
//====================================================================================================
void setup() 
{
 // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  #ifdef PRINT_DEBUG
    Serial.print(F("server is at "));
    Serial.println(Ethernet.localIP());
  #endif


  // Start up the OneWire library
  sensors_A.begin();
  sensors_B.begin();
  sensors_C.begin();
  sensors_D.begin();
  findSensors();  // Look for each sensor on the nework and print out it's status
  
  setupNTPTime();
  
  #ifdef PRINT_DEBUG
    // Print amount of free SRAM
    freeRam(true);
    Serial.println(F("Finished Setup()"));
  #endif


} // end setup()


//====================================================================================================
//====================================================================================================
void loop() 
{
 
  static uint32_t refreshTemps;
  int ntpTimeAry[6];
  char timebuf[30];  // char array to hold formatted time hour (military) & minutes
  bool localRequest; // true if user is on LAN, false if on WAN.  Client.print response is different for each
  
  // once a minute update the time and temperature.  This takes a couple seconds to execute
  if(((long)(millis() - refreshTemps) > 0L) || (millis()< 10000L) )
  {
    refreshTemps = millis() + 60000L; // reset update timer
  
    // Get the time  
     if (getTime(ntpTimeAry))
     // Put hours (military) and minutes and seconds in character array
     {  sprintf(timebuf, "Time: %d:%02d:%02d", ntpTimeAry[0], ntpTimeAry[2], ntpTimeAry[3]); }
     else
     // Didn't get time 
     {  ntpTimeAry[0] = -1; }
  
    // Set the hour for the temp[][] array
    if (ntpTimeAry[0] >= 0 && ntpTimeAry[0] <= 23)  // Only set hour if nptTime has a valid time
    { hr = ntpTimeAry[0]; }; 
  
    // Read all temperature values
   readTemperatures(hr);

  } // End update
  
  // listen for incoming clients on webserver
  EthernetClient client = server.available();
  if (client) 
  {
    Serial.println(F("new client"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    String buffer = "";
   
    while(client.connected()) 
    {
      while(client.available()) // repleced if() with while() per forum suggestion  http://bit.ly/16P5rVd
      {
        char c = client.read();
        Serial.write(c);
        buffer += c;
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) 
        {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connnection: close");
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
//          client.println("<meta http-equiv=\"refresh\" content=\"10\">");  // add a meta refresh tag, so the browser pulls again every 10 seconds:
          client.println("<HEAD>");
          client.println("<TITLE>Suntec Radiant Heat</TITLE>");
          client.println("<STYLE TYPE=\"text/css\"><!--TD{font-family: Arial; font-size: 9pt; text-align:center; color:white;}---></STYLE>");  // defines format for text in <td> tags
          
          client.println("</HEAD>");
          client.println(F("<H1>Radiant Heat Temperature Profiler of Awesomeness</H1>"));
          client.println("<BODY>");

          // Source for code to that uses radio buttons - http://code.google.com/p/arduino-projects-hq/downloads/list
      
          if(localRequest)
          { // User is on LAN
            client.print("<FORM action=\"http://");
            client.print(Ethernet.localIP());
            client.print("/\" >");
          }
          else
          { // user is on WAN
//          client.print("<FORM action=\"http://64.17.119.193:46084/\" >");
            client.print("<FORM action=\"http://207.136.204.147:");
            client.print(WANPORT);
            client.print("/\" >");
          }
          
          // This is used to keep the selected radio button set.  Without it the butotn would change it's default upon auto-refresh
          if (displayLiveData == true)
          { // Set the live radio button
            client.print("<P> <INPUT type=\"radio\" name=\"status\" value=\"1\"checked >Live ");
            client.print("<INPUT type=\"radio\" name=\"status\" value=\"0\">Historical ");
          }
          else
          {  // Set the historical radio button
            client.print("<P> <INPUT type=\"radio\" name=\"status\" value=\"1\">Live ");
            client.print("<INPUT type=\"radio\" name=\"status\" value=\"0\"checked >Historical ");
          }
          client.print("<INPUT type=\"submit\" value=\"Submit\"> </FORM>");
          
          if(displayLiveData)
          { HTML_Live(client); }
          else
          {  HTML_Historical(client); }
          
          client.println(COSM_GRAPH);  // display COSM graps
          client.println("<p>");
          client.println(timebuf);  // print current time
          client.println("</BODY></html>");
          break;  // need this, but I don't know why
        }
        
        // read status of radio button
        if (c == '\n') 
        {
          // start a new line
          currentLineIsBlank = true;
          buffer = "";
        } 
        else if (c == '\r') 
        { //  \r means the buffer has a complete line of text
          if(buffer.indexOf("GET /?status=1")>=0)
          { displayLiveData = true; } 
          
          if(buffer.indexOf("GET /?status=0")>=0)
          { displayLiveData = false; } 
          
          // See if user is coming from WAN or LAN
          if(buffer.indexOf("Host:")>=0)
          {            
            if(buffer.indexOf(WANPORT)>=0)
            { localRequest = false; } // User is coming from WAN using port forwarding
            else
            { localRequest = true; } // User is local on LAN
          }
        }
        else if (c != '\r') 
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
        
      } // end while client available 
    } // end while client connected
    
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println(F("client disonnected"));
  }  // if (client)
  
  
} // end loop()


//====================================================================================================
// Read temperatures
//====================================================================================================
void readTemperatures(byte ihour)
{
  
  int readDelay = 50;  // delay between reads
  
  #ifdef PRINT_DEBUG
    Serial.print(F("Get temps.  Hour = "));
    Serial.println(ihour);
  #endif
   
  // Loop through all 4 strands and get temperatures
  sensors_A.requestTemperatures();   // Send the command to get temperatures
  for(int i=0; i <= 16; i++)
  { 
    temp[ihour][i] = (int) sensors_A.getTempF(&tempSensors[i][0]); 
    delay(readDelay); 
  }
  
  sensors_B.requestTemperatures();   // Send the command to get temperatures
  for(int i=17; i <= 25; i++)
  { 
    temp[ihour][i] = (int) sensors_B.getTempF(&tempSensors[i][0]); 
    delay(readDelay); 
  }

  sensors_C.requestTemperatures();   // Send the command to get temperatures
  for(int i=26; i <= 40; i++)
  { 
    temp[ihour][i] = (int) sensors_C.getTempF(&tempSensors[i][0]); 
    delay(readDelay); 
  }

  // Sensors on strands D&E don't read as reliably as the others
  sensors_D.requestTemperatures();   // Send the command to get temperatures
  delay(200);
  for(int i=41; i <= 59; i++)
  { 
    // Only accept new sensor reading if difference between old and new is 3 degrees or less
    // Put this code in because there were sensors on this string oscillating between 60 and the actual temp
    // Sometimes the temp sensor will return 185 degrees, if this happens, ignore new value
    int newTemp = (int) sensors_D.getTempF(&tempSensors[i][0]);
    if ( (temp[ihour][i] - newTemp <= 3) && (newTemp != 185) )
    { temp[ihour][i] = newTemp; }
    delay(readDelay); 
  }


  // If any reading is < 0, change it to zero
  for(int i=0; i < NUM_SENSORS; i++)
  {
    if(temp[ihour][i] < 0)
    { temp[ihour][i] = 0; }
  }

  // Read thermistors 
  double filterVal = 0.01;  // low pass filter
  temp[ihour][CRAWLSPACE] = (Thermistor(analogRead(INPUTPIN_CRAWLSPACE)) * (1-filterVal)) + (filterVal * (float)temp[ihour][CRAWLSPACE]);
  temp[ihour][OUTSIDE] =    (Thermistor(analogRead(INPUTPIN_OUTSIDE)) * (1-filterVal)) + (filterVal * (float)temp[ihour][OUTSIDE]);
  temp[ihour][LIVINGRM] =   (Thermistor(analogRead(INPUTPIN_LIVINGRM)) * (1-filterVal)) + (filterVal * (float)temp[ihour][LIVINGRM]);
  
  // Read thermocouples (future)

} // end readTemperatures()

//=======================================================================================================================
// Get the average radiant heat temperature but exclulde the sections next to the foundation
//=======================================================================================================================
float avgFloorTemp(uint8_t zone)
{

  uint8_t validTempCnt = 0; // number of valid readings
  
  if (zone == 1)
  { // zone 1 - living room, kitchen
    uint32_t sumRadiant = 0;
    for (int i=1; i<=39; i++)
    {
      if(i!=16 && i!=17)
      { 
        sumRadiant += temp[hr][i]; 
        if(temp[hr][i] > 0)
        { validTempCnt++;}
      }
    }
    return (float)(sumRadiant / (float) validTempCnt );
  }
  else // zone 2 - hot tub area
  {
    uint32_t sumRadiant = 0;
    for (int i=42; i<=58; i++)
    {
      sumRadiant += temp[hr][i]; 
      if(temp[hr][i] > 0)
      { validTempCnt++;}
    }
    return (float)(sumRadiant / (float)validTempCnt );
  }
 
} // end avgFloorTemp()

    
//=======================================================================================================================
// look for all sensors on the network
// What's strange is if you don't have any OneWire devices connected, the isConnected() returns true for everything
//=======================================================================================================================
void findSensors()
{
  
  int retryDelay = 50;  // not sure if a delay helps
  char buff[45];
  int deviceCount = 0;

  deviceCount = sensors_A.getDeviceCount();
  if (deviceCount == 0)
  { deviceCount = sensors_A.getDeviceCount(); }  // Try again
  sprintf(buff, "Found %d of 17 sensors in strand A", deviceCount);
  Serial.println(buff);

  deviceCount = sensors_B.getDeviceCount();
  if (deviceCount == 0)
  { deviceCount = sensors_B.getDeviceCount(); }  // Try again
  sprintf(buff, "Found %d of 9 sensors in strand B", deviceCount);
  Serial.println(buff);

  deviceCount = sensors_C.getDeviceCount();
  if (deviceCount == 0)
  { deviceCount = sensors_C.getDeviceCount(); }  // Try again
  sprintf(buff, "Found %d of 16 sensors in strand C", deviceCount);
  Serial.println(buff);

  deviceCount = sensors_D.getDeviceCount();
  if (deviceCount == 0)
  { deviceCount = sensors_D.getDeviceCount(); }  // Try again
  sprintf(buff, "Found %d of 19 sensors in strands D & E", deviceCount);
  Serial.println(buff);
  
  // Loop through strand A to see if sensors are online
  for(int i=0; i <= 16; i++)
  {
    if ( sensors_A.isConnected(&tempSensors[i][0]) == true )  // See if device is connected
    { // Found temp sensor 
      Serial.print(F("Found ID# "));
      printAddress(i, &tempSensors[i][0]);
    }
    else
    { // Failed, try a 2nd time
      delay(retryDelay); // don't know if a delay would help or not
      if ( sensors_A.isConnected(&tempSensors[i][0]) == true )
      {
        Serial.print(F("Found ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
      else
      { // Failed two times
        Serial.print(F("Could not find ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
    }
  }
  
  // Loop through strand B to see if sensors are online
  for(int i=17; i <= 25; i++)
  {
    if ( sensors_B.isConnected(&tempSensors[i][0]) == true )  // See if device is connected
    { // Found temp sensor 
      Serial.print(F("Found ID# "));
      printAddress(i, &tempSensors[i][0]);
    }
    else
    { // Failed, try a 2nd time
      delay(retryDelay); // don't know if a delay would help or not
      if ( sensors_B.isConnected(&tempSensors[i][0]) == true )
      {
        Serial.print(F("Found ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
      else
      { // Failed two times
        Serial.print(F("Could not find ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
    }
  }
  
  // Loop through strand C to see if sensors are online
  for(int i=26; i <= 40; i++)
  {
    if ( sensors_C.isConnected(&tempSensors[i][0]) == true )  // See if device is connected
    { // Found temp sensor 
      Serial.print(F("Found ID# "));
      printAddress(i, &tempSensors[i][0]);
    }
    else
    { // Failed, try a 2nd time
      delay(retryDelay); // don't know if a delay would help or not
      if ( sensors_C.isConnected(&tempSensors[i][0]) == true )
      {
        Serial.print(F("Found ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
      else
      { // Failed two times
        Serial.print(F("Could not find ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
    }
  }

  // Loop through strands D & E to see if sensors are online
  for(int i=41; i <= 59; i++)
  {
    if ( sensors_D.isConnected(&tempSensors[i][0]) == true )  // See if device is connected
    { // Found temp sensor 
      Serial.print(F("Found ID# "));
      printAddress(i, &tempSensors[i][0]);
    }
    else
    { // Failed, try a 2nd time
      delay(retryDelay); // don't know if a delay would help or not
      if ( sensors_D.isConnected(&tempSensors[i][0]) == true )
      {
        Serial.print(F("Found ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
      else
      { // Failed two times
        Serial.print(F("Could not find ID# "));
        printAddress(i, &tempSensors[i][0]);
      }
    }
  }
  
} // end findSensors()


//====================================================================================================
// Print 1Wire device address
//====================================================================================================
void printAddress(int id, DeviceAddress deviceAddress)
{

  Serial.print(id);
  Serial.print(F(" at address "));
  
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print(F("0"));
    Serial.print(deviceAddress[i], HEX);
    if(i < 7)
    { Serial.print(F(":")); }
  }

  Serial.println();

} // end printAddress()

//====================================================================================================
//====================================================================================================
double Thermistor(int RawADC) 
{
  double Temp;
  Temp = log(((10240000/RawADC) - 10000));
  Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
  Temp = Temp - 273.15;            // Convert Kelvin to Celsius
  Temp = (Temp * 9.0)/ 5.0 + 32.0; // Convert Celsius to Fahrenheit
  return Temp;
} // end Thermistor()


//====================================================================================================
// Print table with live data
// Table uses <th> tags which is a header tag.  Default font is centered and bold
//====================================================================================================
void HTML_Live(EthernetClient client)
{
  Serial.println(F("Send live data to browser"));
  
  client.print("<table border=\"0\" cellpadding=\"0\" cellspacing=\"1\" width=\"600\" >");  
  // Generate table of live temperature values
  for(int j=0; j<24; j++)
  {
    client.print("<tr>");
    
    // First section has 17 sensors, don't print anything when j > 17
    // Also shift this section down 4 positions
    uint8_t shiftcol = 4; 
    if(j >= shiftcol && j < SecondSectionOffset + shiftcol ) 
    { 
      client.print("<th bgcolor=\"#");
      client.print(GetHtmlColor(temp[hr][j-shiftcol]), HEX);
      client.print("\">");
      client.print(temp[hr][j-shiftcol]);
      client.print("</th>");
    } 
    else
    { // If no sensor at this location, make color white
      client.print("<th bgcolor=\"#");
      client.print(0xFFFFFF, HEX);  // white
      client.print("\">");
      client.print("");
      client.print("</th>");
    }
    
    // Second section has 24 sensors, this is the most and you can print all values for j
    client.print("<th bgcolor=\"#");
    client.print(GetHtmlColor(temp[hr][j+SecondSectionOffset]), HEX);
    client.print("\">");
    client.print(temp[hr][j+SecondSectionOffset]);
    client.print("</th>");
    
    // The third secion has 19 sensors, don't print anything over j + ThirdSectionOffset > 59
    // shift section down 2 positions
    shiftcol = 2;
    if( j >= shiftcol && j + ThirdSectionOffset < NUM_SENSORS+shiftcol) 
    { 
      client.print("<th bgcolor=\"#");
      client.print(GetHtmlColor(temp[hr][j+ThirdSectionOffset-shiftcol]), HEX);
      client.print("\">");
      client.print(temp[hr][j+ThirdSectionOffset-shiftcol]);
      client.print("</th>");
    }
    else
    { // If j + ThirdSectionOffset > number of sensors, just print white
      client.print("<th bgcolor=\"#");
      client.print(0xFFFFFF, HEX);  // white
      client.print("\">");
      client.print("");
      client.print("</th>");
    }
    
    client.print("</tr>"); // end of the row
  }
  client.print("</table>");
  // Finished printing live table


  // Print zone averages - excludes end bays
  client.println("<p>Zone 1 average = ");
  client.println(avgFloorTemp(1));
  client.println("<br>Zone 2 average = ");
  client.println(avgFloorTemp(2));

  // Print non-radiant heat temps
  client.println("<p>Crawlspace = ");
  client.println(temp[hr][CRAWLSPACE]);
  client.println("<br>Living Room = ");
  client.println(temp[hr][LIVINGRM]);
  client.println("<br>Outside = ");
  client.println(temp[hr][OUTSIDE]);
  client.println("<p>");
  
} // end HTML_Live()  
  

//====================================================================================================
// Print historical table
// All sensors are desplayed in one row
// Oldest row is at top, newest on bottom
// HTML table uses <td> tag which is a normal cell.  Default format is left justified. But css
// formatting on <head> changed it to center
//====================================================================================================
void HTML_Historical(EthernetClient client)
{
  
  Serial.println(F("Send historical data to browser"));

  // create an index that will print the oldest data in the top row, and most recent in the bottom
  uint8_t dispIndex[24];
  uint8_t indexhr = hr + 1;
  for(int k = 0; k < 24; k++)
  {
    if (indexhr == 24)
    { indexhr = 0; }
    dispIndex[k] =  indexhr;
    indexhr++;
  }
  
  client.print("<table border=\"0\" cellpadding=\"0\" cellspacing=\"1\" width=\"800\" >");  

  // print header row
  client.print("<tr><td bgcolor=\"#74DF00\" colspan=\"17\"><FONT COLOR=\"black\">Zone 1 - Left</FONT></td>");
  client.print(    "<td bgcolor=\"#74DF00\" colspan=\"24\"><FONT COLOR=\"black\">Zone 1 - Right</FONT></td>");
  client.print("    <td bgcolor=\"#74DF00\" colspan=\"19\"><FONT COLOR=\"black\">Zone 2</FONT></td>");
  client.print("    <td bgcolor=\"#74DF00\"><FONT COLOR=\"black\">Out</FONT></td> ");
  client.print("    <td bgcolor=\"#74DF00\"><FONT COLOR=\"black\">L/R</FONT></td> ");
  client.print("    <td bgcolor=\"#74DF00\"><FONT COLOR=\"black\">C/S</FONT></td>   </tr>");

  for(int row = 0; row < 24; row++)
  {// print an entire row, 60 cells
    client.print("<tr>");  // start a new row
    for(int col = 0; col < NUM_TEMPS; col++)
    {
      client.print("<td bgcolor=\"#");
      client.print(GetHtmlColor(temp[dispIndex[row]][col]), HEX);
      client.print("\">");
      client.print(temp[dispIndex[row]][col]);
      client.print("</td>");
    }
    client.print("</tr>"); // end of the row
  } 

  client.print("</table><p>");  // end of table
  

} // end HTML_Historical()



//====================================================================================================
// Returns a hex color based on the temperature
// Red component = (2.686 * Temp) - 97.3
// Green component = 20
// Blue component = (-3.11 * temp) + 410.7
//====================================================================================================
uint32_t GetHtmlColor(int floorTemp)
{
  
  uint32_t htmlColor;
  
  // keep temp range batween 120 and 50 for colors
  if (floorTemp > 120)
  { floorTemp = 120; }
  if (floorTemp < 50 &&  floorTemp > 0)
  { floorTemp = 50;}
  
  // Temperature gradiant from Blue (50 degrees) to red (120 degrees)
  uint32_t red = (2.686 * (double) floorTemp) - 97.3;
  uint32_t grn = 20;
  uint32_t blu = (-3.11 * (double) floorTemp) + 410.7;
  
  // Shift component colors into htmlColor variable
  red = red << 16;
  grn = grn << 8;
  htmlColor = red | grn | blu;
  
  if(floorTemp == 0)
  {htmlColor = 0xFFFFFF;}
  
  return htmlColor;  
}  // end GetHtmlColor()



//====================================================================================================
// Displays the amount of freem RAM
//====================================================================================================
int freeRam(bool PrintRam)
{
  int freeSRAM;
  extern int __heap_start, *__brkval;
  int v;
 
  freeSRAM =  (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
  if(PrintRam)
  {
  Serial.print(F("RAM "));
  Serial.println(freeSRAM);
  }
  return freeSRAM;
} // end freeRam()

