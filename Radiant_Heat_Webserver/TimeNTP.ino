/*

 Udp NTP Client
 
 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket 
 For more on NTP time servers and the messages needed to communicate with them, 
 see http://en.wikipedia.org/wiki/Network_Time_Protocol
 
 Created by Scott Goldthwaite Aug 26 2012

 This sketch is based off code from Michael Margolis
 See: http://arduino.cc/en/Tutorial/UdpNtpClient
 
 */


unsigned int localPort = 8888;           // local port to listen for UDP packets
IPAddress timeServer(64, 90, 182, 55 );  // time.nist.gov NTP server  http://tf.nist.gov/tf-cgi/servers.cgi
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov NTP server
const int NTP_PACKET_SIZE= 48;           // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];      //buffer to hold incoming and outgoing packets 

// A UDP instance to let us send and receive packets over UDP  
EthernetUDP Udp;


//============================================================
//============================================================
void setupNTPTime() 
{
  Udp.begin(localPort);
}  // setupNTPTime()

//============================================================
// Get the time from NTP server and return in ntpTime[] array
// Function returns true if it got the time, false otherwise
// Return data in 5-element array:
// 0 - hour (12 hr format)
// 1 - hour (24 hr format)
// 2 - minute
// 3 - second
// 4 - 1 for AM, 2 for PM
// 5 - day of the week, 0=Sunday
//============================================================
bool getTime(int *ntpTime)
{
  const byte h24 = 0;
  const byte h12 = 1;
  const byte m = 2;
  const byte s = 3;
  const byte AMPM = 4;
  const byte dow = 5;

  sendNTPpacket(timeServer); // send an NTP packet to a time server
  delay(500);  // wait for response, if you remove this, you get the same time every time.
  if ( Udp.parsePacket() ) 
  {  
    
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  

    // now convert NTP time into everyday time:
    const unsigned long seventyYears = 2208988800UL;     
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;  

    // Adjust for time zone
    const int estOffset = 5;
    epoch = epoch - (estOffset * 3600UL);

    // get the hour, minute and second:
    ntpTime[h24] = (epoch  % 86400L) / 3600;  // hour (86400 equals secs per day)
    ntpTime[m] =   (epoch  %   3600) / 60;    // minute (3600 equals secs per minute)
    ntpTime[s] =   (epoch  %     60);         // second
   
    // Convert hr from 24 hr format to 12 hour
    // and set AM/PM
    if (ntpTime[h24] > 12)
    { 
      ntpTime[h12] = ntpTime[h24] - 12;
      ntpTime[AMPM] = 2;   // Time is PM
    }
    else if (ntpTime[h24] == 12)
    { // Time is PM but don't subtract 12
      ntpTime[AMPM] = 2;   // Time is PM
    }
    else if (ntpTime[h24] == 0)
    { // Set time for 12 midnight
      ntpTime[h12] = 12;
      ntpTime[AMPM] = 1;   // Time is AM
    }
    else
    { // It's morning
      ntpTime[h12] = ntpTime[h24]; 
      ntpTime[AMPM] = 1;   // Time is AM
    }
    
    // Calculate day of week
    // 0 = sunday
    long days = epoch / 86400L;
    ntpTime[dow] = (days+4) % 7;

    return true;
  } // Got UDP packet
  else 
  {  // didn't get UDP Packet
    Serial.println(F("Didn't get UDP Packet"));
    return false;
  }

} // getTime()


//============================================================
// send an NTP request to the time server at the given address 
//============================================================
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp: 		   
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
  
} // sendNTPpacket()
