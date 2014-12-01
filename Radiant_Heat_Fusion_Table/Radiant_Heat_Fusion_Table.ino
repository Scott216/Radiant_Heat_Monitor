/*
Sketch sends data to Google Fusion Table:
https://www.google.com/fusiontables/DataSource?docid=131lEwLkyLpsiIo956Su6OCdcepuylcs2RKxEmGo
The date gets to the fusion table in a rather round about manner.  First it's sent
to pushingbox.com which acts as an HTTPS proxy.  If the arduino supported HTTPS, this step could be skipped.
PushingBox sends the data for a Google Form, which is linked to a Google spreadsheet
The Google spreadsheet has a script running that then puts the data into a Fusion Table 
whenever the formSumbit action is executed.
Here are the instructions on how to set all this up:
http://fusion-tables-api-samples.googlecode.com/svn/trunk/FusionTablesFormSync/docs/reference.html
In order to send data to the form, you need all the field IDs. To get field IDs, go to edit form, select Responses 
Menu > Get pre-fill URL > Fill in some dummy data, click submit and you'll see URL with fields IDs.
In URL change viewForm to viewResponse. 
When this URL is used with a GET or POST request, the data will go into the form, then into the spreadsheet then into the 
fusion table.  I modified the spreadsheet script to delete the previous record when a new one is added, the stops the 
spreadsheet from filling up with data.

PushingBox has a URL length limit, so it has to be done in two steps to get all the data to Google since 
the database has so many fields.
In PushingBox I created a custom URL Service called Radiant Heat.  It is just this base URL from the pre-fill URL step 
with "viewForm" replaced by "viewResponse"
https://docs.google.com/forms/d/1P_M_LNWf7gVjWUczG2QqxVQWYNwbA8Z2DWyeumqvBL0/formResponse
In PushingBox select the GET method when creating this Service.

Next in PushingBox I created a Scenarios.  The scenario takes the key-value pairs the Arduino sends out
and appends them to the Google Form URL.  The Scenario has a Device ID associated with it.
The data for the action is:
?$F0$=$D0$&$F1$=$D1$&$F2$=$D2$&$F3$=$D3$&$F4$=$D4$&$F5$=$D5$&$F6$=$D6$&$F7$=$D7$&$F8$=$D8$&$F9$=$D9$&$F10$=$D10$&$F11$=$D11$&$F12$=$D12$&$F13$=$D13$&$F14$=$D14$&$F15$=$D15$&$F16$=$D16$&$F17$=$D17$&$F18$=$D18$&$F19$=$D19$&$F20$=$D20$&$F21$=$D21$&$F22$=$D22$&$F23$=$D23$&$F24$=$D24$&$F25$=$D25$&$F26$=$D26$&$F27$=$D27$&$F28$=$D28$&$F29$=$D29$&$F30$=$D30$&$F31$=$D31$&$F32$=$D32$&$F33$=$D33$&$F34$=$D34$&$F35$=$D35$&$F36$=$D36$&$F37$=$D37$&$F38$=$D38$&$F39$=$D39$&$F40$=$D40$&$F41$=$D41$&$F42$=$D42$&$F43$=$D43$&$F44$=$D44$&$F45$=$D45$&$F46$=$D46$&$F47$=$D47$&$F48$=$D48$&$F49$=$D49$&$F50$=$D50$&$F51$=$D51$&$F52$=$D52$&$F53$=$D53$&$F54$=$D54$&$F55$=$D55$&$F56$=$D56$&$F57$=$D57$&$F58$=$D58$&$F59$=$D59$&$F60$=$D60$&$F61$=$D61$&$F62$=$D62$&$F63$=$D63$&$F64$=$D64$&$F65$=$D65$&$F66$=$D66$&$F67$=$D67$&$F68$=$D68$&$F69$=$D69$

The Google form fields all have an ID like: entry.1485604880
The numerical part of this is stored in the array fieldIDs[].  The sketch builds out the URL pulling field ID and data from arrays.

PushingBox URL IDs.  Key value paris are setup like F0 for field ID (ie entity.1485604880) and D0 for the temperature. Then F1 for the next field an  so on  
0-16 are L1-17 - left side of zone 1
17-40 are C1-C24 - center section of zone 1
41-59 - Zone 2
60 Zone 1 avarage
61 Zone 2 average
62 Crawlspace
63 Living Room
64 Outside
65 Hot Water
66 Zone 1 Source - water temp fed into zone 1
67 Zone 1 Return - water temp returning from zone 1
68 Zone 2 Source
69 Zone 2 Return

To send out the data, the URL would look like this (this example would only do the first two fields):
api.pushingbox.com/pushingbox?devid=[PUT DEVICE ID HERE]&F0=entry.1485604880&D0=108&F1=1454471935&D1=105

Arduino post with pushingbox http://forum.arduino.cc/index.php?topic=108796.0

See Google spreadsheet for sensor info and location 
https://docs.google.com/spreadsheet/ccc?key=0AipOz9H0GTbSdDRDdjFIX1RhQWM2SHcxQkQ3TE1CS2c#gid=0

Issues: 
One day I noticed the Zone 2 average temp and all the senors in the right section were not changing. I've always had some problems with this group 
of sensors.  I added code to soft reset the Arduino of the averages don't change at all in 2 hours 

Change Log
11/27/14 v2.1  New PCB  - changed I/O pins for OneWire strands
11/30/14 v2.2 Moved thermistor input from A0 to A3 because LED is using A0.  Formatting changes 
 
*/

#define VERSION "v2.1"


#include <OneWire.h>             // Reference: http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h>   // Reference: http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
                                 // Reference: http://www.ay60dxg.com/doc/loguino/class_dallas_temperature.html
                                 // Wiki: http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
#include <SPI.h>                 // Allows you to communicate with Ethernet shield http://arduino.cc/en/Reference/SPI
#include <Ethernet.h>            // Reference: http://arduino.cc/en/Reference/Ethernet
#include "Tokens.h"              // pushingbox Device ID

uint32_t uploadTime = 0UL;
uint32_t lastConnectionTime;

#define STATBOOTUP 10 // Bootup status code
#define STATOK      0 // everything okay status code

// I/O Pins
const byte STRAND_A_PIN =  4;
const byte STRAND_B_PIN =  5;
const byte STRAND_C_PIN =  6; 
const byte STRAND_DE_PIN = 7;
const byte GAS_PULSE_PIN = 8; // Gas meter pin
const byte UPLOAD_LED_PIN = A0;

// Define Thermistor Analog pins
#define INPUTPIN_OUTSIDE    1
#define INPUTPIN_LIVINGRM   2
#define INPUTPIN_CRAWLSPACE 3


char serverName[] = "api.pushingbox.com";
char url[] = "/pushingbox";
EthernetClient client; // create a client that connects to Google
boolean lastConnected = false; // State of the connection last time through the main loop
byte statusCode = 10;  // status code uploaded to database.  
                      // 0 Ok; 1 - 8 Zone 2 temp not changing; 10 bootup

#define NUM_FIELDS 69  // number of fields in Google form and Fusion Table
                    
// Field IDs  from Google Form Pre-fill URL
uint32_t fieldIDs[] = 
{ 
1485604880UL, 1454471935UL, 684946295UL, 995372575UL, 743946830UL, 356028983UL, 1065271046UL, 
697862853UL, 185582037UL, 1504991118UL, 884771400UL, 161327017UL, 1634650784UL, 1545347713UL, 
550239388UL, 13050017UL, 331240857UL, 1586530672UL, 1775703323UL, 1618431601UL, 669671588UL, 
2056068523UL, 120724998UL, 830818980UL, 824852898UL, 507821130UL, 1603174704UL, 1852226715UL, 
1545951517UL, 1543863830UL, 1810751075UL, 1547812614UL, 1758581139UL, 1049231275UL, 1909818331UL, 
1367504293UL, 1652260363UL, 457372453UL, 1922540169UL, 927290636UL, 1036367803UL, 800525522UL, 
1447280443UL, 1391354426UL, 1963032762UL, 1021755589UL, 139281338UL, 1537004943UL, 1400464801UL, 
1869412965UL, 331542690UL, 1464811047UL, 554669301UL, 1583699996UL, 1460619256UL, 1070740819UL, 
1582898456UL, 1616305818UL, 1461194045UL, 1282018561UL, 42618908UL, 1206219700UL, 953412505UL, 
132589797UL, 767022784UL, 949667705UL, 1210212730UL, 1484759656UL, 647369364UL, 1796545086UL 
};

byte temperature[NUM_FIELDS];  // array to hold temperatures, it's a byte, so no decimals

const uint8_t NUM_SENSORS = 60;   // Total number of onewire sensors in for radiant heat

// Element location in temperature[] arrays
const uint8_t  AVG_Z1 =     60;
const uint8_t  AVG_Z2 =     61;
const uint8_t  CRAWLSPACE = 62;
const uint8_t  LIVINGRM =   63;
const uint8_t  OUTSIDE =    64;  
const uint8_t  HOTWATER =   65;
const uint8_t  Z1SOURCE =   66;
const uint8_t  Z1Return =   67;
const uint8_t  Z2SOURCE =   68;
const uint8_t  Z2RETURN =   69;

const uint8_t LAST_TEMP_FIELD = 65; // temperaturs to upload to fusion table.


// Setup a oneWire instances to communicate OneWire devices in digital Pin #
OneWire oneWire_A(STRAND_A_PIN);  // strand is under kitchen and dining room, IDs 0-16
OneWire oneWire_B(STRAND_B_PIN);  // strand is under main room, IDs 17-25
OneWire oneWire_C(STRAND_C_PIN);  // strand is under main room, ID's 26-40
OneWire oneWire_D(STRAND_DE_PIN); // 2 strands (D & E) are in hot tub room and bathroom, IDs 41-59

// Pass our oneWire references to Dallas Temperature. 
DallasTemperature sensors_A(&oneWire_A);
DallasTemperature sensors_B(&oneWire_B);
DallasTemperature sensors_C(&oneWire_C);
DallasTemperature sensors_D(&oneWire_D);


// Suntec External IP 207.136.204.147:46084
byte mac[] = { 0x46, 0x46, 0x46, 0x00, 0x00, 0x0A };
IPAddress ip(192,168,46,84);  // Suntec

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


void setup()
{  
  Serial.begin(9600);
  delay(1000);
  Serial.print(F("Begin Radiant Heat Setup "));
  Serial.println(VERSION); 
  
  pinMode(UPLOAD_LED_PIN, OUTPUT);
  
  
  Ethernet.begin(mac, ip);
  delay(1000); // give the Ethernet shield a second to initialize
  Serial.print(F("My IP address: "));
  Serial.println(Ethernet.localIP());
  
  // Start up the OneWire library
  sensors_A.begin();
  sensors_B.begin();
  sensors_C.begin();
  sensors_D.begin();
  findSensors();  // Look for each sensor on the nework and print out it's status
  
  freeRam(true);
  
} // setup()

  
void loop()
{ 
  // Upload first set of data 
  if ( (long)(millis() - uploadTime) > (60000UL * 15UL) || uploadTime == 0UL)  // upload every 15 minuts
  {
    readTemperatures();
    temperature[HOTWATER] = statusCode; // SRG temporary put status code in the Hot Water field
     
     
    if( client.connect(serverName, 80) )
    {    
      Serial.println(F("Client Connected :)"));
      lastConnectionTime = millis();
      postRequest(serverName, url); 
      uploadTime = millis();  // record upload time
    }
    else
    { 
      Serial.println(F("Client connection failed :("));
      client.stop();    
    }
  }
  
  // print the response from server
  // Should get HTTP/1.1 200 OK
  if (client.available()) 
  {
    char c = client.read();
    Serial.print(c);
  }

  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if ( !client.connected() && lastConnected ) 
  {
    Serial.println(F("\ndisconnecting...\n"));
    client.stop();
  }
  lastConnected = client.connected();

  // give the server 5 seconds to reply, then close the connection
  if ( (long) (millis() - lastConnectionTime) > 5000 && client.connected() )
  {
    Serial.println(F("Close connection after 5 sec"));
    client.stop();
  }
  
  if ( statusCode == STATBOOTUP )  // reset status code after bootup code
  { statusCode = STATOK; }

} // loop()


//====================================================================================================
// PostRequest
// Builds URL and sends to PushingBox
//====================================================================================================
void postRequest(char *hostName, char *url)
{
  char urldata[30];
  
  Serial.println(F("Data sent to server: "));
  
  String buf = "POST " + String(url) + " HTTP/1.1";
  client.println(buf);
  client.println("Host: " + String(hostName));
  client.println("Content-Type: application/x-www-form-urlencoded");
  int datalen = strlen(DEVID_RADIANTHEAT1);
  // caclulate data length
  for (int f = 0; f <= LAST_TEMP_FIELD; f++)
  {
    sprintf(urldata, "&F%d=entry.%lu&D%d=%d", f, fieldIDs[f], f, temperature[f] );
    datalen = datalen + strlen(urldata);
  }
  client.println("Content-Length: " + String(datalen));
  client.println("");
  client.print(DEVID_RADIANTHEAT1);
  for(int f = 0; f <= LAST_TEMP_FIELD; f++)
  {
    sprintf(urldata, "&F%d=entry.%lu&D%d=%d", f, fieldIDs[f], f, temperature[f] );
    client.print(urldata);
  }
  client.println("");
  client.println("");
  client.println(""); // POST request as GET ends with 2 line breaks and carriage returns (\n\r);
  
}  // postRequest()


//====================================================================================================
// Read temperatures
//====================================================================================================
void readTemperatures()
{
  
  int readDelay = 50;  // delay between reads
  static float Z2Avg;     // float that records the value of zone 2 average.  Used to see if temperature is changing
  static float Z2AvgOld;  // Zone 2 average last time
  static byte noChangeCounter;  // counter used to see if zone 2 temps stuck
  
  // Loop through all 4 strands and get temperatures
  sensors_A.requestTemperatures();   // Send the command to get temperatures
  for(int i=0; i <= 16; i++)
  { 
    temperature[i] = (byte) sensors_A.getTempF(&tempSensors[i][0]); 
    delay(readDelay); 
  }
  
  sensors_B.requestTemperatures();   // Send the command to get temperatures
  for(int i=17; i <= 25; i++)
  { 
    temperature[i] = (byte) sensors_B.getTempF(&tempSensors[i][0]); 
    delay(readDelay); 
  }

  sensors_C.requestTemperatures();   // Send the command to get temperatures
  for(int i=26; i <= 40; i++)
  { 
    temperature[i] = (byte) sensors_C.getTempF(&tempSensors[i][0]); 
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
    if ( (temperature[i] - newTemp <= 3) && (newTemp != 185) )
    { temperature[i] = newTemp; }
    delay(readDelay); 
  }

  // If any reading is < 0, change it to zero
  for(int i=0; i < NUM_SENSORS; i++)
  {
    if(temperature[i] < 0)
    { temperature[i] = 0; }
  }

  // Read thermistors 
  double filterVal = 0.01;  // low pass filter
  temperature[CRAWLSPACE] = (Thermistor(analogRead(INPUTPIN_CRAWLSPACE)) * (1-filterVal)) + (filterVal * (float)temperature[CRAWLSPACE]);
  temperature[OUTSIDE] =    (Thermistor(analogRead(INPUTPIN_OUTSIDE)) *    (1-filterVal)) + (filterVal * (float)temperature[OUTSIDE]);
  temperature[LIVINGRM] =   (Thermistor(analogRead(INPUTPIN_LIVINGRM)) *   (1-filterVal)) + (filterVal * (float)temperature[LIVINGRM]);
  
  // Get average temperature for zone 1 & 2
  temperature[AVG_Z1] = (byte) avgFloorTemp(1);
  Z2Avg = avgFloorTemp(2);
  temperature[AVG_Z2] = (byte) Z2Avg;
  
  // See if Zone 2 temperaturs are changing over time, if not, reset arduino
  if ( Z2Avg == Z2AvgOld )
  { 
    noChangeCounter++; 
    statusCode = noChangeCounter;
    Serial.println(F("No change in zone 2 temp"));
  }
  else
  { 
    noChangeCounter = 0; 
    if (statusCode <= 8 )  // reset status code of it's not there are only zone2 avg problems
    { statusCode = STATOK; }
  }  // reset counter
  
  
  // If zone 2 aveage hasn't changed in 2 hours (8 fifteen min checks) then reset arduino
  if ( noChangeCounter >= 8 )
  { 
    Serial.println(F("Zone 2 not responding, rebooting"));
    softReset(); 
  }
  
  Z2AvgOld = Z2Avg;

} // end readTemperatures()

//=======================================================================================================================
// Get the average radiant heat temperature but exclulde the sections next to the foundation because they are always cold
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
        sumRadiant += temperature[i]; 
        if(temperature[i] > 0)
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
      sumRadiant += temperature[i]; 
      if(temperature[i] > 0)
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
// Displays the amount of free RAM
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


//====================================================================================================
//  Reset arduino
//====================================================================================================
void softReset()
{
  asm volatile ("  jmp 0");  
}  // end softReset()

