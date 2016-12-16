#include <IRremote.h>
#include <Wire.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
int RECV_PIN = 14;
IRrecv irrecv(RECV_PIN);
decode_results results;

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year; // Values to read from DS3231
byte alarmHour = 15;
byte alarmMinute = 15;
bool alarmOn = true;
bool alarmActive = false;
long lastTimeReading = 0;

const int backlightLCD = 2;
const int btnAlarmHour = 3;
const int btnAlarmMinute = 4;
const int btnAlarmToggle = 5;
const int ledMosfet = 6;

long alarmTurnedOnAt = 0;
long alarmDuration = 3600000; //1 hour.
long alarmStepTime = 0;

long pushedButtonAt = 0;
long backlightDuration = 10000;
bool backlightOnForButton = false;

int brightness = 0;
int brightnessStep = 51;
int smallBrightnessStep = 10;
bool lightOn = false;


// Variables for debouncing the buttons
int stateBtnAlarmHour = 0;
int stateBtnAlarmMinute = 0;
int stateBtnAlarmToggle = 0;
int lastStateBtnAlarmHour = 0;
int lastStateBtnAlarmMinute = 0;
int lastStateBtnAlarmToggle = 0;
int readingBtnAlarmHour = 0;
int readingBtnAlarmMinute = 0;
int readingBtnAlarmToggle = 0;
long lastDebounceTimeBtnAlarmHour = 0;
long lastDebounceTimeBtnAlarmMinute = 0;
long lastDebounceTimeBtnAlarmToggle = 0;
const long debounceDelay = 50;

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

void setup()
{
	Wire.begin();
	irrecv.enableIRIn();
	Serial.begin(9600);
	pinMode(backlightLCD, OUTPUT);
	pinMode(ledMosfet, OUTPUT);
	pinMode(btnAlarmHour, INPUT);
	pinMode(btnAlarmMinute, INPUT);
	pinMode(btnAlarmToggle, INPUT);

	lcd.createChar(0, sunCharacter);
	lcd.begin(16, 2);
	/* set the initial time here:
	   DS3231 seconds, minutes, hours, day(1 for sunday, 2 for monday...), date, month, year
	   Set the time by uncommenting the following line after editing the values and load the sketch on your arduino. Right after that,
	    comment out the line and load the sketch again. */

	// setDS3231time(00,59,23,1,31,12,16);
}

void loop()
{
	ListenForButtonPress();

	if (millis() - lastTimeReading > 1000) //Once every second
	{
		ReadTime();
		DisplayTimeAndDateOnSerialMonitor();
		RefreshLCD();
		if (alarmOn)
		{
			if (second == 0)
			{
				CheckAlarm();
			}
		}
		lastTimeReading = millis();
	}

	if (alarmActive)
	{
		if (millis() - alarmTurnedOnAt < alarmDuration) // For the duration of alarm
		{
			digitalWrite(backlightLCD, HIGH);
			if (millis() - alarmStepTime > 7000) // Once in seven seconds, will  reach full brightness at 30 minutes
			{
				ChangeBrightness(1);
				alarmStepTime = millis();
			}
		}
		else // Once at the end of alarm duration
		{
			brightness = 0;
			SetBrightness(brightness);
			alarmActive = false;
			digitalWrite(backlightLCD, LOW);
		}
	}

	if (backlightOnForButton)
	{
		if (millis() - pushedButtonAt < backlightDuration) // For the backlight duration
		{
			digitalWrite(backlightLCD, HIGH);
		}
		else // Once at the end of backlight duration
		{
			digitalWrite(backlightLCD, LOW);
			backlightOnForButton = false;
		}
	}

	if (irrecv.decode(&results)) {		// If a button is pressed on the IR remote
		if (results.value == 0xFFA25D)
		{
			LightOn();
			PushedAnyButton();
		}
		if (results.value == 0xFF22DD)
		{
			LightOff();
			PushedAnyButton();
		}
		if (results.value == 0xFFE21D)
		{
			ChangeBrightness(brightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF629D)
		{
			ChangeBrightness(-brightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFFC23D)
		{
			ChangeBrightness(smallBrightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF02FD)
		{
			ChangeBrightness(-smallBrightnessStep);
			PushedAnyButton();
		}
		if (results.value == 0xFF52AD)
		{
			ChangeBrightness(-1);
			PushedAnyButton();
		}
		if (results.value == 0xFF5AA5)
		{
			ChangeBrightness(1);
			PushedAnyButton();
		}
		irrecv.resume();
	}
	// delay(100);
}

// Most of the code here is to make buttons work well. See lines 225-226, 244-245, 263-264 for what happens on button press.   
// https://www.arduino.cc/en/Tutorial/Debounce 
void ListenForButtonPress()
{
	readingBtnAlarmHour = digitalRead(btnAlarmHour);
	readingBtnAlarmMinute = digitalRead(btnAlarmMinute);
	readingBtnAlarmToggle = digitalRead(btnAlarmToggle);

	if (readingBtnAlarmHour != lastStateBtnAlarmHour)
	{
		lastDebounceTimeBtnAlarmHour = millis();
	}
	if (readingBtnAlarmMinute != lastStateBtnAlarmMinute)
	{
		lastDebounceTimeBtnAlarmMinute = millis();
	}
	if (readingBtnAlarmToggle != lastStateBtnAlarmToggle)
	{
		lastDebounceTimeBtnAlarmToggle = millis();
	}

	if (millis() - lastDebounceTimeBtnAlarmHour > debounceDelay)
	{
		if (readingBtnAlarmHour != stateBtnAlarmHour)
		{
			stateBtnAlarmHour = readingBtnAlarmHour;
			if (stateBtnAlarmHour == LOW)
			{
				if (backlightOnForButton)
				{
					IncreaseAlarmHour();
					PushedAnyButton();
				}
				else
				{
					PushedAnyButton();
				}
			}
		}
	}
	if (millis() - lastDebounceTimeBtnAlarmMinute > debounceDelay)
	{
		if (readingBtnAlarmMinute != stateBtnAlarmMinute)
		{
			stateBtnAlarmMinute = readingBtnAlarmMinute;
			if (stateBtnAlarmMinute == LOW)
			{
				if (backlightOnForButton)
				{
					IncreaseAlarmMinute();
					PushedAnyButton();
				}
				else
				{
					PushedAnyButton();
				}
			}
		}
	}
	if (millis() - lastDebounceTimeBtnAlarmToggle > debounceDelay)
	{
		if (readingBtnAlarmToggle != stateBtnAlarmToggle)
		{
			stateBtnAlarmToggle = readingBtnAlarmToggle;
			if (stateBtnAlarmToggle == LOW)
			{
				if (backlightOnForButton)
				{
					ToggleAlarm();
					PushedAnyButton();
				}
				else
				{
					PushedAnyButton();
				}
			}
		}
	}
	lastStateBtnAlarmHour = readingBtnAlarmHour;
	lastStateBtnAlarmMinute = readingBtnAlarmMinute;
	lastStateBtnAlarmToggle = readingBtnAlarmToggle;
}

void ReadTime()
{
	// retrieve data from DS3231
	readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
}


void DisplayTimeAndDateOnSerialMonitor()
{
	Serial.print(hour, DEC);
	Serial.print(":");
	if (minute < 10)
	{
		Serial.print("0");
	}
	Serial.print(minute, DEC);
	Serial.print(":");
	if (second < 10)
	{
		Serial.print("0");
	}
	Serial.print(second, DEC);
	Serial.print(" ");
	Serial.print(dayOfMonth, DEC);
	Serial.print("/");
	Serial.print(month, DEC);
	Serial.print("/");
	Serial.print(year, DEC);
	Serial.print(" Day of week: ");
	switch (dayOfWeek) {
	case 1:
		Serial.println("Sunday");
		break;
	case 2:
		Serial.println("Monday");
		break;
	case 3:
		Serial.println("Tuesday");
		break;
	case 4:
		Serial.println("Wednesday");
		break;
	case 5:
		Serial.println("Thursday");
		break;
	case 6:
		Serial.println("Friday");
		break;
	case 7:
		Serial.println("Saturday");
		break;
	}
}


void RefreshLCD()
{
	lcd.clear();
	PrintOnLCD(hour);
	lcd.print(":");
	PrintOnLCD(minute);
	lcd.print(":");
	PrintOnLCD(second);
	lcd.print("   ");
	if (alarmActive)
	{
		lcd.print("!");
	}
	else
	{
		lcd.print(" ");
	}
	lcd.write(byte(0));
	lcd.print(brightness);

	lcd.setCursor(0, 1);
	lcd.print("Alarm:");
	PrintOnLCD(alarmHour);
	lcd.print(":");
	PrintOnLCD(alarmMinute);
	if (alarmOn)
	{
		lcd.print(" (on)");
	}
	else
	{
		lcd.print("(off)");
	}
}

void PrintOnLCD(byte value)
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

void BacklightForButtonPress()
{
	backlightOnForButton = true;
	pushedButtonAt = millis();
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
	analogWrite(ledMosfet, brightnessValue);
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
	BacklightForButtonPress();
	if (alarmActive)
	{
		alarmActive = false;
		brightness = 0;
		SetBrightness(brightness);
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
