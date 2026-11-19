
#include <SPI.h> //to communicate with one or more peripheral device
#include <Arduino.h>

//for stepper motor
#include<Stepper.h> //TO LOAD THE LIBRARY TO RUN driver
int stepsPerRevolution=2048 ; //depends on the AMOUNT OF STEPS MAKES A WHOLE REVOLUTION
int motSpeed= 15; //revolution per minute (rpm)  
Stepper myStepper(stepsPerRevolution, 12,27,14,26); //pin number depending on different driver (in1,in3,in2,in4)

//setting up millis function for 3 event 
const long eventTime1= 60000; //1 mins
const long eventTime2= 10000; //10 s
const long eventTime3= 79200000; //22h
unsigned long previousTime_1 = 0;
unsigned long previousTime_2 = 0;
unsigned long previousTime_3 = 0;
// for PWM signal
int PWM_pin = 4;  //Pin for PWM signal to the MOSFET driver 
const int freq = 5000;
const int PWMChannel = 0;
const int resolution = 8;

//Temperature, Humidity, CO2, and Wifi
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#define RXD2 16 //for CO2
#define TXD2 17 //for CO2
#define Type DHT11
int sensePin=32;
DHT HT(sensePin,Type);
int value1; //Humidity
int value2; //Temperature in celcius
float value3; //For the CO2 measurement,because its value is decimal
int setTime= 50;
int dt=10000;
const char * ssid = "YOUR_WIFI_NAME"; //ssid of wifi (replace with your own)
const char * password = "YOUR_WIFI_PASSWORD"; //wifi password (replace with your own)

String server = "http://maker.ifttt.com";
String eventName = "DHT11 temp & humidity sensor";
String IFTTT_Key = "YOUR_IFTTT_WEBHOOK_KEY";
String IFTTTUrl="https://maker.ifttt.com/trigger/temp_data/with/key/YOUR_IFTTT_WEBHOOK_KEY";


//Variables
float set_temperature = 28;            // Temperature setpoint. 
float temperature_read = 0.0;
float PID_error = 0;
float previous_error = 0;
float elapsedTime, Time, timePrev;
float PID_value = 0;
 

//PID constants
//////////////////////////////////////////////////////////
int kp = 90;   int ki = 60;   int kd = 80; //edit as necessary
//////////////////////////////////////////////////////////

int PID_p = 0;    int PID_i = 0;    int PID_d = 0; 
int PID_values_fixed =0;


void setup() {
    // configure PWM functionalitites for ESP32
  ledcSetup(PWMChannel, freq, resolution);
  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(PWM_pin, PWMChannel);
  Serial.begin(9600);
  myStepper.setSpeed(motSpeed); //to input how fast we want the stepper motor to move
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); //CO2 setup
  HT.begin(); //DHT11 setup
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Viola, Connected !!!");
  pinMode(PWM_pin,OUTPUT);
  Time = millis();
  //make the stepper motor run in the beginning, then after 22 hours, the millis function will work
  myStepper.step(-2.3*stepsPerRevolution); // the stepper will make the lead extrude to touch the touch screen (turn it off)
delay(500);
myStepper.step(2.3*stepsPerRevolution); // to retract the lead                                                                                                                                                                                                                                                                                                                                                                                                                        *stepsPerRevolution);// negative will make it go backwards      
}
void sendDataToSheet(void) //send data to google spread sheet
{
  String url = server + "/trigger/" + eventName + "/with/key/" + IFTTT_Key + "?value1=" + String((int)value1) + "&value2="+String((int)value2) +"&value3=" + String((int)value3);  
  Serial.println(url);
  //Start to send data to IFTTT
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  http.begin(url); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  
}

void loop() {
  unsigned long currentTime=millis();
  
  //Sending data to google sheet every 10 mins
  if ( currentTime - previousTime_1 >= eventTime1) {
     sendDataToSheet();
     previousTime_1= currentTime;
  }
  
  //reading of temperature, humidity, co2 to serial monitor and feedback every 10s
    if ( currentTime - previousTime_2 >= eventTime2) { 
     value1=HT.readHumidity(); //humidity
  value2=HT.readTemperature(); //temp in C
  value3=Serial2.parseFloat(); //carbon dioxidereading
   delay(1);
  Serial.print("Values are ");
  Serial.print(value1);
  Serial.print(' ');
  Serial.print(value2);
  Serial.print(' ');
  Serial.println(value3);
  Serial.print(' '); 
  //input feedback for CO2 here
    // First we read the real value of temperature
  temperature_read = value2; //read temp in celcius from DHT11
  //Next we calculate the error between the setpoint and the real value
  PID_error = set_temperature - temperature_read + 3;
  //Calculate the P value
  PID_p = 0.01*kp * PID_error;
  //Calculate the I value in a range on +-3
  PID_i = 0.01*PID_i + (ki * PID_error);
   //For derivative we need real time to calculate speed change rate
  timePrev = Time;                            // the previous time is stored before the actual time read
  Time = millis();                            // actual time read
  elapsedTime = (Time - timePrev) / 1000; 
  //Now we can calculate the D calue
  PID_d = 0.01*kd*((PID_error - previous_error)/elapsedTime);
  //Final total PID value is the sum of P + I + D
  PID_value = PID_p + PID_i + PID_d;

  //We define PWM range between 0 and 255
  if(PID_value < 0)
  {    PID_value = 0;    }
  if(PID_value > 255)  
  {    PID_value = 255;  }
  //Now we can write the PWM signal to the mosfet on digital pin D3
  //Since we use MOSFET directly, we write 0-PID valuey
ledcWrite(PWMChannel, PID_value);  //control the peltier heating 
Serial.println(PID_value);
  previous_error = PID_error;     //Remember to store the previous error for next loop.
  delay(250); 
     previousTime_2= currentTime;
  }
  
  //stepper motor function
    if ( currentTime - previousTime_3 >= eventTime3) { 
myStepper.step(-2.3*stepsPerRevolution); // the stepper will make the lead extrude to touch the touch screen (turn it off)
delay(500);
myStepper.step(2.3*stepsPerRevolution); // to retract the lead                                                                                                                                                                                                                                                                                                                                                                                                                        *stepsPerRevolution);// negative will make it go backwards
delay(7200000); //to turn off the oxygen concentrator for 2 hours
  myStepper.step(-2.3*stepsPerRevolution); // the stepper will make the lead extrude to touch the touch screen (turn it on)
delay(500);
myStepper.step(2.3*stepsPerRevolution); // to retract the lead                                                                                                                                                                                                                                                                                                                                                                                                                        *stepsPerRevolution);// negative will make it go backwards
  previousTime_3= currentTime;
  }  

  }
