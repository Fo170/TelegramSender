/*
 * TelegramSender - Example TelegramDemo
 * https://github.com/Fo170/TelegramSender
 *
 * Envoie un message Telegram au boot (test mode puis envoi réel).
 * Prérequis :
 *   - WiFi
 *   - configTime() : obligatoire pour la validation BearSSL (sinon TELEGRAM_NO_CLOCK)
 *   - TELEGRAM_BOT_TOKEN / TELEGRAM_CHAT_ID définis à la compilation
 *     (remplacez ci-dessous ou passez-les en build_flags)
 */

#include <Arduino.h>
#include <time.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#define TELEGRAM_BOT_TOKEN  "123456789:AA...remplacez"
#define TELEGRAM_CHAT_ID    "123456789"

#include <TelegramSender.h>

const char* SSID     = "VOTRE_SSID";
const char* PASSWORD = "VOTRE_MOT_DE_PASSE";

TelegramSender tg;

void syncTime() {
    configTime(3600, 0, "pool.ntp.org");
    Serial.print("Synchronisation NTP ");
    while (time(nullptr) < 100000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" OK");
}

void setup() {
    Serial.begin(115200);
    delay(200);

    WiFi.begin(SSID, PASSWORD);
    Serial.print("Connexion WiFi ");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" OK");

    syncTime();

    // Test mode : le message est affiché sur Serial, rien n'est envoyé
    tg.setTestMode(true);
    tg.sendMessage("Test mode : message non envoyé");

    // Envoi réel
    tg.setTestMode(false);
    int code = tg.sendMessage("TelegramSender opérationnel !");
    Serial.print("sendMessage -> ");
    Serial.println(TelegramSender::errorText(code));
}

void loop() {
    // Rien à faire : envoi synchrone au boot
}
