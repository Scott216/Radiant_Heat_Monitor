Arduino sketch that reads 60 OneWire sensors, some thermistors and thermocouples
Arduino acts as a webserver and displays the data


To Do: 
- look at these bug fixes http://www.pjrc.com/teensy/td_libs_OneWire.html#bugs
- If no OneWire sensors are hooked up, why is temp in some of them 32 and others zero
- Add I2C so you can send data to pachube via another arduino
- add icon that displays if heat is on or off.  Use img tag like you do with the graph
- try removing the request temperature function and see what happens.  Does the temperature still update?
- add code for three thermocouples
- Historical page doesn't work remotely, I think because it's losing the port info
  - Figure out how to use page names http://www.chipkin.com/arduino-ethernet-shield-a-better-webserver/
  - https://github.com/sirleech/Webduino
  - http://playground.arduino.cc/Code/WebServer
       http://arduino.cc/forum/index.php/topic,95113.0.html
