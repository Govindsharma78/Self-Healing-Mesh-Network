#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SEND_BTN   D4
#define BUZZER     1      // TX PIN
#define SELECT_PIN A0

Adafruit_SSD1306 display(128, 64, &Wire, -1);

int myID = 1;
int destination = 1;
int msgIndex = 0;
int msgID = 0;
int mode = 0; // 0:DIRECT, 1 MESH , 3 BRODCAST

bool ackReceived = true;
unsigned long lastSendTime = 0;
bool lastSelectState = false;
unsigned long pressTime = 0;
bool autoMode = false;
unsigned long lastChangeTime = 0;
int retryCount = 0;

void handleSelect();
void handleSendButton();
void sendPacket();
void processPacket(String pkt);
void beep();
void longBeep();

String messages[] = {
  "Lost in unknown area.",
  "Severe injury, need help.",
  "Building collapse nearby.",
  "Low battery, stuck here.",
  "Flood situation, rescue needed.",
  "Danger zone, can't escape.",
  "Contact authorities ASAP!"
};


// ================= SETUP =================
void setup() {
  pinMode(SEND_BTN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  Wire.begin(D2, D1);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);
  display.setTextSize(1);

  SPI.begin();
  LoRa.setPins(D8, D3, D0);

  if (!LoRa.begin(433E6)) {
    while(true);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  LoRa.receive();

  showScreen();
}

// ================= LOOP =================
void loop() {

  handleSelect();
  handleSendButton();

  // 🔥 AUTO CHANGE SYSTEM
  if(autoMode && millis() - lastChangeTime > 800){

    destination++;
    if(destination > 3) destination = 0;

    mode++;
    if(mode > 2) mode = 0;

    showScreen();

    lastChangeTime = millis();
  }

  // 📡 RECEIVE
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String pkt = "";
    while (LoRa.available()) pkt += (char)LoRa.read();
    processPacket(pkt);
  }

  // 🔁 RETRY SYSTEM
  if(!ackReceived && (millis() - lastSendTime > 6000)){

    if(retryCount < 2){
      retryCount++;

      display.clearDisplay();
      display.setCursor(0,0);
      display.println("RETRY...");
      display.display();

      beep();
      sendPacket();   // 🔥 resend
    }
    else{
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("FAILED!");
      display.println("NO ACK");
      display.display();

      longBeep();

      delay(2000);

      retryCount = 0;
      ackReceived = true;
      showScreen();
    }
  }

}
// ================= BUTTONS =================
void handleSelect() {
  int val = analogRead(SELECT_PIN);
  bool current = (val > 200); 
  if (current != lastSelectState) {
    if (current == true) {
      msgIndex++;
      if (msgIndex > 4) msgIndex = 0;
      beep(); // Chhota beep selection ke liye
      showScreen();
    }
    delay(150);
    lastSelectState = current;
  }
}

void handleSendButton(){

  static bool lastState = HIGH;
  bool btn = digitalRead(SEND_BTN);

  // press detect
  if(lastState == HIGH && btn == LOW){

    if(!autoMode){
      // 🔴 FIRST PRESS → start auto change
      autoMode = true;
      lastChangeTime = millis();
    }
    else{
      // 🔴 SECOND PRESS → stop + send
      autoMode = false;
      sendPacket();
    }

    delay(200); // debounce
  }

  lastState = btn;
}
// ================= SEND/RECEIVE =================
void sendPacket(){
  msgID++;
  char modeChar = (mode == 0) ? 'D' : (mode == 1 ? 'M' : 'B');
  String pkt = String(myID) + "," + String(destination) + "," + String(modeChar) + "," + String(msgID) + "," + messages[msgIndex];

  display.clearDisplay();
  display.println("SENDING...");
  display.display();

  LoRa.beginPacket();
  LoRa.print(pkt);
  LoRa.endPacket(true);
  LoRa.receive();
  ackReceived = false;
  lastSendTime = millis();
}

void processPacket(String pkt) {
  // ACK logic (5 sec delay)
  if(pkt.startsWith("ACK")){
    ackReceived = true;
    retryCount = 0;
    display.clearDisplay();
    display.println("ACK SUCCESSful");
    display.println("RECEIVED!");
    display.display();
    beep(); // Chhota beep confirmation ke liye
    delay(5000); 
    showScreen();
    return;
  }

  int p1=pkt.indexOf(',');
  int p2=pkt.indexOf(',',p1+1);
  int p3=pkt.indexOf(',',p2+1);
  int p4=pkt.indexOf(',',p3+1);

  int sender = pkt.substring(0,p1).toInt();
  int dest   = pkt.substring(p1+1,p2).toInt();
  String msg = pkt.substring(p4+1);

  // Message Receive (10 sec delay)
  if(dest == myID || dest == 0){
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("NODE "); display.println(sender);
    display.print("MSG:");
    for(int i=0; i<msg.length(); i+=16) display.println(msg.substring(i, i+16));
    display.print("DEST:"); display.println(dest);
    display.print("MODE:"); 
    if(pkt.substring(p2+1,p3)[0] == 'D') display.println("DIRECT"); else display.println("MESH");
    display.display();

    longBeep(); // 🔥 EMERGENCY ALERT: 1 second lamba buzzer

    delay(100);
    LoRa.beginPacket();
    LoRa.print("ACK," + String(sender) + "," + String(myID));
    LoRa.endPacket(true);
    
    LoRa.receive();
    delay(3000); 
    showScreen();
  }
}

// ================= DISPLAY =================
void showScreen() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("NODE "); display.println(myID);
  display.print("MSG:");
  String msg = messages[msgIndex];
  for(int i=0; i<msg.length(); i+=16) display.println(msg.substring(i, i+16));
  display.print("DEST:"); display.println(destination);
  display.print("MODE:");
  if (mode == 0) display.println("DIRECT");
  else if (mode == 1) display.println("MESH");
  else display.println("BROADCAST");
  display.display();
}

// ================= BUZZER FUNCTIONS =================
void beep() {
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
}

void longBeep() {
  digitalWrite(BUZZER, HIGH);
  delay(1000); // 🔥 1 Second lamba beep
  digitalWrite(BUZZER, LOW);
}