#include <SoftwareSerial.h>

// Pin assignments
const int relayPin = 13;      // Relay IN1
const int motorPinIN1 = 10;   // IBT-2 Motor Driver IN1 connected to pin 10 (PWM capable)
const int motorPinIN2 = 9;    // IBT-2 Motor Driver IN2 connected to pin 9 (PWM capable)
SoftwareSerial nextion(2, 3); // Nextion TX, RX connected to pins 2 (RX) and 3 (TX)

// Motor control variables
bool motorRunning = false;        // Track if the motor is currently running
bool motorDirection = true;       // true for one direction, false for reverse
unsigned long motorStartTime = 0; // To track the timing of motor behavior
int motorState = 0;               // Current state of the motor sequence
int maxMotorSpeed = 255;          // Maximum motor speed
bool isSliderSet = false;         // Indicates if the slider has been set

// Variables for ramp duration, top speed duration, and pause duration
unsigned long minRampDuration = 5000;      // 5 seconds in milliseconds
unsigned long maxRampDuration = 60000;     // 60 seconds in milliseconds
unsigned long rampDuration = 5000;         // Initialize with the minimum ramp duration
unsigned long decelerationDuration = 5000; // Deceleration duration for gradual stop
unsigned long topSpeedDuration = 10000;    // Default 10 seconds in milliseconds
unsigned long pauseDuration = 5000;        // Default 5 seconds in milliseconds for pause duration

// New variable for ramp up/down duration
unsigned long rampUpDownDuration = 5000; // Default 5 seconds in milliseconds for ramp up/down duration

// Variables for Total Run Time
unsigned long totalRunTime = 0;
unsigned long totalRunTimeStart = 0;    // Timestamp when Total Run Time starts
unsigned long totalRunTimeElapsed = 0;  // Total Run Time elapsed time
unsigned long totalRunTimePausedAt = 0; // Timestamp when Total Run Time was paused
bool totalRunTimePaused = false;        // Flag to indicate if Total Run Time is paused
bool totalRunTimeStarted = false;       // Flag to indicate if Total Run Time has been started
unsigned long timenow = 0;
unsigned long remainingTime = 0;
unsigned long countDown = 0;
bool motorAction = false;
bool timerIsRunning = true;
void setup()
{
    pinMode(relayPin, OUTPUT);
    pinMode(motorPinIN1, OUTPUT);
    pinMode(motorPinIN2, OUTPUT);
    digitalWrite(relayPin, LOW);
    digitalWrite(motorPinIN1, LOW);
    digitalWrite(motorPinIN2, LOW);
    nextion.begin(9600);
    Serial.begin(9600);
    Serial.println("Setup complete");
}

void loop()
{
    static byte data[10];
    static byte count = 0;

    while (nextion.available())
    {
        byte incomingByte = nextion.read();
        data[count++] = incomingByte;

        if (count >= 3 && data[count - 1] == 0xFF && data[count - 2] == 0xFF && data[count - 3] == 0xFF)
        {
            processNextionCommand(data, count - 3); // Process the command
            count = 0;                              // Reset count after processing a command
        }
    }

    if (motorRunning && !totalRunTimePaused)
    {
        manageMotorBehavior();
        checkTotalRunTime();
    }
}

void processNextionCommand(byte *data, byte count)
{
    // Debug print to see received data
    Serial.print("Received data: ");
    for (byte i = 0; i < count; i++)
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (memcmp(data, "\x65\x00\x01\x01\xFF\xFF\xFF", 7) == 0)
    { // Play button
        Serial.println("Play button pressed.");
        if (!totalRunTimeStarted && !totalRunTimePaused)
        {
            if (!isSliderSet)
            {
                Serial.println("Redirecting to page 1 for b0 button press.");
                sendNextionCommand("page 1");
            }
            else
            {
                if (!motorAction && !timerIsRunning)
                {
                    startTotalRunTime(); // Start the Total Run Time if the motor is not running
                    Serial.println("Starting Total Run Time.");
                }
            }
            totalRunTimeStarted = true;
        }
        else
        {
            if (!motorRunning)
            {
                // resumeTotalRunTime();
                if (totalRunTime > 0)
                {
                    // totalRunTimeStart = millis() - totalRunTimeElapsed; // Resume from remaining time
                    resumeTotalRunTime();
                }
                Serial.println("Resuming motor and Total Run Time.");
            }
            else
            {
                Serial.println("Resuming motor.");
            }
            startMotorSequence();
        }
    }
    else if (data[0] == 0x65 && data[1] == 0x01 && data[2] == 0x0A && data[3] == 0x01)
    { // b0 button
        Serial.println("b0 button pressed, motor operation now allowed.");
        isSliderSet = true;
    }
    else if (data[0] == 0x42 && data[1] == 0x42 && data[2] == 0x71)
    { // Motor speed slider
        int sliderValue = data[3];
        maxMotorSpeed = map(sliderValue, 0, 100, 0, 255);
        Serial.print("Max Motor Speed: ");
        Serial.print(map(maxMotorSpeed, 0, 255, 0, 100));
        Serial.println("%");
        isSliderSet = true;
    }
    else if (data[0] == 0x41 && data[1] == 0x01 && data[2] == 0x71)
    { // Ramp duration slider h0
        int sliderValue = data[3];
        rampDuration = map(sliderValue, 5, 60, minRampDuration, maxRampDuration);
        Serial.print("Ramp duration set to: ");
        Serial.println(rampDuration / 1000);
    }
    else if (data[0] == 0x45 && data[1] == 0x45 && data[2] == 0x71)
    { // Ramp up/down duration slider h0
        int sliderValue = data[3];
        rampUpDownDuration = map(sliderValue, 0x04, 0x3C, 4000, 60000);
        rampUpDownDuration = constrain(rampUpDownDuration, 4000, 60000);
        Serial.print("Ramp up/down duration set to: ");
        Serial.println(rampUpDownDuration / 1000);
    }
    else if (data[0] == 0x40 && data[1] == 0x40 && data[2] == 0x71)
    { // Pause duration slider h5
        int sliderValue = data[3];
        pauseDuration = map(sliderValue, 0x04, 0x3C, 4000, 60000);
        pauseDuration = constrain(pauseDuration, 4000, 60000);
        Serial.print("Pause duration set to: ");
        Serial.println(pauseDuration / 1000);
    }
    else if (data[0] == 0x41 && data[1] == 0x41 && data[2] == 0x71)
    { // New slider h4 (Top Speed Duration)
        int sliderValue = data[3];
        topSpeedDuration = map(sliderValue, 4, 60, 4000, 60000);
        topSpeedDuration = constrain(topSpeedDuration, 4000, 60000);
        Serial.print("Top Speed Run Duration set to: ");
        Serial.println(topSpeedDuration / 1000);
    }
    else if (memcmp(data, "\x46\x46\x71", 3) == 0)
    { // New slider h1 (Total Run Time)
        int sliderValue = data[3];
        totalRunTime = map(sliderValue, 0x00, 0x64, 0, 100); // Ensure it doesn't exceed 100
        countDown = totalRunTime * 60;
        Serial.print("Total Run Time set to: ");
        Serial.print(totalRunTime); // Display the value directly
        Serial.println(" minutes");
    }
    else if (memcmp(data, "\x65\x00\x02\x01\xFF\xFF\xFF", 7) == 0)
    { // Stop button
        stopMotor();
    }
    else if (memcmp(data, "\x65\x00\x04\x01\xFF\xFF\xFF", 7) == 0)
    { // Relay toggle button
        toggleRelay();
    }
    else if (memcmp(data, "\x65\x00\x03\x01\xFF\xFF\xFF", 7) == 0)
    { // Pause button
        if (motorRunning)
        {
            pauseTotalRunTime();
            stopMotor();
        }
    }
    else if (memcmp(data, "\x65\x00\x05\x01\xFF\xFF\xFF", 7) == 0)
    { // Resume button
        if (motorRunning)
        {
            resumeTotalRunTime();
            startMotorSequence();
        }
    }
}

void sendNextionCommand(String command)
{
    nextion.print(command);
    nextion.write(0xFF);
    nextion.write(0xFF);
    nextion.write(0xFF);
}

void startMotorSequence()
{
    motorAction = false;
    motorRunning = true;
    motorState = 1; // Start with ramping up
    motorStartTime = millis();
    Serial.println("Motor started");
}

void stopMotor()
{
    if (motorRunning)
    {
        motorState = 6;            // Set state to deceleration phase
        motorStartTime = millis(); // Reset the timer for deceleration
    }
}

void toggleRelay()
{
    static bool relayState = false;
    relayState = !relayState;
    digitalWrite(relayPin, relayState ? HIGH : LOW);
    Serial.println(relayState ? "Relay turned ON" : "Relay turned OFF");
}

void manageMotorBehavior()
{
    unsigned long currentTime = millis();
    int motorSpeed;

    switch (motorState)
    {
    case 1: // Ramping up
        motorSpeed = map(currentTime - motorStartTime, 0, rampUpDownDuration, 0, maxMotorSpeed);
        Serial.print("ramping up :");
        Serial.println(motorSpeed);
        if (motorSpeed >= maxMotorSpeed)
        {
            motorSpeed = maxMotorSpeed;
            motorState = 2;
            motorStartTime = currentTime;
        }
        break;
    case 2: // Running at max speed
        motorSpeed = maxMotorSpeed;
        Serial.print("Running at max speed :");
        Serial.println(motorSpeed);
        if (currentTime - motorStartTime >= topSpeedDuration)
        {
            motorState = 3;
            motorStartTime = currentTime;
        }
        break;
    case 3: // Ramping down
        motorSpeed = map(currentTime - motorStartTime, 0, rampUpDownDuration, maxMotorSpeed, 0);
        Serial.print("ramping down :");
        Serial.println(motorSpeed);
        if (motorSpeed <= 0)
        {
            motorSpeed = 0;
            motorState = 4;
            motorStartTime = currentTime;
        }
        break;
    case 4: // Pausing at 0%
        if (currentTime - motorStartTime >= pauseDuration)
        {
            motorState = 5;
            motorStartTime = currentTime;
        }
        break;
    case 5: // Reverse direction
        motorDirection = !motorDirection;
        motorState = 1; // Repeat cycle starting with ramp up
        motorStartTime = currentTime;
        break;
    case 6: // Decelerating for stop
        motorSpeed = map(currentTime - motorStartTime, 0, decelerationDuration, maxMotorSpeed, 0);
        Serial.print("Decelerating for stop :");
        Serial.println(motorSpeed);
        if (motorSpeed <= 0)
        {
            motorSpeed = 0;
            motorRunning = false;
            motorState = 0;
            Serial.println("Motor decelerated and stopped.");
        }
        break;
    }

    if (motorDirection)
    {
        analogWrite(motorPinIN1, motorSpeed);
        analogWrite(motorPinIN2, 0);
    }
    else
    {
        analogWrite(motorPinIN1, 0);
        analogWrite(motorPinIN2, motorSpeed);
    }
}

void startTotalRunTime()
{
    totalRunTimeStart = millis(); // Start the Total Run Time counter
    totalRunTimeElapsed = 0;      // Reset elapsed time
    totalRunTimePausedAt = 0;     // Reset paused timestamp
    totalRunTimePaused = false;   // Reset pause flag
    Serial.println("Total Run Time started.");
}

void checkTotalRunTime()
{
    if (totalRunTime > 0 && !totalRunTimePaused)
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - totalRunTimeStart - totalRunTimeElapsed;
        remainingTime = (totalRunTime * 60 * 1000) - elapsedTime;

        if (/*remainingTime <= 0 &&*/ countDown <= 0)
        {
            stopMotor();
            totalRunTime = 0;
            totalRunTimePaused = false;
            motorAction = false;
            Serial.println("Total Run Time expired. Motor stopped.");
        }
        else
        {
            runtime();
            Serial.print("Total Run Time Remaining: ");
            // Serial.print(remainingTime / 1000); // Display in seconds
            Serial.print(countDown); // Display in seconds
            Serial.println(" seconds");
        }
    }
}

void pauseTotalRunTime()
{
    totalRunTimePausedAt = millis();
    totalRunTimeElapsed += totalRunTimePausedAt - totalRunTimeStart;
    totalRunTimePaused = true;
    motorRunning = false;
    motorAction = true; // paused
    timerIsRunning = false;
    Serial.println("Total Run Time paused.");
}

void resumeTotalRunTime()
{
    timerIsRunning = true;
    motorAction = false;
    totalRunTimeStart = totalRunTimePausedAt;
    totalRunTimePausedAt = 0;
    totalRunTimePaused = false;
    Serial.print("Total Run Time resumed. ");
}

void runtime()
{
    if (!totalRunTimePaused && countDown > 0)
    {
        if (millis() - timenow > 1000)
        {
            timenow = millis();
            countDown--;
        }
    }
}