# TelegramSender

Envoi bas niveau de messages Telegram pour **ESP8266** (header-only, une seule fonction synchrone `sendMessage()`).

https://github.com/Fo170/TelegramSender

## Caractéristiques

- `int sendMessage(const String&)` → `int` (`TelegramError`, 0 = succès), **aucune allocation heap en retour**.
- TLS par certificat : `TELEGRAM_ROOT_CA` (Go Daddy Root CA - G2) en PROGMEM, validation BearSSL. `setCertValidation(false)` → `setInsecure()` (uniquement si l'horloge n'est jamais synchronisée).
- Configuration : `setEnabled(bool)` (défaut `true`), `setTestMode(bool)` (défaut `false`, affiche sur Serial au lieu d'envoyer), `setCertValidation(bool)` (défaut `true`).
- `static const char* errorText(int code)` : texte des codes retour, sans dépendance externe.
- Gestion évoluée (queue, retry, throttling) : **hors de cette lib**, à faire dans une librairie de plus haut niveau.

## Token et chat ID

**Jamais en clair dans le code.** Définir à la compilation (avant l'include) :

```cpp
#define TELEGRAM_BOT_TOKEN  "123456:ABC..."
#define TELEGRAM_CHAT_ID    "123456789"
#include <TelegramSender.h>
```

## Prérequis

- **NTP obligatoire** pour la validation BearSSL : `configTime()` avant le premier `sendMessage()`, sinon `TELEGRAM_NO_CLOCK`.
- WiFi connecté (`TELEGRAM_NO_WIFI` sinon), heap ≥ 12000 (`TELEGRAM_LOW_HEAP` sinon).

## Exemple

Voir `examples/TelegramDemo/TelegramDemo.ino` (compile avec `platformio run` dans `examples/TelegramDemo`).

## Licence

GPL-3.0-only — Olivier FOURNET, https://github.com/Fo170
