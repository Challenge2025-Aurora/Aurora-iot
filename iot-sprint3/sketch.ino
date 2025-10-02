
#include <WiFi.h>
#include <HTTPClient.h>

#define TRIG_PIN   19   
#define ECHO_PIN   18  
#define BTN_INC    14
#define BTN_AREA   27
#define BTN_CLEAR  26
#define BUZZER_PIN 23
#define LED_LOC    21
#define LED_OCC    22
#define LED_MIS     5
#define LED_UNK    15
#define LED_LOW     2
#define POT_PIN    34

/*** REDE / THINGSPEAK ***/
const char* ssid = "Wokwi-GUEST";
const char* pass = "";           

const char* WRITE_API_KEY = "FJYJ8BUXL9XHBA2F";       
const unsigned long TS_INTERVAL_MS = 16000;            
const char* TS_URL = "http://api.thingspeak.com/update";

/*** LÓGICA ***/
const unsigned long TELEMETRY_MS = 1000;
const unsigned long READ_DIST_MS = 200;
const int  OCCUPIED_CM    = 20;
const int  LOW_BATT_PCT   = 25;
const unsigned long DEBOUNCE_MS  = 40;

enum OutputMode { MODE_JSON = 0, MODE_TABELA = 1, MODE_AMBOS = 2 };
OutputMode modoSaida = MODE_TABELA;

bool areaA = true;
int  slotNumber = 0;
String slotExpected = "A-12";

bool beepActive = false;
unsigned long beepEnd = 0;
unsigned long lastTel = 0;
unsigned long lastDistRead = 0;
long lastDistance = 999;
unsigned long lineCount = 0;
unsigned long lastTsSend = 0;

/*** Botões (com debounce) ***/
struct Btn { uint8_t pin; bool lastRaw; bool stable; unsigned long lastChange; };
Btn bInc{BTN_INC, true, true, 0};
Btn bArea{BTN_AREA, true, true, 0};
Btn bClear{BTN_CLEAR, true, true, 0};

void setupBtn(Btn &b){ pinMode(b.pin, INPUT_PULLUP); b.lastRaw=digitalRead(b.pin); b.stable=b.lastRaw; b.lastChange=millis(); }
bool updateBtn(Btn &b){
  bool raw=digitalRead(b.pin);
  unsigned long now=millis();
  if(raw!=b.lastRaw){ b.lastChange=now; b.lastRaw=raw; }
  if((now-b.lastChange)>DEBOUNCE_MS && b.stable!=raw){ b.stable=raw; return (b.stable==LOW); }
  return false;
}

/*** HC-SR04 ***/
long readDistanceCM(){
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if(dur==0) return 999;
  return dur/58;
}

/*** Bateria (pot) ***/
int readBatteryPct(){
  int raw = analogRead(POT_PIN); // ESP32: 0..4095
  int pct = map(raw, 0, 4095, 0, 100);
  pct = constrain(pct, 0, 100);
  return pct;
}

/*** Slot atual ***/
String slotAtual(){
  if(slotNumber<=0) return "-";
  char buf[8];
  char area = areaA ? 'A' : 'B';
  snprintf(buf, sizeof(buf), "%c-%02d", area, slotNumber);
  return String(buf);
}

/*** Saídas (Serial) ***/
void printJSON(bool presenca, int bateria, bool misplaced, bool unknownOcc, bool lowBatt){
  Serial.print("{\"dist_cm\":"); Serial.print(lastDistance);
  Serial.print(",\"presenca\":"); Serial.print(presenca?1:0);
  Serial.print(",\"slot\":\""); Serial.print(slotAtual()); Serial.print("\"");
  Serial.print(",\"bateria_pct\":"); Serial.print(bateria);
  Serial.print(",\"esperado\":\""); Serial.print(slotExpected); Serial.print("\"");
  Serial.print(",\"fora_do_lugar\":"); Serial.print(misplaced?1:0);
  Serial.print(",\"ocupacao_desconhecida\":"); Serial.print(unknownOcc?1:0);
  Serial.print(",\"ts\":"); Serial.print(millis());
  Serial.println("}");
}

void printHeader(){
  Serial.println(F("tempo(s) | pres dist(cm) | slot  esp   | batt | alertas"));
  Serial.println(F("---------+---------------+-------------+------+------------------------------"));
}

void barraBateria(char* out,int outSize,int pct){
  int filled=(pct+5)/10; filled = constrain(filled,0,10);
  out[0]='['; for(int i=0;i<10;i++) out[1+i]=(i<filled)?'#':'.'; out[11]=']'; out[12]=0;
}

void printTabela(bool presenca,long dist,int batt,bool misplaced,bool unknownOcc,bool lowBatt){
  if(lineCount%16==0) printHeader();
  char bar[13]; barraBateria(bar,sizeof(bar),batt);
  float secs=millis()/1000.0f;
  String s=slotAtual();
  String alerts=""; if(misplaced) alerts+="fora_do_lugar "; if(unknownOcc) alerts+="ocupacao_desconhecida "; if(lowBatt) alerts+="bateria_baixa "; if(alerts.length()==0) alerts="-";
  char buf[32]; dtostrf(secs,7,1,buf);
  Serial.print(buf); Serial.print(" | ");
  Serial.print(presenca?"SIM ":" NAO "); Serial.print(" ");
  if(dist>999) Serial.print("---"); else{ snprintf(buf,sizeof(buf),"%3ld",dist); Serial.print(buf); }
  Serial.print("    | ");
  Serial.print(s); Serial.print("  ");
  Serial.print(slotExpected); Serial.print(" | ");
  snprintf(buf,sizeof(buf),"%3d%%",batt); Serial.print(buf); Serial.print(" "); Serial.print(bar); Serial.print(" | ");
  Serial.println(alerts);
  lineCount++;
}

/*** Ajuda comandos ***/
void ajuda(){
  Serial.println(F("\nComandos (uma linha):"));
  Serial.println(F("  {\"action\":\"beep\",\"duration_ms\":1500}"));
  Serial.println(F("  {\"set_expected\":\"A-12\"}"));
  Serial.println(F("  {\"modo\":\"tabela\" | \"json\" | \"ambos\"}"));
  Serial.println(F("  {\"status\":true}\n"));
}

/*** Beep (tone/noTone) ***/
void beepOn(){ tone(BUZZER_PIN, 2000); }
void beepOff(){ noTone(BUZZER_PIN); }

/*** Wi-Fi ***/
void wifiConnect(){
  if(WiFi.status() == WL_CONNECTED) return;
  Serial.print("WiFi conectando a "); Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(400); Serial.print(".");
  }
  Serial.println(" OK");
}

/*** ThingSpeak (HTTP) ***/
bool tsSend(long dist_cm, int presenca, const String& slot, int bateria_pct,
            const String& esperado, int fora_do_lugar, int ocup_desconh){
  if(strlen(WRITE_API_KEY) < 8) {
    Serial.println("[TS] Falta WRITE_API_KEY");
    return false;
  }
  HTTPClient http;
  String body = "api_key=" + String(WRITE_API_KEY) +
                "&field1=" + String(dist_cm) +
                "&field2=" + String(presenca) +
                "&field3=" + (slot.length()?slot:"-") +
                "&field4=" + String(bateria_pct) +
                "&field5=" + (esperado.length()?esperado:"-") +
                "&field6=" + String(fora_do_lugar) +
                "&field7=" + String(ocup_desconh);
  http.begin(TS_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
  if(code==200 && resp.length()>0) {
    Serial.print("[TS OK] entry_id="); Serial.println(resp);
    return true;
  } else {
    Serial.print("[TS ERRO] HTTP "); Serial.print(code); Serial.print(" -> "); Serial.println(resp);
    return false;
  }
}

/*** Serial comandos ***/
void handleSerial(){
  static String line="";
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='\n'||c=='\r'){
      if(line.length()>0){
        String s=line; line="";
        if(s.indexOf("\"action\"")>=0 && s.indexOf("beep")>=0){
          unsigned long dur=1000;
          int i=s.indexOf("duration_ms");
          if(i>=0){ int j=s.indexOf(':',i); if(j>=0){
              String num=""; for(int k=j+1;k<(int)s.length();k++){
                if(isDigit(s[k])) num+=s[k]; else if(num.length()>0) break;
              }
              if(num.length()>0) dur=(unsigned long)num.toInt();
          }}
          beepActive=true; beepEnd=millis()+dur;
        }
        int k=s.indexOf("\"set_expected\"");
        if(k>=0){ int q1=s.indexOf('"',k+15); int q2=(q1>=0)?s.indexOf('"',q1+1):-1; if(q1>=0 && q2>q1) slotExpected=s.substring(q1+1,q2); }
        if(s.indexOf("\"modo\"")>=0 || s.indexOf("\"mode\"")>=0){
          bool tabela=(s.indexOf("tabela")>=0)||(s.indexOf("pretty")>=0);
          bool json=(s.indexOf("\"json\"")>=0);
          bool ambos=(s.indexOf("ambos")>=0)||(s.indexOf("both")>=0);
          if(tabela && !ambos && !json) modoSaida=MODE_TABELA;
          else if(json && !ambos) modoSaida=MODE_JSON;
          else modoSaida=MODE_AMBOS;
          Serial.print(F(">> modo=")); Serial.println(modoSaida==MODE_TABELA?F("tabela"):modoSaida==MODE_JSON?F("json"):F("ambos"));
          printHeader();
        }
        if(s.indexOf("\"status\"")>=0){
          bool presenca=(lastDistance<OCCUPIED_CM);
          int  bateria=readBatteryPct();
          bool lowBatt=(bateria<LOW_BATT_PCT);
          String slotCurr=slotAtual();
          bool hasSlot=(slotCurr.length()>0);
          bool misplaced=presenca && hasSlot && (slotCurr!=slotExpected);
          bool unknownOcc=presenca && !hasSlot;
          printTabela(presenca,lastDistance,bateria,misplaced,unknownOcc,lowBatt);
          if(modoSaida!=MODE_TABELA) printJSON(presenca,bateria,misplaced,unknownOcc,lowBatt);
        }
        if(s.indexOf("ajuda")>=0||s.indexOf("help")>=0) ajuda();
      }
    } else {
      line+=c; if(line.length()>200) line.remove(0,50);
    }
  }
}

void setup(){
  Serial.begin(115200);

  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);
  pinMode(LED_LOC,OUTPUT);
  pinMode(LED_OCC,OUTPUT);
  pinMode(LED_MIS,OUTPUT);
  pinMode(LED_UNK,OUTPUT);
  pinMode(LED_LOW,OUTPUT);

  setupBtn(bInc); setupBtn(bArea); setupBtn(bClear);

  digitalWrite(LED_LOC,LOW); digitalWrite(LED_OCC,LOW);
  digitalWrite(LED_MIS,LOW); digitalWrite(LED_UNK,LOW); digitalWrite(LED_LOW,LOW);

  wifiConnect();

  Serial.println(F("\nEasyMoto ESP32 — HTTP + tone/noTone"));
  ajuda();
  printHeader();
}

void loop(){
  unsigned long now=millis();

  if(updateBtn(bInc)){ slotNumber++; if(slotNumber>99) slotNumber=1; }
  if(updateBtn(bArea)) areaA=!areaA;
  if(updateBtn(bClear)) slotNumber=0;

  if(now-lastDistRead>=READ_DIST_MS){ lastDistance=readDistanceCM(); lastDistRead=now; }

  bool presenca=(lastDistance<OCCUPIED_CM);
  int  bateria=readBatteryPct();
  bool lowBatt=(bateria<LOW_BATT_PCT);
  String slotCurr=slotAtual();
  bool hasSlot=(slotCurr.length()>0);
  bool misplaced=presenca && hasSlot && (slotCurr!=slotExpected);
  bool unknownOcc=presenca && !hasSlot;

  digitalWrite(LED_OCC, presenca?HIGH:LOW);
  digitalWrite(LED_MIS, misplaced?HIGH:LOW);
  digitalWrite(LED_UNK, unknownOcc?HIGH:LOW);
  digitalWrite(LED_LOW, lowBatt?HIGH:LOW);

  if(beepActive){
    if(now<beepEnd){ digitalWrite(LED_LOC,HIGH); beepOn(); }
    else { beepActive=false; digitalWrite(LED_LOC,LOW); beepOff(); }
  }

  if(now-lastTel>=TELEMETRY_MS){
    if(modoSaida==MODE_TABELA||modoSaida==MODE_AMBOS) printTabela(presenca,lastDistance,bateria,misplaced,unknownOcc,lowBatt);
    if(modoSaida==MODE_JSON||modoSaida==MODE_AMBOS)   printJSON(presenca,bateria,misplaced,unknownOcc,lowBatt);
    lastTel=now;
  }

  if(now-lastTsSend >= TS_INTERVAL_MS){
    wifiConnect();
    tsSend(lastDistance, presenca?1:0, slotCurr.length()?slotCurr:"-",
           bateria, slotExpected.length()?slotExpected:"-",
           misplaced?1:0, unknownOcc?1:0);
    lastTsSend = now;
  }

  handleSerial();
}
