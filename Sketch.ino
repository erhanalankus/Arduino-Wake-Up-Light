#include <IRremote.h>
#include <Wire.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 10, 2, 8, 7);
int RECV_PIN = 14;
IRrecv irrecv(RECV_PIN);
decode_results results;

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; // Values to read from DS3231 RTC Module
byte alarmHour = 6;
byte alarmMinute = 0;
bool alarmOn = true;
bool alarmActive = false;
long lastTimeReading = 0;

const int backlightLCD = 9;

const int LEDStrip12V = 6;
const int LEDStrip6V = 5;

int activeLED = LEDStrip12V;

long alarmTurnedOnAt = 0;
long alarmDuration = 14400000; //4 hours.
long alarmStepTime = 0;

int brightness = 0;
int softLightBrightness = 0;
int brightnessStep = 51;
int smallBrightnessStep = 10;
bool lightOn = false;

int backlightBrightnessForNight = 20;
int backlightBrightnessForDay = 255;
bool isNight = false;
bool backlightManuallyTurnedOff = false;

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

//Defining a special character to use as brightness icon
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

void setup()
{
	Wire.begin();
	irrecv.enableIRIn();
	pinMode(backlightLCD, OUTPUT);
	pinMode(LEDStrip12V, OUTPUT);
	pinMode(LEDStrip6V, OUTPUT);

	lcd.createChar(0, sunCharacter);
	lcd.begin(16, 2);
	/* set the initial time here:
	   DS3231 seconds, minutes, hours, day(1 for sunday, 2 for monday...), date, month, year
	   Set the time by uncommenting the following line after editing the values and load the sketch on your arduino. Right after that, comment out the line and load the sketch again. */

	   // setDS3231time(00,59,23,1,31,12,16);
}

void loop()
{
	if (millis() - lastTimeReading > 1000) //Once every second
	{
		ReadTime();
		lastTimeReading = millis();
		CheckAlarmEveryMinute();
		if (HourStart())
		{
			CheckDayOrNight();
			DisableBacklightOverrideAt(6);
			AdjustSettingsForSleepTimeAt(2);
		}
		RefreshLCD();
	}

	if (!backlightManuallyTurnedOff)
	{
		LCDBacklight();
	}
	else
	{
		analogWrite(backlightLCD, 0);
	}

	if (alarmActive)
	{
		if (millis() - alarmTurnedOnAt < alarmDuration) //For the duration of alarm
		{
			if (millis() - alarmStepTime > 7000) //Once in 7 seconds, will  reach full brightness at 30 minutes
			{
				ChangeBrightness(1);
				alarmStepTime = millis();
			}
		}
		else //Once at the end of alarm duration
		{
			brightness = 0;
			SetBrightness(brightness);
			alarmActive = false;
		}
	}

	if (irrecv.decode(&results)) {		//If a button is pressed on the IR remote
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
			ToggleLedMosfet();
			PushedAnyButton();
		}

		if (results.value == 0xFF38C7) //8 button
		{
			backlightManuallyTurnedOff = !backlightManuallyTurnedOff;
			PushedAnyButton();
		}

		irrecv.resume();
	}
	delay(100); //Documentation recommended this, but it may not be necessary.
}

void CheckAlarmEveryMinute() {
	if (alarmOn)
	{
		if (second == 0)
		{
			CheckAlarm();
		}
	}
}

void DisableBacklightOverrideAt(int hourToDisableBacklightOverride) {
	if (hour == hourToDisableBacklightOverride)
	{
		backlightManuallyTurnedOff = false;
	}
}

void LCDBacklight() {
	if (isNight)
	{
		analogWrite(backlightLCD, backlightBrightnessForNight);
	}
	else
	{
		analogWrite(backlightLCD, backlightBrightnessForDay);
	}
}

void ToggleLedMosfet() {
	if (activeLED == LEDStrip12V)
	{
		activeLED = LEDStrip6V;
		analogWrite(LEDStrip12V, 0);
		brightness = 0;
	}
	else
	{
		activeLED = LEDStrip12V;
		analogWrite(LEDStrip6V, 0);
		brightness = 0;
	}
}

void ReadTime()
{
	// retrieve data from DS3231
	readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
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
	if (activeLED == LEDStrip12V)
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
	alarmActive = true;
	alarmTurnedOnAt = millis();
	alarmStepTime = millis();
	backlightManuallyTurnedOff = false;
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
	if (brightness != 0)
	{
		brightness = 0;
		SetBrightness(brightness);
	}
	else
	{
		return;
	}
}

void PushedAnyButton()
{
	if (alarmActive)
	{
		alarmActive = false;
		brightness = 0;
		SetBrightness(brightness);
	}
}

void CheckDayOrNight() {
	if ((hour > 20) || (hour >= 0 && hour < 6))
	{
		isNight = true;
	}
	else
	{
		isNight = false;
	}
}

bool HourStart() {
	if (second == 0)
	{
		if (minute == 0)
		{
			return true;
		}
	}
	return false;
}

void AdjustSettingsForSleepTimeAt(int hourToSwitchToSleepMode) {
	if (hour == hourToSwitchToSleepMode)
	{
		backlightManuallyTurnedOff = true;
		brightness = 0;
		SetBrightness(brightness);
		activeLED = LEDStrip12V;
	}
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
	dayOfMonth, byte month, byte year)
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
void readDS3231time(byte *second,
	byte *minute,
	byte *hour,
	byte *dayOfWeek,
	byte *dayOfMonth,
	byte *month,
	byte *year)
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
