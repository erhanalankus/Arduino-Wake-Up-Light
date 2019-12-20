#include <IRremote.h>			// https://github.com/z3t0/Arduino-IRremote
#include <LiquidCrystal_I2C.h>	// https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
#include <Wire.h>

#pragma region Definitions

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
const int ticker = 2;

int activeLED = WhiteLEDStrip;

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

// Alarm will end after 3 hours
long alarmDuration = 10800000;

long startTickingAfter = 1800000; // Thirty minutes 1800000
long startingTickerInterval = 4000;
long tickerInterval = startingTickerInterval;
long tickerStateChangeTime = 600000; // Ten minutes 600000
long tickerStateChangedAt = 0;
long tickerToggledAt = 0;
bool tickerActive = false;
bool tickerOn = false;
bool tickerEnabled = true;

int backlightBrightnessForNight = 1;
int backlightBrightnessForDay = 255;
bool isNight = false;
bool backlightManuallyTurnedOff = false;
bool nightLightEnabled = true;

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

// Brightness icon
byte sunCharacter[8] = {
	B00000,
	B00100,
	B10101,
	B01110,
	B11011,
	B01110,
	B10101,
	B00100
};

// Alarm icon
byte bellCharacter[8] = {
	B00000,
	B00100,
	B01010,
	B01010,
	B01010,
	B11111,
	B00100,
	B00000
};

// Large exclamation mark
byte exclamationCharacter[] = {
	B01110,
	B01110,
	B01110,
	B01110,
	B01110,
	B00000,
	B01110,
	B01110
};

byte soundCharacter[] = {
	B00000,
	B00010,
	B00010,
	B00010,
	B00010,
	B01110,
	B01110,
	B00000
};

byte smiley[] = {
	B00000,
	B01010,
	B01010,
	B00000,
	B10001,
	B01110,
	B00000,
	B00000
};

#pragma endregion

void setup()
{
	Wire.begin();

	lcd.begin();
	lcd.createChar(0, sunCharacter);
	lcd.createChar(1, bellCharacter);
	lcd.createChar(2, exclamationCharacter);
	lcd.createChar(3, soundCharacter);
	lcd.createChar(4, smiley);

	irrecv.enableIRIn();

	pinMode(backlightLCD, OUTPUT);
	pinMode(WhiteLEDStrip, OUTPUT);
	pinMode(RedLEDStrip, OUTPUT);
	pinMode(ticker, OUTPUT);

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
					if (nightLightEnabled)
					{
						ActivateNightLight();
					}
					else
					{
						LightOff();
					}
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

			if (millis() - alarmTurnedOnAt > startTickingAfter)
			{
				if (millis() - tickerStateChangedAt > tickerStateChangeTime)
				{
					tickerActive = true;
					tickerStateChangedAt = millis();
					tickerToggledAt = millis();
					DecreaseTickerInterval();
				}
			}
		}
		// Once at the end of alarm duration
		else
		{
			brightness = 0;
			SetBrightness(brightness);
			alarmActive = false;
			tickerActive = false;
			tickerStateChangedAt = millis();
			tickerInterval = startingTickerInterval;
		}
	}

	if (tickerActive)
	{
		if (millis() - tickerToggledAt > tickerInterval)
		{
			tickerToggledAt = millis();
			ToggleTicker();
		}
	}

	/* IR Codes for my Remote
	Button			Code
	Play/Pause	-->	0xFFA25D
	CH-			-->	0xFF629D
	CH+			-->	0xFFE21D
	EQ			-->	0xFF22DD
	VOL-		-->	0xFF02FD
	VOL+		-->	0xFFC23D
	PREV		-->	0xFFA857
	NEXT		-->	0xFF906F
	PICK SONG	-->	0xFF42BD
	CH SET		-->	0xFF52AD
	0			-->	0xFFE01F
	1			-->	0xFF6897
	2			-->	0xFF9867
	3			-->	0xFFB04F
	4			-->	0xFF30CF
	5			-->	0xFF18E7
	6			-->	0xFF7A85
	7			-->	0xFF10EF
	8			-->	0xFF38C7
	9			-->	0xFF5AA5
	*/

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

		if (results.value == 0xFF6897) //1 button
		{
			tickerEnabled = !tickerEnabled;
			PushedAnyButton();
		}

		if (results.value == 0xFF30CF) //4 button
		{
			nightLightEnabled = !nightLightEnabled;
			PushedAnyButton();
		}

		irrecv.resume();

		// This delay is recommended by the IRRemote library.
		delay(100);
	}
}

void ActivateNightLight() {
	LightOff();
	activeLED = RedLEDStrip;
	brightness = 10;
	SetBrightness(brightness);
}

void DecreaseTickerInterval() {
	if (tickerInterval % 2 == 1)
	{
		tickerInterval -= 1;
	}
	tickerInterval /= 2;
}

void ToggleTicker() {
	if (!tickerEnabled)
	{
		return;
	}

	if (tickerOn)
	{
		digitalWrite(ticker, LOW);
		tickerOn = false;
	}
	else
	{
		digitalWrite(ticker, HIGH);
		tickerOn = true;
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
		tickerActive = false;
		tickerStateChangedAt = millis();
		tickerInterval = startingTickerInterval;
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
	LightOff();
	activeLED = WhiteLEDStrip;
	alarmActive = true;
	alarmTurnedOnAt = millis();
	tickerStateChangedAt = millis();
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
	lcd.print(" ");
	if (nightLightEnabled)
	{
		lcd.write(byte(4)); //smiley
	}
	else
	{
		lcd.print(" ");
	}
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
	PrintTimeValueOnLCD(alarmHour);
	lcd.print(":");
	PrintTimeValueOnLCD(alarmMinute);
	if (alarmOn)
	{
		if (alarmActive)
		{
			lcd.write(byte(2)); //Exclamation icon
		}
		else
		{
			lcd.write(byte(1)); //Bell icon
		}
	}
	else
	{
		lcd.print("-");
	}
	lcd.print("     ");
	if (tickerEnabled)
	{
		lcd.write(byte(3)); //Sound icon
	}
	else
	{
		lcd.print("-");
	}
	if (alarmActive)
	{
		lcd.print(tickerInterval);
	}
	else
	{
		lcd.print(DayOfWeekForLCD());
	}
}

String DayOfWeekForLCD() {
	switch (dayOfWeek)
	{
	case 1:
		return " sun";
		break;
	case 2:
		return " mon";
		break;
	case 3:
		return " tue";
		break;
	case 4:
		return " wed";
		break;
	case 5:
		return " thu";
		break;
	case 6:
		return " fri";
		break;
	case 7:
		return " sat";
		break;
	default:
		break;
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
void readDS3231time(byte* second, byte* minute, byte* hour, byte* dayOfWeek,
	byte* dayOfMonth, byte* month, byte* year)
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
