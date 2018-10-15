# Arduino Wake Up Light
An alarm clock which turns on the lights very slowly. Lights can be controlled manually with an IR remote. White LED strip is for the alarm, Red LED strip is for soft light to use in the evening. 

# Step 1: Build the circuit

![WiringDiagram](https://raw.githubusercontent.com/erhanalankus/Arduino-Wake-Up-Light/master/wiring-diagram.png)

# Step 2: Set the time

Before uploading the sketch to your Arduino, change the values at line 85 with the current values for the time and date and uncomment the line. Upload the sketch. Comment out line 85 and upload again. The time won't be lost unless you remove the battery from the DS3231 RTC module.

# Step 3: Set the remote

Go to line 129. There you will find what happens for which remote button. Those values most likely won't work with the remote that you are using. Watch this video and you'll learn how to setup your remote: https://youtu.be/ftdJ0R_5NZk

# Step 4: Using the device

When the alarm is active, brightness of the lights will increase 1 unit every 7 seconds. Maximum brightness value is 255. Current brightness value is displayed on the top right of the LCD screen. It will take 30 minutes to reach full brightness and after the time stated in the alarmDuration variable(4 hours), the lights will turn off. You can change alarm duration at line 45 and the speed at line 115. Pressing any button will deactivate the alarm.

You can buy all the parts on aliexpress.
