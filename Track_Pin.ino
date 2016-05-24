

//**Track Pin Oil Volume Test**//
 //**** Date: 04 May 2015 ****//
 //**** Author: J. Hogue ****//

/*Test will be started from pushbutton
 Step 1) Open Supply Solenoid
 Step 2) Time Delay to reach test pressure on Pressure sensor on A0
 Step 3) Close Supply Solenoid
 Step 4) Read before pressure
 Step 5) Time Delay
 Step 6) Open Test Solenoid
 Step 7) Read immediate after pressure
 Step 8) Log Before and After pressure to SD Card with time/date stamp
 Step 9) Compare Pressure drop to Lookup Table for actual oil volume
 Step 10) Pass or Fail
 Step 11) Reset from 2nd button */
 

#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

// HTTP Page Setup Text //
const char string_0[] PROGMEM = "HTTP/1.1 200 OK";
const char string_1[] PROGMEM = "Content-Type: text/html";
const char string_2[] PROGMEM = "<HTML><HEAD></HEAD>";
const char string_3[] PROGMEM = "<BODY>";
const char string_4[] PROGMEM = "<form method='post' enctype='multipart/form-data'>";
const char string_5[] PROGMEM = "<input type='file' name='data'><input type='submit'></form>";
const char string_6[] PROGMEM = "</BODY></HTML>";


const char* const httpText[] PROGMEM = {string_0, string_1, string_2, string_3, string_4, string_5, string_6};

#define packetBufSize 128

char buf[packetBufSize+1];

Sd2Card card;
SdVolume volume;
SdFile root;
SdFile logfile;

// store error strings in flash to save RAM
#define error(s) error_P(PSTR(s))

void error_P(const char* str) {
  PgmPrint("error: ");
  SerialPrintln_P(str);
  if (card.errorCode()) {
    PgmPrint("SD error: ");
    Serial.print(card.errorCode(), HEX);
    Serial.print(',');
    Serial.println(card.errorData(), HEX);
  }
  while(1);
}

// SD Card & Ethernet chip setup //
const int SDchipSelect = 4;
const int EthchipSelect = 10;

// Ethernet Configuration //
EthernetServer server = EthernetServer(80);
static uint8_t mac[] = { 
  0x90, 0xA2, 0xDA, 0x00, 0x22, 0x2F };
static uint8_t ip[] = { 
  192, 168, 0, 15};
static uint8_t gateway[] = { 
  192, 168, 0, 1};

int byteCount = 0;
int saveCount = 0;

String received="";
String boundary="";

// Channel Setup //
const int StartbuttonPin = 2;  // Start test button digital input pin
const int SOL1Pin = 3;  // Test Supply Solenoid Relay & On/Off digital output Pin
const int SOL2Pin = 5; // Test Solenoid Relay & On/Off digital output Pin
const int Delay1 = 2000; //Time Delay 1 5 seconds
const int PressurePin = 0; // Pressure Sensor Analog input Pin
const int TestFinishedPin = 6; // Test Complete digital input Pin
const int Delay2 = 1000; // 1s delay
const int TestModePin = 1;  // Test Mode Analog Input Pin from Toggle Switch
int Mode = 0; // Test Mode Selected

// Test Variables //
float UpperLimit = 30;
float LowerLimit = 29;
float Slope = 20; // Slope for Pressure Scaling
float Intercept = 0; // Intercept for Pressure Scaling
float PressurePSI = 0;
int TestCounter = 0; // Counter for Test Start/Stop button presses
int StartbuttonState = 0; // State for Start Button
int lastStartbuttonState = 0; // Last Start button State

void setup(void) 
{
  Serial.begin(115200); // initialize serial communication
  
  pinMode(StartbuttonPin, INPUT);
  pinMode(SOL1Pin, OUTPUT);
  pinMode(SOL2Pin, OUTPUT);
  pinMode(SDchipSelect, OUTPUT);
  pinMode(EthchipSelect, OUTPUT);
  
  useSD();
  
  PgmPrint("Free RAM: ");
  Serial.println(FreeRam());
  
    // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!card.init(SPI_FULL_SPEED, 8)) error("card.init failed");

  // initialize a FAT volume
  if (!volume.init(&card)) error("volume.init failed");

  // open the root directory
  if (!root.openRoot(&volume)) error("openRoot failed");
  
  logfile.remove(&root, "playlog.txt");

  
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  //pinMode(10, OUTPUT);  //but turn off the w5100 chip (ethernet) (UNO)
  //digitalWrite(10, HIGH); //but turn off the w5100 chip (ethernet) (UNO)
  pinMode(53, OUTPUT);  //but turn off the w5100 chip (ethernet) (MEGA)
  digitalWrite(53, HIGH); //but turn off the w5100 chip (ethernet) (MEGA)

  // see if the card is present and can be initialized:
  if (!SD.begin(SDchipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

  useEthernet();
  Ethernet.begin(mac, ip, gateway);
  server.begin();
}

void loop(void) 
{  
     useEthernet();
  // listen for incoming clients
  Client client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank && !httpPostDetected) {
          sendHtml(client, server);
          break;
        }
        else if (c == '\n' && httpPostDetected && !httpPostStart) {
          
          if (boundary == "") {
            PgmPrint("TEST1\n");
            Serial.print(received);
            int search = received.indexOf("boundary=");
            if (search!=-1)
            {
              boundary = received.substring(search+9);
              PgmPrint("BOUNDARY READ\n");
            }
          }
          else {
            int search = received.indexOf(boundary);
            if (search!=-1)
            {
              PgmPrint("BOUNDARY DETECTED\n");
              PgmPrint("POST DATA START\n");
              PgmPrint("AVAILABLE BYTES: ");
              Serial.println(client.available());
              delay(1);
              useSD();
              if (!logfile.open(&root, "playlog.txt", O_WRONLY | O_APPEND | O_CREAT)) 
              error("can't open log file");
              useEthernet();
              httpPostStart = true;
            }
          }
           
          received = "";
          byteCount = 0;
        }
        else if (c == '\n' && !httpPostDetected) {
          PgmPrint("NEW LINE\n");
          // you're starting a new line
          currentLineIsBlank = true;
          int search = received.indexOf("POST");
          if (search!=-1)
          {
            httpPostDetected = true;
            PgmPrint("POST DETECTED\n");
          }
          received = "";
        } 
  
  analogRead(TestModePin); //Read Test Mode Pin
 
// Set Mode to Run

 if (TestModePin == 0)
   {
   TestMode(); // Normal test Sequence, Log data to file, show results on LCD
   }
 else if (TestModePin == 1)
   {
   CalMode(); // Calibration Sequence - Sequence Runs the same as Test Mode, no data logging, Show Results on LCD
   }
 else if (TestModePin == 2)
   {
   LeakMode(); // Leak test - Sequence Runs the same as Test Mode, no data logging, Only showing Pressure Drop on LCD
   } 
}


void TestMode (){
  
  float StartPressure = 0, EndPressure = 0, PressureDrop = 0;
  float voltage;
  voltage = analogRead(PressurePin) * .004882814;
  PressurePSI = (voltage * Slope) + Intercept;
  String dataString = ""; // make a string for assembling the data to log:
    
  StartbuttonState = digitalRead(StartbuttonPin); // Read Start Pushbutton Pin
    
    if (StartbuttonState != lastStartbuttonState) // compare the buttonState to its previous state 
     {
      if (StartbuttonState == HIGH) // if the state has changed, increment the counter 
      {
       TestCounter++; // if the current state is HIGH then the button went from off to on
       Serial.println("on");
       Serial.print("number of button pushes:  ");
       Serial.println(TestCounter);
      }
      else 
      {
       Serial.println("off"); // if the current state is LOW then the button went from on to off
      }
       delay(50); // Delay a little bit to avoid bouncing
     }
    
   if (TestCounter == 2)
   { 
     digitalWrite(SOL1Pin, HIGH); // Turn on Supply Solenoid & LED
     delay(Delay1); // Allow time for Supply Pressure to Build
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     StartPressure = PressurePSI; // Record Starting Pressure
     Serial.print("Starting Pressure: ");
     Serial.print(StartPressure);
     Serial.println(" psi");
     digitalWrite(SOL1Pin, LOW); // Turn off Supply Solenoid & LED
     digitalWrite(SOL2Pin, HIGH); // Turn on Test Circuit Solenoid & LED
     delay(Delay2);
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     EndPressure = PressurePSI; // Record End Pressure
     Serial.print("Ending Pressure: ");
     Serial.print(EndPressure);
     Serial.println(" psi");
     PressureDrop = (StartPressure - EndPressure); // Calculate Pressure Drop
     //delay(Delay1); // Allow time for Supply Pressure to Build
     Serial.print("Pressure Loss: ");
     Serial.print(PressureDrop);
     Serial.println(" psi");
     digitalWrite(SOL2Pin, LOW); // Turn off Test Circuit Solenoid & LED
     // Lookup Table Code Here
    
  File dataFile = SD.open("datalog2.csv", FILE_WRITE);  // open the file. note that only one file can be open at a time

  // if the file is available, write to it:
  if (dataFile) {
    //dataFile.print("Start Pressure: ");
    dataFile.println("Start Pressure, End Pressure, Pressure Drop");
    dataFile.print(StartPressure);
    dataFile.print(", ");
    //dataFile.print("End Pressure: ");
    dataFile.print(EndPressure);
    dataFile.print(", ");
    //dataFile.print("Pressure Drop: ");
    dataFile.println(PressureDrop);
    dataFile.close();
    // print to the serial port too:
    Serial.println("Start Pressure, End Pressure, Pressure Drop");
    Serial.print(StartPressure);
    Serial.print(", ");
    //Serial.print("End Pressure: ");
    Serial.print(EndPressure);
    Serial.print(", ");
    //Serial.print("Pressure Drop: ");
    Serial.println(PressureDrop);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog2.csv");
  }
  
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    digitalWrite(TestFinishedPin, HIGH); // Turn On Test Complete LED
    TestCounter = 0;
    Serial.println("Test Complete");
  }
  else
  {
    digitalWrite(SOL1Pin, LOW); // Turn Off Supply Solenoid & LED
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    analogRead(PressurePin);
    Serial.print("Current Pressure: ");
    Serial.print(PressurePSI);
    Serial.println(" psi");
    delay(10);
    
  }
 lastStartbuttonState = StartbuttonState; // save the current state as the last state, for next time through the loop
}

void CalMode (){
  
  float StartPressure = 0, EndPressure = 0, PressureDrop = 0;
  float voltage;
  voltage = analogRead(PressurePin) * .004882814;
  PressurePSI = (voltage * Slope) + Intercept;
  String dataString = ""; // make a string for assembling the data to log:
    
  StartbuttonState = digitalRead(StartbuttonPin); // Read Start Pushbutton Pin
    
    if (StartbuttonState != lastStartbuttonState) // compare the buttonState to its previous state 
     {
      if (StartbuttonState == HIGH) // if the state has changed, increment the counter 
      {
       TestCounter++; // if the current state is HIGH then the button went from off to on
       Serial.println("on");
       Serial.print("number of button pushes:  ");
       Serial.println(TestCounter);
      }
      else 
      {
       Serial.println("off"); // if the current state is LOW then the button went from on to off
      }
       delay(50); // Delay a little bit to avoid bouncing
     }
    
   if (TestCounter == 2)
   { 
     digitalWrite(SOL1Pin, HIGH); // Turn on Supply Solenoid & LED
     delay(Delay1); // Allow time for Supply Pressure to Build
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     StartPressure = PressurePSI; // Record Starting Pressure
     Serial.print("Starting Pressure: ");
     Serial.print(StartPressure);
     Serial.println(" psi");
     digitalWrite(SOL1Pin, LOW); // Turn off Supply Solenoid & LED
     digitalWrite(SOL2Pin, HIGH); // Turn on Test Circuit Solenoid & LED
     delay(Delay2);
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     EndPressure = PressurePSI; // Record End Pressure
     Serial.print("Ending Pressure: ");
     Serial.print(EndPressure);
     Serial.println(" psi");
     PressureDrop = (StartPressure - EndPressure); // Calculate Pressure Drop
     //delay(Delay1); // Allow time for Supply Pressure to Build
     Serial.print("Pressure Loss: ");
     Serial.print(PressureDrop);
     Serial.println(" psi");
     digitalWrite(SOL2Pin, LOW); // Turn off Test Circuit Solenoid & LED
     // Lookup Table Code Here
    
  {
    // print to the serial port too:
    Serial.println("Start Pressure, End Pressure, Pressure Drop");
    Serial.print(StartPressure);
    Serial.print(", ");
    //Serial.print("End Pressure: ");
    Serial.print(EndPressure);
    Serial.print(", ");
    //Serial.print("Pressure Drop: ");
    Serial.println(PressureDrop);
  }
  
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    digitalWrite(TestFinishedPin, HIGH); // Turn On Test Complete LED
    TestCounter = 0;
    Serial.println("Test Complete");
  }
  else
  {
    digitalWrite(SOL1Pin, LOW); // Turn Off Supply Solenoid & LED
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    analogRead(PressurePin);
    Serial.print("Current Pressure: ");
    Serial.print(PressurePSI);
    Serial.println(" psi");
    delay(10);
    
  }
 lastStartbuttonState = StartbuttonState; // save the current state as the last state, for next time through the loop
}

void LeakMode (){
  
  float StartPressure = 0, EndPressure = 0, PressureDrop = 0;
  float voltage;
  voltage = analogRead(PressurePin) * .004882814;
  PressurePSI = (voltage * Slope) + Intercept;
  String dataString = ""; // make a string for assembling the data to log:
    
  StartbuttonState = digitalRead(StartbuttonPin); // Read Start Pushbutton Pin
    
    if (StartbuttonState != lastStartbuttonState) // compare the buttonState to its previous state 
     {
      if (StartbuttonState == HIGH) // if the state has changed, increment the counter 
      {
       TestCounter++; // if the current state is HIGH then the button went from off to on
       Serial.println("on");
       Serial.print("number of button pushes:  ");
       Serial.println(TestCounter);
      }
      else 
      {
       Serial.println("off"); // if the current state is LOW then the button went from on to off
      }
       delay(50); // Delay a little bit to avoid bouncing
     }
    
   if (TestCounter == 2)
   { 
     digitalWrite(SOL1Pin, HIGH); // Turn on Supply Solenoid & LED
     delay(Delay1); // Allow time for Supply Pressure to Build
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     StartPressure = PressurePSI; // Record Starting Pressure
     Serial.print("Starting Pressure: ");
     Serial.print(StartPressure);
     Serial.println(" psi");
     digitalWrite(SOL1Pin, LOW); // Turn off Supply Solenoid & LED
     digitalWrite(SOL2Pin, HIGH); // Turn on Test Circuit Solenoid & LED
     delay(Delay2);
     voltage = analogRead(PressurePin) * .004882814;// This equation converts the 0 to 1023 value that analogRead() returns,
     PressurePSI = (voltage * Slope) + Intercept; // into a 0.0 to 5.0 value that is the true voltage being read at that pin.
     EndPressure = PressurePSI; // Record End Pressure
     Serial.print("Ending Pressure: ");
     Serial.print(EndPressure);
     Serial.println(" psi");
     PressureDrop = (StartPressure - EndPressure); // Calculate Pressure Drop
     //delay(Delay1); // Allow time for Supply Pressure to Build
     Serial.print("Pressure Loss: ");
     Serial.print(PressureDrop);
     Serial.println(" psi");
     digitalWrite(SOL2Pin, LOW); // Turn off Test Circuit Solenoid & LED
     // Lookup Table Code Here
    
  {
    // print to the serial port too:
    Serial.println("Start Pressure, End Pressure, Pressure Drop");
    Serial.print(StartPressure);
    Serial.print(", ");
    //Serial.print("End Pressure: ");
    Serial.print(EndPressure);
    Serial.print(", ");
    //Serial.print("Pressure Drop: ");
    Serial.println(PressureDrop);
  }
   
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    digitalWrite(TestFinishedPin, HIGH); // Turn On Test Complete LED
    TestCounter = 0;
    Serial.println("Test Complete");
  }
  else
  {
    digitalWrite(SOL1Pin, LOW); // Turn Off Supply Solenoid & LED
    digitalWrite(SOL2Pin, LOW); // Turn Off Test Circuit Solenoid & LED
    analogRead(PressurePin);
    Serial.print("Current Pressure: ");
    Serial.print(PressurePSI);
    Serial.println(" psi");
    delay(10);
    
  }
 lastStartbuttonState = StartbuttonState; // save the current state as the last state, for next time through the loop
}

void useEthernet()
{
  digitalWrite(EthchipSelect, LOW);
  digitalWrite(SDchipSelect, HIGH);
}

void useSD()
{
  digitalWrite(SDchipSelect, LOW);
  digitalWrite(EthchipSelect, HIGH);
}

void sendHtml(Client client, Server server)
{
          
          PgmPrint("SEND SERVER HTML");

          char buffer[65];    // make sure this is large enough for the largest string it must hold
          // send a standard http response header
          for (int i = 0; i < 2; i++)
          {
            strcpy_P(buffer, (char*)pgm_read_word(&(httpText[i]))); // Necessary casts and dereferencing, just copy. 
            client.println( buffer );
          }
          client.println();
          for (int i = 2; i < 7; i++)
          {
            strcpy_P(buffer, (char*)pgm_read_word(&(httpText[i]))); // Necessary casts and dereferencing, just copy. 
            server.print( buffer );
          }
}

void save(char* data)
{
  ++saveCount;
              
  useSD();

  logfile.write(data);
    
  delay(500);   

  if (saveCount==5) {
    logfile.sync();
    saveCount=0;
  }

  useEthernet();
}
