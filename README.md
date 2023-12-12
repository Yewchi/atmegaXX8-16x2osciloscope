# doc
Simple low-resolution (8x5 pixel characters) osciloscope and tuner, with automatically adjusting horizontal and vertical zoom.

Built for an Arduino UNO. Works great with simple waveforms, does okay with light frequency modulation, not suited for mixed and mastered audio.

## warranty
NO WARRANTY

Nothing is guaranteed to work, to be safe, nor easy to set up if you do not already have a working knowledge of a similar LCD connected to an Arduino.. This solution is definitely janky because it's an osciloscope on a 16x2 screen that can only display 8 custom characters at a time but it does get the job done with stuff lying around.

## electrical configuration
Regular AC (or audio) signal will need to be shifted to the 0v to 5v range. E.g. if the voltages range was raw -10V to 10V signal (much greater than listening level AC signal), it will need to be reduced to -2.5V to 2.5V and summed with a 2.5V signal.

[Example -10V to 10V range resistor approximate sum](https://falstad.com/circuit/circuitjs.html?ctz=CQAgjCAMB0l3BWcMBMcUHYMGZIA4UA2ATmIxAUgpABZsKBTAWjDACgAlEJwmkNPNzApBAqCHo0qYKrKjQEbAE7de-fLVEaquSJ241B2NEOESTcqdTkxFKpofNU1xuWEpsA7pvWDhojBQoLxAXE39+QOCVFARCSKCwDHjMIJ04NgB5cBEEnKNCQVk2AA9ubELwQkkq+L4UPhZIADUAHQBnABcAew6ZNvalAEMAOwBzBg6hgFcAEwBLXvb2+bGRoYAbNjH+OLzY+Nw+Yu8I1PBkvL1ukHI1KiliP1loCGk2G+dj2kgn5HkIBA0hI2EA) -- Adjust the triangle wave voltage range and adjust the resistors so that the sample voltage range is ~0V to 5V

Adjust your screen component's brightness to adjust pixel lifetime.

See your LCD spec sheet for other operating voltages and how to connect device and screen power.

## setup
Assuming you have an LCD connected and controlled by an arduino/atmegaxx8 device...

works as compiled and uploaded via ArduinoIDE

Requires: Arduino LiquidCrystal library

reads voltage signal into Analog IN 0

Assuming your LCD is QC1602A-like:
LCD_RS digital 12
LCD_ENABLE digital pin 11
LCD_D4 digital pin 5
LCD_D5 digital pin 4
LCD_D6 digital pin 3
LCD_D7 digital pin 2
