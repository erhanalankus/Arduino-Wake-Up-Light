# Arduino Wake Up Light
An alarm clock which turns on the lights very slowly. Lights can be controlled manually with an IR remote.
Video: https://youtu.be/UwAyBwA90BE


![Breadboard](https://raw.githubusercontent.com/erhanalankus/Arduino-Wake-Up-Light/master/breadboard.jpg)

# Step 1: Build the circuit

![Schematics](https://raw.githubusercontent.com/erhanalankus/Arduino-Wake-Up-Light/master/schematics_.jpg)

# Step 2: Set the time

Before uploading the sketch to your Arduino, change the values at line 93 with the current values for the time and date and uncomment the line. Upload the sketch. Comment out line 93 and upload again. You can see time and date on the serial monitor. The time won't be lost unless you remove the battery from the DS3231 RTC module. I may add the option to edit the time later, pull requests are welcome.

# Step 3: Set the remote

Go to line 148, there you will find what happens for which remote button. Those values most likely won't work with the remote that you are using. Watch this video and you'll learn how to setup your remote: https://youtu.be/ftdJ0R_5NZk The remote is optional. Its only job is to adjust the brightness of the LEDs manually.

# Step 4: Using the device

There are three buttons. First two buttons set the alarm, the third button toggles alarm on/off. When the alarm is active, brightness of the lights will increase 1 unit every 7 seconds. Maximum brightness value is 255. It will take 30 minutes to reach full brightness and after an hour, the lights will turn off. You can change alarm duration at line 24 and the speed at line 120. Pressing any button will deactivate the alarm.

You can buy all the parts on aliexpress.
