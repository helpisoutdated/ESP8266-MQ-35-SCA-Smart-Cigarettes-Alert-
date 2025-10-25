#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// KONFIGURASI SENSOR
#define A_CO 13760.9304 
#define B_CO -4.27562
const float RL = 1000.0; 
const float VCC = 5.0;    
const int ADC_MAX = 1023;  
const int SENSOR_PIN = A0; 

float R0 = 0.0;
float ppm = 0.0;

// TELEGRAM 
const char* ssid = "-------";
const char* password = "-------";
#define BOT_TOKEN "-----------------------"
String CHAT_ID = "-------------";

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

const float CO_THRESHOLD = 75; 
unsigned long lastAlertTime = 0;
const unsigned long ALERT_COOLDOWN = 30000;
unsigned long lastCheckTime = 0;
const unsigned long BOT_CHECK_INTERVAL = 2000; 

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
  float rsAvg = rsSum / samples;
  float R0calc = rsAvg / 4;
  return R0calc;
}

float calcPPM(float rs) {
  float ratio = rs / R0;
  return A_CO * pow(ratio, B_CO);
}

// COMMAND BOT
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Maaf, kamu tidak diizinkan menggunakan bot ini.", "");
      continue;
    }

    if (text == "/Mulai" || text == "/mulai") {
      String msg = "Sistem Smart Cigarette Alert dimulai!\n"
                   "R0 saat ini: " + String(R0, 2);
      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "/Recalibrate" || text == "/recalibrate") {
      bot.sendMessage(CHAT_ID, "Kalibrasi ulang sensor sedang dilakukan...\nPastikan udara bersih tanpa asap.");
      R0 = calibrateR0();
      String msg = "Kalibrasi selesai!\nNilai R0 baru: " + String(R0, 2);
      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "/Tes" || text == "/tes") {
      float rs = readRs();
      float currentPPM = calcPPM(rs);
      String status = (currentPPM > CO_THRESHOLD) ? "Asap Terdeteksi!" : "Normal";
      String msg = "Data Sensor Saat Ini:\n"
                   "Rs: " + String(rs, 2) + " Ω\n"
                   "R0: " + String(R0, 2) + " Ω\n"
                   "CO: " + String(currentPPM, 2) + " ppm\n"
                   "Status: " + status;
      bot.sendMessage(CHAT_ID, msg, "");
    }
    else if (text == "/Help" || text == "/help") {
      String helpMsg = "*Daftar Perintah Tersedia:*\n\n";
      helpMsg += "/Mulai - Menampilkan pesan sistem dimulai.\n";
      helpMsg += "/Tes - Menampilkan data sensor saat ini (Rs, R0, dan CO ppm).\n";
      helpMsg += "/Recalibrate - Kalibrasi ulang sensor MQ135.\n";
      helpMsg += "/Help - Menampilkan daftar perintah ini.";
  bot.sendMessage(CHAT_ID, helpMsg, "Markdown");
}

    }
  }


// SETUP
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== Kalibrasi MQ135 ===");
  Serial.println("Pastikan udara bersih (tanpa asap rokok).");
  R0 = calibrateR0();
  Serial.print("R0 terkalibrasi: ");
  Serial.println(R0);
  Serial.println("=========================\n");

  // Koneksi WiFi
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  client.setInsecure();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  bot.sendMessage(CHAT_ID, "Sistem Smart Cigarette Alert dimulai.\nR0 awal: " + String(R0, 2), "");
}

// LOOPING
void loop() {
  float rs = readRs();
  ppm = calcPPM(rs);

  Serial.print("Rs: "); Serial.print(rs, 2);
  Serial.print(" Ω | CO: "); Serial.print(ppm, 2);
  Serial.print(" ppm");

  if (ppm > CO_THRESHOLD) {
    Serial.println(" Asap rokok terdeteksi!");

    unsigned long currentTime = millis();
    if (currentTime - lastAlertTime > ALERT_COOLDOWN) {
      String message = "PERINGATAN!\n"
                       "Asap rokok terdeteksi di Kamar Mandi 2!\n"
                       "Kadar CO: " + String(ppm, 2) + " ppm\n"
                       "Ambang batas: " + String(CO_THRESHOLD, 1) + " ppm";
      bot.sendMessage(CHAT_ID, message, "");
      Serial.println("Peringatan dikirim ke Telegram!");
      lastAlertTime = currentTime;
    }
  } else {
    Serial.println(" Normal");
  }

  // Cek pesan
  if (millis() - lastCheckTime > BOT_CHECK_INTERVAL) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    if (numNewMessages) handleNewMessages(numNewMessages);
    lastCheckTime = millis();
  }

  delay(500);
}
