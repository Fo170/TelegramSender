# TelegramSender

Envoi bas niveau de messages Telegram pour **ESP8266 et ESP32** (header-only, une seule fonction synchrone `sendMessage()`).

https://github.com/Fo170/TelegramSender

## Caractéristiques

- `int sendMessage(const String&)` → `int` (`TelegramError`, 0 = succès), **aucune allocation heap en retour**.
- Envoi en **POST** (pas de limite d'URL, message hors ligne de requête) ; le succès vérifie le champ `"ok":true` de la réponse, pas seulement le code HTTP.
- TLS par certificat : `TELEGRAM_ROOT_CA` (Go Daddy Root CA - G2) en PROGMEM, validation BearSSL (ESP8266) / mbedTLS (ESP32). `setCertValidation(false)` → `setInsecure()` (uniquement si l'horloge n'est jamais synchronisée).
- Configuration : `setEnabled(bool)` (défaut `true`), `setTestMode(bool)` (défaut `false`, affiche sur Serial au lieu d'envoyer), `setCertValidation(bool)` (défaut `true`), `setParseMode(const String&)` (défaut `"Markdown"` ; `""` = texte brut).
- `static const char* errorText(int code)` : texte des codes retour, sans dépendance externe.
- Gestion évoluée (queue, retry, throttling) : **hors de cette lib**, à faire dans une librairie de plus haut niveau.

## Plateformes

- **Officiellement supportées** : ESP8266 (BearSSL, `setBufferSizes(1024,1024)`) et ESP32.
- Autres plateformes : tentative générique (`WiFi.h` + `HTTPClient.h`), compilable mais **non testée**.

## Token et chat ID

**Jamais en clair dans le code.** Définir à la compilation (avant l'include) :

```cpp
#define TELEGRAM_BOT_TOKEN  "123456:ABC..."
#define TELEGRAM_CHAT_ID    "123456789"
#include <TelegramSender.h>
```

## Prérequis

- **NTP obligatoire** pour la validation BearSSL : `configTime()` avant le premier `sendMessage()`, sinon `TELEGRAM_NO_CLOCK`.
- WiFi connecté (`TELEGRAM_NO_WIFI` sinon), heap ≥ `TELEGRAM_MIN_HEAP` (`TELEGRAM_LOW_HEAP` sinon).
- `TELEGRAM_MIN_HEAP` (défaut `12000`) : seuil de heap minimal pour un envoi, surchargeable à la compilation (`-DTELEGRAM_MIN_HEAP=8000` pour les devices à RAM serrée, ex. le coulométre dont le heap de repos est ~10 Ko).
- **`setParseMode()`** : le mode par défaut `"Markdown"` interprète `_`, `*`, `` ` ``… Si votre message contient ces caractères (ex. `moyenne_1h`), Telegram répond HTTP 400 : passez `""` (texte brut) ou échappez-les vous-même.

## Exemple

Voir `examples/TelegramDemo/TelegramDemo.ino` (compile avec `platformio run` dans `examples/TelegramDemo`).

## Licence

GPL-3.0-only — Olivier FOURNET, https://github.com/Fo170
