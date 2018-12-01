#include <IRremote.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// Set the LCD address to 0x3F for a 16 chars and 2 line display
// You can find the address of your I2C device using https://playground.arduino.cc/Main/I2cScanner
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// 14 is A0 on Arduino UNO, this is the pin for the IR receiver
int RECV_PIN = 14;
IRrecv irrecv(RECV_PIN);
decode_results results;

const int backlightLCD = 9;
const int WhiteLEDStrip = 5;
const int RedLEDStrip = 6;

int activeLED = WhiteLEDStrip;

// Defining a special character to use as brightness icon
byte sunCharacter[8] = {
	0b00000,
	0b00100,
	0b10101,
	0b01110,
	0b11011,
	0b01110,
	0b10101,
	0b00100
};

// Brightness step variables set the amount of brightness to change, while using the lights manually(not for alarm).
int brightness = 0;
int brightnessStep = 51;
int smallBrightnessStep = 10;
bool lightOn = false;

// Values to read from DS3231 RTC Module
byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

// Default alarm time, in case of power loss during the night, alarm time will reset to this.
byte alarmHour = 6;
byte alarmMinute = 30;

bool alarmOn = true;
bool alarmActive = false;
long alarmTurnedOnAt = 0;
long alarmStepTime = 0;
long lastTimeReading = 0;

// Alarm will end after 4 hours
long alarmDuration = 14400000;

int backlightBrightnessForNight = 1;
int backlightBrightnessForDay = 255;
bool isNight = false;
bool backlightManuallyTurnedOff = false;

// decToBcd & bcdToDec are used for reading the time from the RTC module
#define DS3231_I2C_ADDRESS 0x68
// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
	return((val / 10 * 16) + (val % 10));
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
	return((val / 16 * 10) + (val % 16));
}

void setup()
{
	Wire.begin();

	lcd.begin();
	lcd.createChar(0, sunCharacter);

	irrecv.enableIRIn();

	pinMode(backlightLCD, OUTPUT);
	pinMode(WhiteLEDStrip, OUTPUT);
	pinMode(RedLEDStrip, OUTPUT);

	analogWrite(WhiteLEDStrip, 0);
	analogWrite(RedLEDStrip, 0);

	ReadTime();
	CheckDayOrNight();

	/* set the initial time here:
	   DS3231 seconds, minutes, hours, day(1 for sunday, 2 for monday...), date, month, year
	   Set the time by uncommenting the following line after editing the values and load the sketch on your arduino. Right after that, comment out the line and load the sketch again. */
	   // setDS3231time(00,59,23,1,31,12,16);
}

void loop()
{
	if (OnceEverySecond())
	{
		ReadTime();
		lastTimeReading = millis();

		// Once every minute
		if (second == 0)
		{
			if (alarmOn)
			{
				CheckAlarm();
			}

			// Once every hour
			if (minute == 0)
			{
				CheckDayOrNight();
				if (hour == 1)
				{
					LightOff();
				}
			}
		}

		SetLCDBacklight();
		RefreshLCD();
	}

	if (alarmActive)
	{
		// For the duration of alarm
		if (millis() - alarmTurnedOnAt < alarmDuration)
		{
			// Increase brightness 1 unit once in 7 seconds. The light will reach full brightness in 30 minutes.
			if (millis() - alarmStepTime > 7000)
			{
				ChangeBrightness(1);
				alarmStepTime = millis();
			}
		}
		// Once at the end of alarm duration
		else
		{
			brightness = 0;
			SetBrightness(brightness);
			alarmActive = false;
		}
	}

	// If a button is pressed on the IR remote, see GreatScott's video for how to read the codes for your remote(https://youtu.be/ftdJ0R_5NZk)
	if (irrecv.decode(&results)) {
		if (results.value == 0xFFA25D) //PLAY button
		{
			LightOn();
			PushedAnyButton();
		}
		if (results.value == 0xFF22DD) //EQ button
		{
			LightOff();
			PushedAnyButton();
		}
		if (results.value == 0xFFE21D) //CH+ button
		{
			ChangeBrightness(brightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF629D) //CH- button
		{
			ChangeBrightness(-brightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFFC23D) //VOL+ button
		{
			ChangeBrightness(smallBrightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF02FD) // VOL- button
		{
			ChangeBrightness(-smallBrightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF52AD) //CH SET button
		{
			ChangeBrightness(-1);
			PushedAnyButton();
		}
		if (results.value == 0xFF5AA5) //9 button
		{
			ChangeBrightness(1);
			PushedAnyButton();
		}
		if (results.value == 0xFFA857) //PREV button
		{
			IncreaseAlarmHour();
			PushedAnyButton();
		}
		if (results.value == 0xFF906F) //NEXT button
		{
			IncreaseAlarmMinute();
			PushedAnyButton();
		}
		if (results.value == 0xFFE01F) //0 button
		{
			ToggleAlarm();
			PushedAnyButton();
		}

		if (results.value == 0xFF42BD) //PICK SONG button
		{
			ToggleActiveLEDStrip();
			PushedAnyButton();
		}

		if (results.value == 0xFF38C7) //8 button
		{
			backlightManuallyTurnedOff = !backlightManuallyTurnedOff;
			PushedAnyButton();
		}

		irrecv.resume();

		// This delay is recommended by the IRRemote library.
		delay(100);
	}
}

void LightOn() {
	if (brightness != 255)
	{
		brightness = 255;
		SetBrightness(brightness);
	}
	else
	{
		return;
	}
}

void LightOff() {
	if (lightOn)
	{
		brightness = 0;
		SetBrightness(brightness);
	}
	else
	{
		return;
	}
}

void SetBrightness(int brightnessValue)
{
	analogWrite(activeLED, brightnessValue);
	if (brightnessValue == 0)
	{
		lightOn = false;
	}
	else
	{
		lightOn = true;
	}
}

void PushedAnyButton()
{
	if (alarmActive)
	{
		alarmActive = false;
		LightOff();
	}
}

void ToggleActiveLEDStrip() {
	LightOff();

	if (activeLED == WhiteLEDStrip)
	{
		activeLED = RedLEDStrip;
	}
	else
	{
		activeLED = WhiteLEDStrip;
	}
}

bool OnceEverySecond() {
	if (millis() - lastTimeReading > 1000)
	{
		lastTimeReading = millis();
		return true;
	}
	else
	{
		return false;
	}
}

void ReadTime()
{
	// retrieve data from DS3231
	readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
}

void CheckAlarm()
{
	if (hour == alarmHour)
	{
		if (minute == alarmMinute)
		{
			Alarm();
		}
	}
}

void Alarm()
{
	activeLED = WhiteLEDStrip;
	alarmActive = true;
	alarmTurnedOnAt = millis();
	alarmStepTime = millis();
	backlightManuallyTurnedOff = false;
}

void RefreshLCD()
{
	lcd.clear();
	PrintTimeValueOnLCD(hour);
	lcd.print(":");
	PrintTimeValueOnLCD(minute);
	lcd.print(":");
	PrintTimeValueOnLCD(second);
	lcd.print("  ");
	if (backlightManuallyTurnedOff)
	{
		lcd.print("!");
	}
	else
	{
		lcd.print(" ");
	}
	if (activeLED == WhiteLEDStrip)
	{
		lcd.write(byte(0));
	}
	else
	{
		lcd.print(" ");
	}
	lcd.write(byte(0));
	lcd.print(brightness);

	lcd.setCursor(0, 1);
	if (!alarmActive)
	{
		lcd.print("Alarm:");
	}
	else
	{
		lcd.print("ALARM:");
	}
	PrintTimeValueOnLCD(alarmHour);
	lcd.print(":");
	PrintTimeValueOnLCD(alarmMinute);
	if (alarmOn)
	{
		lcd.print(" (on)");
	}
	else
	{
		lcd.print("(off)");
	}
}

void PrintTimeValueOnLCD(byte value)
{
	if (value < 10)
	{
		lcd.print(0);
	}
	lcd.print(value);
}

void ChangeBrightness(int step) {
	brightness += step;
	if (brightness > 255)
	{
		brightness = 255;
	}
	if (brightness < 0)
	{
		brightness = 0;
	}
	SetBrightness(brightness);
}

void IncreaseAlarmHour()
{
	if (alarmHour == 23)
	{
		alarmHour = 0;
	}
	else
	{
		alarmHour++;
	}
	RefreshLCD();
}

void IncreaseAlarmMinute()
{
	if (alarmMinute == 59)
	{
		alarmMinute = 0;
	}
	else
	{
		alarmMinute++;
	}
	RefreshLCD();
}

void ToggleAlarm()
{
	alarmOn = !alarmOn;
	RefreshLCD();
}

void CheckDayOrNight() {
	if ((hour >= 21) || (hour >= 0 && hour < 9))
	{
		isNight = true;
	}
	else
	{
		isNight = false;
	}
}

void SetLCDBacklight() {
	if (backlightManuallyTurnedOff)
	{
		analogWrite(backlightLCD, 0);
	}
	else if (alarmActive)
	{
		analogWrite(backlightLCD, brightness);
	}
	else
	{
		if (isNight)
		{
			analogWrite(backlightLCD, backlightBrightnessForNight);
		}
		else
		{
			analogWrite(backlightLCD, backlightBrightnessForDay);
		}
	}
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek,
	byte dayOfMonth, byte month, byte year)
{
	// sets time and date data to DS3231
	Wire.beginTransmission(DS3231_I2C_ADDRESS);
	Wire.write(0); // set next input to start at the seconds register
	Wire.write(decToBcd(second)); // set seconds
	Wire.write(decToBcd(minute)); // set minutes
	Wire.write(decToBcd(hour)); // set hours
	Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
	Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
	Wire.write(decToBcd(month)); // set month
	Wire.write(decToBcd(year)); // set year (0 to 99)
	Wire.endTransmission();
}
void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek,
	byte *dayOfMonth, byte *month, byte *year)
{
	Wire.beginTransmission(DS3231_I2C_ADDRESS);
	Wire.write(0); // set DS3231 register pointer to 00h
	Wire.endTransmission();
	Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
	// request seven bytes of data from DS3231 starting from register 00h
	*second = bcdToDec(Wire.read() & 0x7f);
	*minute = bcdToDec(Wire.read());
	*hour = bcdToDec(Wire.read() & 0x3f);
	*dayOfWeek = bcdToDec(Wire.read());
	*dayOfMonth = bcdToDec(Wire.read());
	*month = bcdToDec(Wire.read());
	*year = bcdToDec(Wire.read());
}
