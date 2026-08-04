// ============================================================
// TelegramSender.h - Envoi bas niveau de messages Telegram
// Version 1.1.0 - https://github.com/Fo170/TelegramSender
// ESP8266 et ESP32. Une seule fonction : sendMessage(). Gestion
// évoluée (queue, retry, throttling) à faire dans une librairie
// de plus haut niveau.
// ============================================================

#ifndef TELEGRAM_SENDER_H
#define TELEGRAM_SENDER_H

#include <Arduino.h>
#include <time.h>

// WiFi / HTTPClient selon la plateforme :
//  - ESP8266 : ESP8266WiFi + ESP8266HTTPClient (BearSSL)
//  - ESP32   : WiFi + HTTPClient
//  - autres  : tentative générique (WiFi.h + HTTPClient.h), non testée
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
#else
  #warning "TelegramSender : plateforme non officiellement supportee (ESP8266/ESP32) - compilation en mode generique"
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
#endif

// Configuration Telegram (à définir à la compilation, jamais en clair ici)
//#define TELEGRAM_BOT_TOKEN      "xxxxx"
//#define TELEGRAM_CHAT_ID        "yyyyy"

// URLs Telegram API
#define TELEGRAM_API_URL        "https://api.telegram.org/bot"
#define TELEGRAM_SEND_MESSAGE   "/sendMessage"

// Certificat racine signataire de api.telegram.org
// (Go Daddy Root Certificate Authority - G2, valide jusqu'en 2037).
// Stocké en PROGMEM : c'est la forme attendue par le constructeur
// BearSSL::X509List de l'ESP8266 (lecture via pgm_read_byte).
static const char TELEGRAM_ROOT_CA[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
    "EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
    "EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
    "ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz\n"
    "NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
    "EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE\n"
    "AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw\n"
    "DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD\n"
    "E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH\n"
    "/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy\n"
    "DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh\n"
    "GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR\n"
    "tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA\n"
    "AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE\n"
    "FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX\n"
    "WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu\n"
    "9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr\n"
    "gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo\n"
    "2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO\n"
    "LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI\n"
    "4uJEvlz36hz1\n"
    "-----END CERTIFICATE-----\n";

// Code retour de sendMessage() : 0 = succès, sinon motif de l'échec.
enum TelegramError {
    TELEGRAM_OK        = 0,
    TELEGRAM_DISABLED,     // envoi désactivé (setEnabled(false))
    TELEGRAM_NO_WIFI,      // WiFi non connecté
    TELEGRAM_NO_CLOCK,     // horloge non synchronisée (NTP requis pour BearSSL)
    TELEGRAM_LOW_HEAP,     // heap < 12000 → on diffère l'envoi
    TELEGRAM_HTTP_ERROR    // HTTP != 200 ou handshake TLS échoué
};

class TelegramSender {
public:
    TelegramSender() {}

    void setEnabled(bool en) {
        enabled = en;
    }

    void setTestMode(bool test) {
        useTestMode = test;
    }

    // Activer/désactiver la validation du certificat serveur.
    // À couper uniquement si l'horloge n'est jamais synchronisée (configTime/NTP)
    // : la validation BearSSL exige une date correcte pour accepter le certificat.
    void setCertValidation(bool enable) {
        validateCert = enable;
    }

    // Envoi bas niveau : une seule tentative synchrone.
    // Retourne TELEGRAM_OK si le serveur a répondu 200, sinon le motif de l'échec.
    int sendMessage(const String& message) {
        if (!enabled) {
            return TELEGRAM_DISABLED;
        }

        if (useTestMode) {
            Serial.print("[TELEGRAM TEST] Message: ");
            Serial.println(message);
            return TELEGRAM_OK;
        }

        if (WiFi.status() != WL_CONNECTED) {
            return TELEGRAM_NO_WIFI;
        }

        // Garde heap : les buffers BearSSL (~11 Ko) sont alloués au moment du
        // handshake TLS, pas au boot. En-dessous, on diffère plutôt que crasher.
        if (ESP.getFreeHeap() < 12000) {
            return TELEGRAM_LOW_HEAP;
        }

        // WiFiClientSecure LOCAL (pas membre global) : les buffers BearSSL ne
        // sont alloués qu'à la connexion. Validation TLS par certificat.
        WiFiClientSecure wifiClient;
        if (validateCert) {
            // La validation BearSSL exige une horloge correcte (configTime) :
            // sinon le certificat est jugé hors validité.
            if (time(nullptr) < 100000) {
                return TELEGRAM_NO_CLOCK;
            }
            #ifdef ESP8266
            static BearSSL::X509List cert(TELEGRAM_ROOT_CA);
            wifiClient.setTrustAnchors(&cert);
            #else
            wifiClient.setCACert(TELEGRAM_ROOT_CA);
            #endif
        } else {
            wifiClient.setInsecure();
        }
        // Buffers TLS réduits pour économiser le heap (BearSSL ~2 Ko au lieu
        // de ~4) - API spécifique ESP8266
#ifdef ESP8266
        wifiClient.setBufferSizes(1024, 1024);
#endif
        // HTTPClient également LOCAL : ses buffers sont libérés à chaque envoi
        HTTPClient http;

        String url = String(TELEGRAM_API_URL) + String(TELEGRAM_BOT_TOKEN) +
                    String(TELEGRAM_SEND_MESSAGE) +
                    "?chat_id=" + String(TELEGRAM_CHAT_ID) +
                    "&text=" + urlEncode(message) +
                    "&parse_mode=Markdown";

        http.begin(wifiClient, url);

        // Timeout court: 3s max (handshake TLS peut bloquer longtemps sinon)
        http.setTimeout(3000);

        int httpCode = http.GET();

        http.end();
        // Libération explicite des buffers TLS BearSSL (sinon ~13 Ko de heap
        // restent alloués/fragmentés après chaque envoi)
        wifiClient.stop();

        return (httpCode == HTTP_CODE_OK) ? TELEGRAM_OK : TELEGRAM_HTTP_ERROR;
    }

    // Message texte associé à un code retour (pour debug/log sans dépendance).
    static const char* errorText(int code) {
        switch (code) {
            case TELEGRAM_OK:        return "OK";
            case TELEGRAM_DISABLED:  return "Envoi désactivé";
            case TELEGRAM_NO_WIFI:   return "WiFi non connecté";
            case TELEGRAM_NO_CLOCK:  return "Horloge non synchronisée (configTime requis)";
            case TELEGRAM_LOW_HEAP:  return "Heap insuffisant";
            case TELEGRAM_HTTP_ERROR:return "Erreur HTTP / handshake TLS";
            default:                 return "Code inconnu";
        }
    }

private:
    // URL encoding pour Telegram API
    String urlEncode(String str) {
        String encoded = "";
        char c;
        for (size_t i = 0; i < str.length(); i++) {
            c = str.charAt(i);
            if (c == ' ') {
                encoded += "%20";
            } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                encoded += "%";
                if (c < 0x10) {
                    encoded += "0";
                }
                encoded += String(c, HEX);
            }
        }
        return encoded;
    }

    bool enabled = true;
    bool useTestMode = false;
    bool validateCert = true;   // TLS : valider le certificat serveur (au lieu de setInsecure)
};

#endif // TELEGRAM_SENDER_H
