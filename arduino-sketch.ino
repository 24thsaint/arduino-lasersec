#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

/*
   The program uses Arduino's built in Software Serial library
   to communicate with Sim800l to send AT commands.
*/
SoftwareSerial gsm(9, 10);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const int buzzer = 6;
const int laser = 13;
const int lightSensor = A0;

char serialInput;
String serialMessage;

float lightValue = 0;
char incomingByte;
String message = "";    // messages from Sim800l are stored in this variable
String owner = "";      // this variable holds the parsed phone number
String sender = "";
String response = "I am sorry, I did not understand your command. Reply HELP to know what I can do.";

/*
   Boolean values to determine module states.
*/
boolean isSenderAuthorized = false;
boolean isAlarmActive = false;
boolean isSecurityActive = false;
boolean isSecurityBreached = false;
boolean isNotificationArmed = false;
boolean isMessageReceived = false;

void setup() {
  pinMode(laser, OUTPUT);
  digitalWrite(laser, LOW);

  Serial.begin(9600);
  lcd.begin(16, 2);
  gsm.begin(9600);

  lcd.print("Initializing...");

  while (!gsm.available()) {
    gsm.print("AT\r");
    delay(1000);
    Serial.println("connecting....");
  }

  Serial.println("Connected..");
  gsm.println("AT+CMGF=1");           // Check if modem supports SMS-text mode
  delay(1000);
  gsm.println("AT+CNMI=1,2,0,0,0");   // When module receives new SMS, communicate it via serial
  delay(1000);
  gsm.println("AT+CMGL=\"REC UNREAD\""); // Read unread messages
  delay(1000);
  executeAlarm();
  Serial.println("Initialization completed");
  lcd.clear();
  lcd.print("Initialized!");
  lcd.setCursor(0, 1);
  lcd.print("System Standby...");
}

void loop() {

  if (Serial.available() > 0) {
    serialInput = Serial.read(); // read the incoming byte:
    serialMessage += serialInput;
    Serial.println(serialMessage);
    if (Serial.available() == 0) {
      gsm.println(serialMessage);
      delay(100);
      serialMessage = "";
    }
  }

  if (isSecurityActive) {
    securityWatcher();
  }

  if (isAlarmActive && isSecurityBreached && isSecurityActive) {
    executeAlarm();
    lcd.clear();
    lcd.print("Alarm Triggered!");
  }

  if (isSecurityBreached && isSecurityActive && isNotificationArmed) {
    executeNotification();
    isNotificationArmed = false;
    lcd.setCursor(0, 1);
    lcd.print("Notification sent!");
  }

  if (gsm.available()) {

    /*
       Concatenate serial messages per byte from the GSM module if a comm is initiated
    */
    while (gsm.available()) {
      incomingByte = gsm.read();
      message += incomingByte;
      delay(50);
    }

    delay(50);

    sender = parseSender(message);

    Serial.println(message);
    message.toUpperCase();

    if (message.indexOf("REGISTER") > -1) {
      response = "Registration Successful!";
      registerOwner();
    }

    if (isMessageReceived) {
      isSenderAuthorized = verifySender(sender);
    }

    if (message.indexOf("HELP") > -1 && isSenderAuthorized) {
      response = "These are the functions I understand: ALARM ON, ALARM OFF, SYSTEM START, SYSTEM STOP, SYSTEM REARM, HELP, REGISTER.";
    }
    if (message.indexOf("ALARM ON") > -1 && isSenderAuthorized) {
      response = "Alarm ACTIVATED!";
      activateAlarm();
    }
    if (message.indexOf("ALARM OFF") > -1 && isSenderAuthorized) {
      response = "Alarm DEACTIVATED!";
      deactivateAlarm();
    }
    if (message.indexOf("SYSTEM START") > -1 && isSenderAuthorized) {
      response = "Security System ACTIVATED!";
      activateSecurity();
    }
    if (message.indexOf("SYSTEM STOP") > -1 && isSenderAuthorized) {
      response = "Security System DEACTIVATED!";
      deactivateSecurity();
    }
    if (message.indexOf("SYSTEM REARM") > -1 && isSenderAuthorized) {
      response = "System Successfully Rearmed!";
      rearmSecurity();
    }
    if (message.indexOf("+CMT") > -1 && !isSenderAuthorized && owner != "") {
      response = "Sorry, you are not authorized to do that.";
    }

    if (isMessageReceived && message.indexOf("OK")) {
      sendMessage(parseSender(message));
      delay(50);
      /*
         Delete messages so that the newest one will be at index[0] and to save memory
      */
      gsm.print("AT+CMGDA=\"DEL ALL\"\r");
    }

    delay(50);
    message = "";
    response = "I am sorry, I did not understand your command. Reply HELP for a list of my functions.";
  }

  delay(50);
}

/*
   This method takes the output AT message from the Sim800l module
   when a message is received and parses the phone number
   of the sender to be used by other methods.
*/
String parseSender(String messageMeta) {
  isMessageReceived = (messageMeta.indexOf("+CMT") > -1); // does the serial message have an unread SMS?
  messageMeta.replace("+639", "09");                      // replace the Philippine +63 country code prefix for uniformity when parsing
  int index = messageMeta.indexOf("\"09");
  return messageMeta.substring(index + 1, index + 12);
}

/**
   These are series of commands that instructs the Sim800l module
   to prepare a message and send it when it reaches the 'carriage return' character
*/
void sendMessage(String recipient) {
  gsm.print(("AT+CMGS=\""));
  gsm.print(recipient);
  gsm.print(("\"\r"));
  delay(500);
  gsm.print(response);
  gsm.print(" ~LaserSec");
  gsm.print("\r");
  delay(500);
  gsm.print((char)26);    // carriage return
  delay(500);
}

/*
   This method verifies if the sender is the owner with
   additional validation such that it prepares a message to
   notify that there is no owner currently registered.

   Params:
   sender: String - the parsed number of the sender
*/
boolean verifySender(String sender) {
  if (owner == "") {
    response = "No authorized number yet, please reply REGISTER to be authorized.";
    return false;
  }
  return (sender == owner);
}

/*
   This method executes a 4000Hz continuous beep.
*/
void executeAlarm() {
  tone(buzzer, 4000);
  delay(50);
  noTone(buzzer);
  delay(50);
}

/*
   This method prepares a response that the security
   has been breached and sends it to the registered owner.
*/
void executeNotification() {
  if (owner != "") {
    response = "ALERT! Security breach detected.";
  }
  sendMessage(owner);
}

/*
   This method watches the LDR if the laser is interrupted with validation such that
   the functions will not execute again when the laser currently turned off.

   In the event that it is interrupted, it changes the isSecurityBreached boolean to true
   and turns off the laser.
*/
void securityWatcher() {
  lightValue = analogRead(lightSensor);
  if (lightValue < 900 && digitalRead(laser) == HIGH) {
    isSecurityBreached = true;
    digitalWrite(laser, LOW);
    Serial.println("SECURITY BREACHED");
  }
}

/*
   This method triggers the isAlarmActive boolean to true and executes a beep to
   affirm that the command has been acknowledged.
*/
void activateAlarm() {
  isAlarmActive = true;
  executeAlarm();
  Serial.println("Alarm Activated");
}

/*
   This method triggers the isAlarmActive boolean to false and executes a beep to
   affirm that the command has been acknowledged.
*/
void deactivateAlarm() {
  isAlarmActive = false;
  noTone(buzzer);
  executeAlarm();
  Serial.println("Alarm Deactivated");
}

/*
   This method activates the security and triggers a series of variables and calls methods
   that will virtually turn ON the modules.
*/
void activateSecurity() {
  isNotificationArmed = true;
  isSecurityActive = true;
  digitalWrite(laser, HIGH);
  activateAlarm();
  lcd.clear();
  lcd.print("System Armed!");
  lcd.setCursor(0, 1);
  lcd.print("Watching...");
  Serial.println("Security Activated");
}

/*
   This method deactivates the security and triggers a series of variables and calls methods
   that will virtually turn OFF the modules.
*/
void deactivateSecurity() {
  isSecurityActive = false;
  digitalWrite(laser, LOW);
  deactivateAlarm();
  lcd.clear();
  lcd.print("System Stopped!");
  lcd.setCursor(0, 1);
  lcd.print("Standby...");
  Serial.println("Security Deactivated");
}

/*
   This method rearms the security and triggers a series of variables and calls methods
   that will virtually reset the modules to its active states.
*/
void rearmSecurity() {
  isSecurityBreached = false;
  activateSecurity();
  Serial.println("Security Successfully Rearmed");
}

/*
   This method registers the phone number of the sender as owner
*/
void registerOwner() {
  owner = sender;
  lcd.clear();
  lcd.print("Owner Registered");
  lcd.setCursor(0, 1);
  lcd.print("Waiting Cmds...");
  delay(1000);
  Serial.println("Owner Registration Successful!");
}
