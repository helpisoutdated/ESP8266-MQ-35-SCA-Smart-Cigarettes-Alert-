#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP8266HTTPClient.h>
#include <time.h>

// =SENSOR
#define A_CO 13760.9304
#define B_CO -4.27562
const float RL = 1000.0;
const float VCC = 5.0;
const int ADC_MAX = 1023;
const int SENSOR_PIN = A0;

float R0 = 0.0;
float ppm = 0.0;
bool systemOn = true;

// WIFI & TELEGRAM SETUP
const char* ssid = "-------------";
const char* password = "-------------";
#define BOT_TOKEN "-----------------"
String CHAT_ID = "-------------------";

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

const float CO_THRESHOLD = 75;
unsigned long lastAlertTime = 0;
const unsigned long ALERT_COOLDOWN = 5000;
unsigned long lastCheckTime = 0;
const unsigned long BOT_CHECK_INTERVAL = 500;

// ===== NTP =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7*3600;
const int daylightOffset_sec = 0;

// LINK GOOGLE SHEETS
String sheetsURL = "----------------------------";
String sheetsreal = "--------------------------;
String lokasi = "KAMAR%20MANDI%202"; 

// FUNGSI SENSOR
float readRs() {
  int adc = analogRead(SENSOR_PIN);
  float vout = (float)adc / ADC_MAX * VCC;
  if (vout < 1e-6) vout = 1e-6;
  return RL * (VCC - vout) / vout;
}

float calibrateR0(int samples = 200) {
  float rsSum = 0;
  for (int i = 0; i < samples; i++) {
    rsSum += readRs();
    delay(100);
  }
  return rsSum / samples / 4;
}

float calcPPM(float rs) {
  float ratio = rs / R0;
  return A_CO * pow(ratio, B_CO);
}

// TIMESTAMP
String getTimestamp() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "0000-00-00 00:00:00";
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

//TELEGRAM
void handleNewMessages(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++){
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if(chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "⚠️ Maaf, tidak diizinkan.", "");
      continue;
    }

    if(text == "/Mulai" || text == "/mulai"){
      systemOn = true;
      bot.sendMessage(CHAT_ID, "✅ Sistem dimulai!\nR0 saat ini: " + String(R0,2), "");
    }
    else if(text == "/Off" || text == "/off"){
      systemOn = false;
      bot.sendMessage(CHAT_ID, "🛑 Sistem dimatikan.", "");
    }
    else if(text == "/Recalibrate" || text == "/recalibrate"){
      bot.sendMessage(CHAT_ID, "⚙️ Kalibrasi sensor...");
      R0 = calibrateR0();
      bot.sendMessage(CHAT_ID, "✅ Kalibrasi selesai!\nR0 baru: " + String(R0,2));
    }
    else if(text == "/Tes" || text == "/tes"){
      float rs = readRs();
      float currentPPM = calcPPM(rs);
      String status = (currentPPM > CO_THRESHOLD) ? "🚨 Asap Terdeteksi!" : "✅ Normal";
      String msg = "📟 Data Sensor Saat Ini:\nCO: " + String(currentPPM,2) + " ppm\nStatus: " + status;
      bot.sendMessage(CHAT_ID, msg, "");
    }
    else if(text == "/Rekap" || text == "/rekap"){
      String msg = "📊 Laporan Google Sheets:\n" + sheetsreal;
      bot.sendMessage(CHAT_ID, msg, "");
    }
    else if(text == "/Help" || text == "/help"){
      String helpMsg = "*Perintah Tersedia:*\n";
      helpMsg += "/Mulai - Menyalakan sistem\n";
      helpMsg += "/Off - Mematikan sistem\n";
      helpMsg += "/Tes - Tampilkan data sensor\n";
      helpMsg += "/Recalibrate - Kalibrasi sensor\n";
      helpMsg += "/Rekap - Link Google Sheets\n";
      helpMsg += "/Help - Tampilkan daftar perintah";
      bot.sendMessage(CHAT_ID, helpMsg, "Markdown");
    }
    else{
      bot.sendMessage(CHAT_ID, "⚠️ Perintah tidak dikenal. Gunakan /Help.", "");
    }
  }
}

// SETUP
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Kalibrasi MQ135 ===");
  R0 = calibrateR0();
  Serial.print("R0: "); Serial.println(R0);

  WiFi.begin(ssid,password);
  client.setInsecure();
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("\n✅ WiFi terhubung!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  bot.sendMessage(CHAT_ID,"✅ Sistem Smart Cigarette Alert dimulai.\nR0 awal: "+String(R0,2),"");
}

// SLOP
void loop() {

  if(systemOn){
    float rs = readRs();
    ppm = calcPPM(rs);
    String ts = getTimestamp();

    Serial.print("CO: "); Serial.print(ppm,2); 
    Serial.print(" ppm | "); Serial.println(ts);

    //GOOGLE SHEETS
    if(ppm > CO_THRESHOLD){
      HTTPClient http;
      client.setInsecure();

      String tsURL = ts;
      tsURL.replace(" ", "%20");

      String url = sheetsURL + "?ppm=" + String(ppm,2);
      url += "&timestamp=" + tsURL;
      url += "&lokasi=" + lokasi;

      Serial.print("Mengirim ke Sheets: "); 
      Serial.println(url);

      http.begin(client,url);
      int httpCode = http.GET();
      Serial.print("HTTP response code: "); 
      Serial.println(httpCode);

      if(httpCode>0){
        Serial.println("✅ Data terkirim ke Google Sheets");
      } else {
        Serial.println("⚠️ Gagal mengirim data ke Google Sheets");
      }
      http.end();
      
      unsigned long currentTime = millis();
      if(currentTime - lastAlertTime > ALERT_COOLDOWN){
        String tsMSG = ts;  
        String msg = 
          "🚨 PERINGATAN!\n"
          "Asap rokok terdeteksi!\n"
          "CO: "+String(ppm,2)+" ppm\n"
          "Lokasi: Kamar Mandi 2\n"
          "Timestamp: " + tsMSG;

        bot.sendMessage(CHAT_ID,msg);
        Serial.println("📤 Peringatan dikirim ke Telegram!");

        lastAlertTime = currentTime;
      }
    }
  }

  if(millis()-lastCheckTime>BOT_CHECK_INTERVAL){
    int numNew = bot.getUpdates(bot.last_message_received+1);
    if(numNew) handleNewMessages(numNew);
    lastCheckTime = millis();
  }

  delay(1000);
}
