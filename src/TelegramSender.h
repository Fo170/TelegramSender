// ============================================================
// TelegramSender.h - Envoi bas niveau de messages Telegram
// Version 1.3.0 - https://github.com/Fo170/TelegramSender
// ESP8266 et ESP32. Envoi : sendMessage(), polling : getUpdatesBody().
// Gestion évoluée (queue, retry, throttling) à faire dans une
// librairie de plus haut niveau.
// ============================================================
//
// Depuis la v1.3.0 : session TLS PERSISTANTE. Le WiFiClientSecure est
// un membre de la classe (un seul handshake TLS à la connexion), au
// lieu d'un client local recréé à CHAQUE appel. Le handshake
// mbedTLS/BearSSL (~10-30 Ko) alloué/libéré à chaque poll/send
// fragmentait le heap (ESP32 : abort()/reboot après quelques heures)
// et ralentissait les échanges. L'HTTP est fait en requêtes HTTP/1.1
// keep-alive brutes sur la session (plus de HTTPClient), et la session
// est fermée/refaite automatiquement si la connexion tombe.
//   Ajout : getUpdatesBody(offset, httpCode) pour le polling sur la
//   même session, et endSession() pour fermer la session explicitement.
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
#define TELEGRAM_API_HOST       "api.telegram.org"
#define TELEGRAM_SEND_MESSAGE   "/sendMessage"
#define TELEGRAM_GET_UPDATES    "/getUpdates"

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

// Garde heap minimale pour un envoi TLS : les buffers BearSSL/mbedTLS
// (~10-30 Ko, selon la plateforme) sont alloués au handshake, pas au boot.
// En-dessous, on diffère plutôt que crasher.
// ⚠️ Fourchette locale : seuil surchargeable à la compilation via
//    -DTELEGRAM_MIN_HEAP=xxxx (le heap de repos du coulomètre est ~10 Ko).
#ifndef TELEGRAM_MIN_HEAP
#define TELEGRAM_MIN_HEAP 12000
#endif

// Code retour de sendMessage() : 0 = succès, sinon motif de l'échec.
enum TelegramError {
    TELEGRAM_OK        = 0,
    TELEGRAM_DISABLED,     // envoi désactivé (setEnabled(false))
    TELEGRAM_NO_WIFI,      // WiFi non connecté
    TELEGRAM_NO_CLOCK,     // horloge non synchronisée (NTP requis pour BearSSL)
    TELEGRAM_LOW_HEAP,     // heap < TELEGRAM_MIN_HEAP → on diffère l'envoi
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

    // Mode de formatage du message côté Telegram : "" (texte brut), "Markdown"
    // (défaut), "HTML" ou "MarkdownV2".
    // ⚠️ Les caractères réservés du mode choisi (ex. "_" ou "*" en Markdown,
    // très fréquents dans un message technique) doivent être échappés par
    // l'appelant, sinon Telegram répond HTTP 400. Pour un texte sans mise en
    // forme, passer setParseMode("") est le plus sûr.
    void setParseMode(const String& mode) {
        parseMode = mode;
    }

    // Ferme explicitement la session TLS persistante (le handshake sera
    // refait à la prochaine requête). Utile en cas de perte WiFi prolongée.
    void endSession() {
        _closeSession();
    }

    // Envoi bas niveau : une seule tentative synchrone.
    // Retourne TELEGRAM_OK si le serveur a répondu 200 + "ok":true.
    // Session TLS PERSISTANTE : le handshake n'a lieu qu'une fois.
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

        // Garde heap : les buffers TLS sont alloués au handshake. En-dessous,
        // on diffère plutôt que crasher.
        if (ESP.getFreeHeap() < TELEGRAM_MIN_HEAP) {
            return TELEGRAM_LOW_HEAP;
        }

        // La validation TLS exige une horloge correcte (configTime).
        if (validateCert && time(nullptr) < 100000) {
            return TELEGRAM_NO_CLOCK;
        }

        String body = "chat_id=" + String(TELEGRAM_CHAT_ID) +
                      "&text=" + urlEncode(message);
        if (parseMode.length() > 0) {
            body += "&parse_mode=" + urlEncode(parseMode);
        }

        String path = String(TELEGRAM_API_URL) + String(TELEGRAM_BOT_TOKEN) +
                      String(TELEGRAM_SEND_MESSAGE);

        int httpCode = -1;
        String response = _httpRequest(path.c_str(), body, httpCode);

        if (httpCode != 200) {
            return TELEGRAM_HTTP_ERROR;
        }
        // Un HTTP 200 ne suffit pas : vérifier le champ "ok" de la réponse
        // (un 200 trompeur avec "ok":false reste un échec).
        bool apiOk = (response.indexOf("\"ok\":true") != -1);
        return apiOk ? TELEGRAM_OK : TELEGRAM_HTTP_ERROR;
    }

    // Poll getUpdates sur la MÊME session TLS persistante (réutilisé par
    // TelegramControl). Retourne le corps JSON brut ("" si échec réseau/HTTP)
    // et remplit httpCode (200 = OK, -1 = échec de connexion).
    String getUpdatesBody(unsigned long offset, int& httpCode) {
        String path = String(TELEGRAM_API_URL) + String(TELEGRAM_BOT_TOKEN) +
                      String(TELEGRAM_GET_UPDATES) + "?offset=" + String(offset);
        return _httpRequest(path.c_str(), String(), httpCode);
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
    // ══════════════════════════════════════════════════════════════
    // Session TLS persistante (depuis v1.3.0)
    // ══════════════════════════════════════════════════════════════
    WiFiClientSecure _tls;          // client TLS MEMBRE : un seul handshake
    bool _sessionReady = false;

    void _closeSession() {
        _tls.stop();
        _sessionReady = false;
    }

    // Ouvre la session TLS si nécessaire (1 seul handshake), la rouvre si
    // la connexion est tombée. Retourne false si impossible (WiFi, horloge,
    // échec de connexion TCP/TLS).
    bool _ensureSession() {
        if (_sessionReady && _tls.connected()) return true;
        _closeSession();

        _tls.setNoDelay(true);
        _tls.setTimeout(3000);

        if (validateCert) {
            // La validation BearSSL/mbedTLS exige une horloge correcte.
            if (time(nullptr) < 100000) return false;
            #ifdef ESP8266
            // Certificat racine STATIC : construit une seule fois (au 1er envoi).
            static BearSSL::X509List cert(TELEGRAM_ROOT_CA);
            _tls.setTrustAnchors(&cert);
            _tls.setBufferSizes(1024, 1024);
            #else
            _tls.setCACert(TELEGRAM_ROOT_CA);
            #endif
        } else {
            _tls.setInsecure();
        }

        if (!_tls.connect(TELEGRAM_API_HOST, 443)) {
            _closeSession();
            return false;
        }
        _sessionReady = true;
        return true;
    }

    // Lit une ligne (jusqu'au '\n', '\r' final supprimé). false si timeout
    // ou connexion fermée avant la fin de ligne.
    bool _readLine(String& out) {
        out = "";
        uint32_t start = millis();
        while (millis() - start < 5000UL) {
            if (!_tls.connected() && !_tls.available()) return false;
            int c = _tls.read();
            if (c < 0) {
                if (!_tls.available()) return false;   // timeout / fermeture
                continue;
            }
            if (c == 0) continue;
            if (c == '\n') {
                if (out.length() && out[out.length() - 1] == '\r') out.remove(out.length() - 1);
                return true;
            }
            out += (char)c;
        }
        return false;
    }

    // Lit exactement `len` octets (corps de réponse). "" si connexion coupée.
    String _readBody(int len) {
        String s;
        if (len > 0) s.reserve(len);
        uint32_t start = millis();
        while ((int)s.length() < len) {
            if (!_tls.available() && !_tls.connected()) break;
            int c = _tls.read();
            if (c < 0) {
                if (millis() - start > 5000UL) break;
                continue;
            }
            if (c == 0) continue;
            s += (char)c;
        }
        return s;
    }

    // Requête HTTP/1.1 keep-alive sur la session TLS persistante.
    //  - path     : "/bot<token>/sendMessage" ou "/bot<token>/getUpdates?offset=N"
    //  - postBody : vide → GET ; sinon POST application/x-www-form-urlencoded.
    // Retourne le corps de réponse (JSON Telegram). Remplit httpCode
    // (200 = OK, -1 = échec réseau/TLS). La connexion est réutilisée
    // (keep-alive) ; fermée si le serveur l'impose (Connection: close).
    String _httpRequest(const char* path, const String& postBody, int& httpCode) {
        httpCode = -1;
        if (!_ensureSession()) return "";

        bool hasBody = (postBody.length() > 0);
        String req;
        req += (hasBody ? "POST " : "GET ") + String(path) + " HTTP/1.1\r\n";
        req += "Host: " TELEGRAM_API_HOST "\r\n";
        req += "User-Agent: TelegramSender/1.3.0\r\n";
        req += "Accept: application/json\r\n";
        if (hasBody) {
            req += "Content-Type: application/x-www-form-urlencoded\r\n";
            req += "Content-Length: " + String(postBody.length()) + "\r\n";
        }
        req += "Connection: keep-alive\r\n\r\n";
        if (hasBody) req += postBody;

        if ((int)_tls.write((const uint8_t*)req.c_str(), req.length()) != (int)req.length()) {
            _closeSession();
            return "";
        }
        _tls.flush();

        // Ligne de statut : "HTTP/1.1 200 OK"
        String status;
        if (!_readLine(status)) { _closeSession(); return ""; }
        int sp = status.indexOf(' ');
        httpCode = (sp >= 0) ? status.substring(sp + 1).toInt() : -1;

        // En-têtes (jusqu'à la ligne vide) : Content-Length + Connection.
        long contentLen = -1;
        bool keepAlive = true;
        for (;;) {
            String h;
            if (!_readLine(h)) { _closeSession(); return ""; }
            if (h.length() == 0) break;
            int c = h.indexOf(':');
            if (c < 0) continue;
            String name = h.substring(0, c); name.toLowerCase();
            String value = h.substring(c + 1); value.trim();
            if (name == "content-length") contentLen = value.toInt();
            else if (name == "connection" && value.indexOf("close") >= 0) keepAlive = false;
        }

        String body;
        if (contentLen >= 0) {
            body = _readBody((int)contentLen);
        } else {
            // Sans Content-Length : lire jusqu'à fermeture (on ne réutilisera pas la session).
            while (_tls.available()) body += (char)_tls.read();
            keepAlive = false;
        }

        if (!keepAlive) _closeSession();
        return body;
    }

    // URL encoding pour Telegram API (application/x-www-form-urlencoded).
    // Traite les octets en non-signé : sans cela, les caractères UTF-8 (é, °…)
    // deviennent négatifs et sont mal encodés (UB sur isalnum, %0ffffffc3…).
    String urlEncode(String str) {
        String encoded = "";
        for (size_t i = 0; i < str.length(); i++) {
            uint8_t c = (uint8_t)str.charAt(i);
            if (c == ' ') {
                encoded += "%20";
            } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += (char)c;
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
    String parseMode = "Markdown";  // formatage Telegram : "" = texte brut, sinon Markdown/HTML/MarkdownV2
};

#endif // TELEGRAM_SENDER_H
