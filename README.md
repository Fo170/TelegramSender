# TelegramSender

**Version 1.3.0** — https://github.com/Fo170/TelegramSender

Envoi bas niveau de messages Telegram pour **ESP8266 et ESP32** (header-only, fonction synchrone `sendMessage()` et polling `getUpdatesBody()`).

## Caractéristiques

- `int sendMessage(const String&)` → `int` (`TelegramError`, 0 = succès), **aucune allocation heap en retour**.
- **Session TLS persistante** (v1.3.0) : `WiFiClientSecure` membre de la classe, un seul handshake TLS (mbedTLS/BearSSL) à la connexion, puis requêtes **HTTP/1.1 keep-alive** réutilisées. Plus de handshake recréé à chaque appel → heap stable, plus de fragmentation (l'ancien comportement provoquait des `abort()`/reboot sur ESP32 après quelques heures).
- `String getUpdatesBody(unsigned long offset, int& httpCode)` : corps JSON brut de `getUpdates` sur la **même** session (polling efficace, pas de reconnexion à chaque poll).
- `void endSession()` : ferme explicitement la session TLS (le handshake sera refait à la prochaine requête), utile en cas de perte WiFi prolongée.
- Envoi en **POST** (pas de limite d'URL, message hors ligne de requête) ; le succès vérifie le champ `"ok":true` de la réponse, pas seulement le code HTTP.
- TLS par certificat : `TELEGRAM_ROOT_CA` (Go Daddy Root CA - G2) en PROGMEM, validation BearSSL (ESP8266) / mbedTLS (ESP32). `setCertValidation(false)` → `setInsecure()` (uniquement si l'horloge n'est jamais synchronisée).
- Configuration : `setEnabled(bool)` (défaut `true`), `setTestMode(bool)` (défaut `false`, affiche sur Serial au lieu d'envoyer), `setCertValidation(bool)` (défaut `true`), `setParseMode(const String&)` (défaut `"Markdown"` ; `""` = texte brut).
- `static const char* errorText(int code)` : texte des codes retour, sans dépendance externe.
- Gestion évoluée (queue, retry, throttling) : **hors de cette lib**, à faire dans une librairie de plus haut niveau.

## Nouveautés v1.3.0

| Avant (≤ 1.2.0) | Depuis 1.3.0 |
|---|---|
| `WiFiClientSecure` + `HTTPClient` **locaux**, recréés (handshake TLS complet) à chaque appel | `WiFiClientSecure` **membre** : un seul handshake à la connexion, réutilisé (keep-alive) |
| HTTP géré par `HTTPClient` | Requête HTTP/1.1 brute sur la session TLS (`_httpRequest`), fermeture/recréation automatique si `Connection: close` ou coupure |
| `sendMessage()` seul point d'entrée | Ajout de `getUpdatesBody(offset, httpCode)` (GET `getUpdates` sur la même session) |
| — | Ajout de `endSession()` (fermeture explicite de la session) |

**Impact mémoire** : les buffers mbedTLS (~10-30 Ko selon la plateforme) restent **alloués en permanence** pour la durée de la session, au lieu d'être alloués/libérés à chaque appel. C'est le compromis voulu : heap stable, un seul handshake. Deux instances = deux sessions (buffers dupliqués).

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
