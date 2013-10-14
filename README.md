Welcome To: DIY pH Meter Example Firmware!!
================================

##### Note: This is for use with the LeoPhi Hardware (preferably Version 2, though this current build is for v1)

This is an example sketch which turns leoPhi hardware + a 4DSystems OLED into a DIY pH Meter. 
It outputs basic data via USB serial and drive the screen via Serial1. using a boost converter/LiPo battery charger allow the meter to monitor its own battery from "drained" to fully charged/charging. 

Building a versatile pH meter is a snap
-------------------------

The atmega32u4 allows this meter to be quite powerful in addition to extremely easy to program and build. if you need to update or charge the battery just plug in the meter via USB, upload a new sketch while the battery charges and off you go. This is a great project and example on how a commercial pH meter's user experience can be improved significantly.

Please see [LeoPhi's Project page](http://www.sparkyswidgets.com/portfolio-item/leophi-usb-arduino-ph-sensor/) for more information!
<http://www.sparkyswidgets.com/portfolio-item/leophi-usb-arduino-ph-sensor/>

Whats in the firmware?
-------------------------

Most of this sketch is the same as the basic LeoPhi firmware, the main changes are those added for Battery monitoring and driving the OLED screen. Some of the user experience to refined and focused to handheld single button interaction. The main button on the unit will switch states allow for various calibration and information "pages". A main readout with the most useful information is displayed as the default state. Battery levels are indicated by both color and level in the battery outline in the upper right corner. probe slope condition and pH are then displayed in the main view area of screen. by adjusting color a quick out of ideal visual indication can be giving. Extra sensors could be added via I2C/SPI/ or 1wire in the case of temperature compensation.

Installation Info
-------------------------

The best way to use LeoPhi hardware in Arduino is to download my current build for whichever OS you are using.
##### Some of the changes needed (pretty standard to adding variants to Arduino) 
- A driver maybe needed on windows (a legacy <1.0.4 IDE driver inf file is provided)

Luckily we have a basic package with these changes [here](http://www.sparkyswidgets.com/Portals/0/LeoPhiSetup.zip).
**Plus use my Arduino Fork for the newest revisions as this is best supported!** 
**Even better see my Arduino Fork where I keep these changes up to date with the newest versions of Arduino!**
<https://github.com/SparkysWidgets/Arduino>

Basic Usage
-------------------------

Since this is geared for use as a handheld Meter, its intended to be very easy to use, calibrate and modify.
You basically turn it on, and take reading :) For calibration follow the on screen instruction(put probe in pH 7 solution press button, put probe in pH 4 solution press button, calibrated push button to proceed!) The USB serial command set is still active as well!

####Some of the commands are:
- C : Continuous Read Mode: Dump readings and data every second
- R : Single pH reading: response "pH: XX.XX" where XX.XX is the pH
- E : Exit continuous read mode
- S : set pH7 Calibration point
- F : set pH4 Calibration point: also recalcs probe slope and saves settings to EEPROM
- T : set pH10 Calibration point: also recalcs probe slope and saves settings to EEPROM 
- X : restore settings to default and ideal probe conditions

Hardware: Schematics and Layouts
-------------------------

- Take a look in [LeoPhi's Hardware Repo](https://github.com/SparkysWidgets/LeoPhiHW) for the EAGLE files!
- Check out my I2C pH interface[MinipH](http://www.sparkyswidgets.com/Projects/MinipH.aspx) for a really cost effective PH Probe interface!


License Info
-------------------------

<a rel="license" href="http://creativecommons.org/licenses/by-sa/3.0/deed.en_US"><img alt="Creative Commons License" style="border-width: 0px;" src="http://i.creativecommons.org/l/by-sa/3.0/88x31.png" /></a><br />
<span xmlns:dct="http://purl.org/dc/terms/" property="dct:title">LeoPhi</span> by <a xmlns:cc="http://creativecommons.org/ns#" href="www.sparkyswidgets.com" property="cc:attributionName" rel="cc:attributionURL">Ryan Edwards, Sparky's Widgets</a> is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-sa/3.0/deed.en_US">Creative Commons Attribution-ShareAlike 3.0 Unported License</a>.<br />
Based on a work at <a xmlns:dct="http://purl.org/dc/terms/" href="/portfolio-item/leophi-usb-arduino-ph-sensor/" rel="dct:source">http://www.sparkyswidgets.com/portfolio-item/leophi-usb-arduino-ph-sensor/</a>