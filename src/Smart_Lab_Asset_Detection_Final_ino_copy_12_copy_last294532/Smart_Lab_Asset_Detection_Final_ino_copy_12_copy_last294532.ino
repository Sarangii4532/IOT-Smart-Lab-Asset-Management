#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <sys/time.h>

#define WIFI_SSID "Aashraya 1"
#define WIFI_PASSWORD "9880044160"

#define API_KEY "AIzaSyD0And3LVnRvFNtWHgw2sdsx9Vu0pR5Fik"
#define DATABASE_URL "https://smart-lab-asset-detection-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define USER_EMAIL "esp32@smartlab.com"
#define USER_PASSWORD "SmartLab123"


// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// RFID pins
#define SS_PIN 5
#define RST_PIN 22

MFRC522 mfrc522(SS_PIN, RST_PIN);


// LED and buzzer pins
#define GREEN_LED 26
#define RED_LED 27
#define BUZZER_PIN 25

// ==========================
// Asset Management Variables
// ==========================

// Asset Status
bool workstationInUse = false;
bool solderingInUse = false;

// Usage Counter
int workstationUsageCount = 0;
int solderingUsageCount = 0;
int scanCount = 0;
// Check-Out Times
String workstationCheckOut = "";
String solderingCheckOut = "";

// Check-In Times
String workstationCheckIn = "";
String solderingCheckIn = "";

// Usage Duration
String workstationUsageDuration = "";
String solderingUsageDuration = "";

// Store start time for duration calculation
unsigned long workstationStartMillis = 0;
unsigned long solderingStartMillis = 0;

// Authorized RFID UIDs
byte whiteCard[] = {0x90, 0x94, 0x1D, 0x56};
byte blueTag[]   = {0x87, 0x92, 0x27, 0x64};


// Compare UID function
bool compareUID(byte *uid1, byte *uid2, byte size) {

  for(byte i = 0; i < size; i++) {

    if(uid1[i] != uid2[i])
      return false;

  }

  return true;
}


void setup() {

  Serial.begin(115200);


  // RFID start
  SPI.begin(18,19,23,5);
  mfrc522.PCD_Init();


  // LED setup
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);


  // Buzzer setup
  ledcAttach(BUZZER_PIN,8000,8);
  ledcWrite(BUZZER_PIN,0);



  // WiFi
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);

  Serial.print("Connecting WiFi");

  while(WiFi.status()!=WL_CONNECTED){

    delay(500);
    Serial.print(".");

  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());



  // Firebase setup

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;


  Firebase.begin(&config,&auth);
  Firebase.reconnectWiFi(true);

  configTime(19800, 0, "pool.ntp.org");

  Serial.println("Getting Time...");

  struct tm timeinfo;

  int retry = 0;

while (!getLocalTime(&timeinfo) && retry < 10) {
    Serial.print(".");
    delay(1000);
    retry++;
}

if (retry >= 10) {
    Serial.println("\nTime Sync Failed");
} else {
    Serial.println("\nTime Ready");
}


  Serial.println();
  Serial.println("Time Ready");

  Serial.println("Firebase Ready");
  Serial.println("Scan RFID Card");
  
  }



void loop() {


  if(!mfrc522.PICC_IsNewCardPresent()) 
     return;

  if(!mfrc522.PICC_ReadCardSerial())
    return;



  String uidString="";


  Serial.print("UID: ");


  for(byte i=0;i<mfrc522.uid.size;i++){

    if(mfrc522.uid.uidByte[i]<0x10)
      Serial.print("0");

    Serial.print(mfrc522.uid.uidByte[i],HEX);


    uidString += String(mfrc522.uid.uidByte[i],HEX);

  }


  Serial.println();



  String status;
  String assetName = "";
  struct tm timeinfo;

  getLocalTime(&timeinfo);

  char dateBuffer[20];
  char timeBuffer[20];

  strftime(dateBuffer, sizeof(dateBuffer), "%d-%m-%Y", &timeinfo);
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);

  String currentDate = String(dateBuffer);
  String currentTime = String(timeBuffer);

  



  // White card
// White Card - Unauthorized RFID

if(compareUID(mfrc522.uid.uidByte, whiteCard, 4))
{
    assetName = "None";
    status = "Unauthorized RFID";

    Serial.println("=================================");
    Serial.println("UNAUTHORIZED RFID DETECTED");
    Serial.println("Access Denied");
    Serial.print("Date : ");
    Serial.println(currentDate);
    Serial.print("Time : ");
    Serial.println(currentTime);
    Serial.println("=================================");

    digitalWrite(RED_LED, HIGH);

    ledcWrite(BUZZER_PIN, 128);

    delay(2000);

    ledcWrite(BUZZER_PIN, 0);

    digitalWrite(RED_LED, LOW);
}
  
  // Blue tag

  // ===============================
// SOLDERING STATION
// ===============================

 else if(compareUID(mfrc522.uid.uidByte, blueTag, 4))
{
    assetName = "Soldering Station";

    if(!solderingInUse)
    {
        solderingInUse = true;

        solderingUsageCount++;

        solderingStartMillis = millis();

        solderingCheckOut = currentTime;

        status = "Checked Out";

        Serial.println("=================================");
        Serial.println("SOLDERING STATION");
        Serial.println("Action : CHECKED OUT");
        Serial.print("Check-Out Time : ");
        Serial.println(solderingCheckOut);
        Serial.print("Usage Count : ");
        Serial.println(solderingUsageCount);
        Serial.println("=================================");
    }
    else
    {
        solderingInUse = false;

        solderingCheckIn = currentTime;

        unsigned long duration = (millis() - solderingStartMillis) / 1000;

        solderingUsageDuration = String(duration) + " Seconds";

        status = "Returned";

        Serial.println("=================================");
        Serial.println("SOLDERING STATION");
        Serial.println("Action : RETURNED");
        Serial.print("Check-In Time : ");
        Serial.println(solderingCheckIn);
        Serial.print("Usage Duration : ");
        Serial.println(solderingUsageDuration);
        Serial.print("Usage Count : ");
        Serial.println(solderingUsageCount);
        Serial.println("=================================");
    }

    digitalWrite(GREEN_LED, HIGH);
    delay(1500);
    digitalWrite(GREEN_LED, LOW);
}


  // Unauthorized

  else{

    assetName = "Unknown Asset";
    status="Unauthorized RFID";


    Serial.println(status);


    digitalWrite(RED_LED,HIGH);


    ledcWrite(BUZZER_PIN,128);


    delay(2000);


    ledcWrite(BUZZER_PIN,0);


    digitalWrite(RED_LED,LOW);


  }


     
   // Send data to Firebase

  if(Firebase.ready()){

  static int scanCount = 0;
  scanCount++;

  String path = "/LabAssets/Scans/Scan_" + 
  String(scanCount);


  // Save Scan History

  if(Firebase.RTDB.setString(&fbdo, (path + "/UID").c_str(), uidString))
    Serial.println("UID Saved");

  if(Firebase.RTDB.setString(&fbdo, (path + "/Asset_Name").c_str(), assetName))
    Serial.println("Asset Saved");

  if(Firebase.RTDB.setString(&fbdo, (path + "/Status").c_str(), status))
    Serial.println("Status Saved");

  if(Firebase.RTDB.setString(&fbdo, (path + "/Date").c_str(), currentDate))
    Serial.println("Date Saved");

  if(Firebase.RTDB.setString(&fbdo, (path + "/Time").c_str(), currentTime))
    Serial.println("Time Saved");


  // Update Current Asset Status

  if(assetName == "Soldering Station")
  {
    String currentStatus;

    if(status == "Checked Out")
    {
      currentStatus = "In Use";
    }
    else if(status == "Returned")
    {
      currentStatus = "Available";
    }
    else
    {
      currentStatus = status;
    }


    Firebase.RTDB.setString(
      &fbdo,
      "/LabAssets/Current_Status/Soldering_Station/Status",
      currentStatus
    );


    Firebase.RTDB.setString(
      &fbdo,
      "/LabAssets/Current_Status/Soldering_Station/Last_Update",
      currentTime
    );


    Serial.println("Current Status Updated");
  }


  Serial.println("Firebase Updated");

}
   
// Send data to Firebase

if(Firebase.ready()){

   static int scanCount = 0;
   scanCount++;
   String path = "/LabAssets/Scans/Scan_" + String(scanCount);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(500);

}
   String path = "/LabAssets/Scans/Scan_" + String(scanCount);

   Firebase.RTDB.setString(&fbdo, (path + "/UID").c_str(), uidString);
   Firebase.RTDB.setString(&fbdo, (path + "/Asset_Name").c_str(), assetName);
   Firebase.RTDB.setString(&fbdo, (path + "/Status").c_str(), status);
   Firebase.RTDB.setString(&fbdo, (path + "/Date").c_str(), currentDate);
   Firebase.RTDB.setString(&fbdo, (path + "/Time").c_str(), currentTime);

   Serial.println("Firebase Updated");
}
  

   



  
