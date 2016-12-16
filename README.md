# Arduino Wake Up Light
An alarm clock which turns on the lights very slowly.
Video: https://youtu.be/UwAyBwA90BE


![Breadboard](https://raw.githubusercontent.com/erhanalankus/Arduino-Wake-Up-Light/master/breadboard.jpg)

# Step 1: Build the circuit

![Schematics](https://raw.githubusercontent.com/erhanalankus/Arduino-Wake-Up-Light/master/schematics.jpg)

# Step 2: Set the time

Before uploading the sketch to your Arduino, change the values at line 93 with the current values for the time and date. Upload the sketch. Comment out line 93 and upload again. The time won't be lost unless you remove the battery from the DS3231 RTC module. I may add the option to edit the time later, pull requests are welcome.

# Step 3: Set the remote

Go to line 148, there you will find what happens for what remote button. Those values most likely won't work with the remote that you are using. Watch this video and you'll learn how to setup your remote: https://youtu.be/ftdJ0R_5NZk
