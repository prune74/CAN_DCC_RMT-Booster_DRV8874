# CAN_DCC_RMT-Booster_DRV8874

Firmware du booster DCC externe CFC, basé sur ESP32 et DRV8874. Le booster reçoit le DCC sur un **CAN dédié**, régénère le signal par RMT et échange sa supervision avec l'EXCC sur UART.

> **État du projet :** les firmwares Booster et EXCC compilent avec le protocole UART version 2. Les essais matériels ne sont pas encore validés. Garder la voie débranchée jusqu'à la validation des sécurités au banc.

## Architecture

```text
Convertisseur DCC ── CAN DCC, 500 kbit/s ──> ESP32 Booster ──> DRV8874 ──> voie
                                               │
                                               ├── RailCom et mesures
                                               └── UART 460800 bauds
                                                      │
CC ── CAN Command ──> ESP32 EXCC ────────────────────┘
```

- **Booster** : reçoit les blocs DCC, génère la sortie DCC, mesure courant/tension et capture RailCom.
- **EXCC** : transmet au Booster les commandes de puissance, le profil de voie et l'identité du canton; reçoit les mesures et événements RailCom.
- **CC** : reçoit son ID de canton de l'ERM. Cet ID est relayé à l'EXCC, puis au Booster.
- Le CAN DCC reste distinct du CAN Command. Les blocs DCC ne passent pas par l'UART.

## Sécurité de démarrage

- Au démarrage, le Booster n'a pas d'ID attribué et sa sortie est désactivée.
- L'ID reçu du CC doit être compris entre `0` et `252`. `0xFF` signifie « non attribué »; `253` et `254` sont réservés.
- L'ID est conservé en RAM, pas en mémoire permanente. L'EXCC le transmet et le rafraîchit; le Booster l'acquitte.
- L'attribution d'un ID ne met pas la voie sous tension. Une commande ON explicite est requise après validation de l'ID.
- Une perte du heartbeat UART pendant 250 ms désactive la sortie et efface l'ID. Un changement d'ID la désactive également.
- Sans bloc DCC valide pendant 500 ms, le Booster reste ou passe à l'état OFF.
- Les protections logicielles sont configurées à 2 000 mA et 9 000 mV. Ces seuils et les mesures analogiques doivent être vérifiés avec le matériel réel.

**Ne pas connecter la voie pour les premiers essais.** Vérifier d'abord l'attribution d'ID, les timeouts, les mesures et la coupure de sortie au banc.

## Matériel et brochage

| Fonction | GPIO | Constante | Remarque |
|---|---:|---|---|
| CAN DCC TX / RX | 5 / 4 | `PIN_CAN_TX` / `PIN_CAN_RX` | TWAI ESP32, 500 kbit/s |
| UART TX / RX vers EXCC | 17 / 16 | `PIN_UART_TX` / `PIN_UART_RX` | UART1, 460800 bauds |
| DRV8874 IN1 / IN2 | 32 / 33 | `PIN_DRV_IN1` / `PIN_DRV_IN2` | Sortie DCC biphasée, RMT canal 5 |
| DRV8874 nSLEEP | 27 | `PIN_DRV_NSLEEP` | Niveau haut = driver actif |
| Mesure IPROPI | 34 | `PIN_DRV_IPROPI` | Entrée analogique |
| Mesure VRAIL | 35 | `PIN_VRAIL` | Entrée analogique via pont diviseur |
| Entrée RailCom isolée | 36 | `PIN_RAILCOM_ISO` | UART2 RX, entrée uniquement |
| WS2812 | 26 | `PIN_WS2812` | 3 LEDs, RMT canal 7 |
| Digipot I2C SDA / SCL | 21 / 22 | `PIN_I2C_SDA` / `PIN_I2C_SCL` | MCP4551/4561 |

Le firmware attend un DRV8874 configuré en mode PWM. Vérifier le strap PMODE et le câblage selon le schéma de la carte. GPIO 36 est une entrée uniquement.

### Câblage UART Booster–EXCC

Utiliser une liaison logique **3,3 V**, croiser TX/RX et relier les masses :

| Booster | EXCC |
|---|---|
| GPIO 17, TX | GPIO 17, RX |
| GPIO 16, RX | GPIO 16, TX |
| GND | GND |

Ne pas confondre cette liaison avec le port USB/moniteur série du Booster.

## Protocole UART

Les deux firmwares doivent utiliser la même version. Le protocole actuel est **version 2**, à 460800 bauds, 8N1 :

```text
A5 5A | version:u8 | type:u8 | séquence:u16 LE | longueur:u8 | payload | CRC:u16 LE
```

Le CRC est CRC-16/CCITT-FALSE. Types utilisés :

| Type | Sens | Fonction |
|---:|---|---|
| `0x01` | EXCC → Booster | Heartbeat, toutes les 100 ms |
| `0x02` | EXCC → Booster | Puissance OFF/ON (`0`/`1`) |
| `0x03` | EXCC → Booster | Profil N/HO (`0`/`1`) |
| `0x04` | Booster → EXCC | ACK de commande |
| `0x05` | EXCC → Booster | ID canton (`0..252`) ou retrait (`0xFF`) |
| `0x10` | Booster → EXCC | Échantillon courant/tension/état, environ 1 kHz |
| `0x11` | Booster → EXCC | Événement RailCom |

L'ACK contient le type de commande, son numéro de séquence et son statut. Le Booster refuse l'activation sans heartbeat valide ou sans ID acquitté. Les versions incompatibles ne doivent pas être utilisées ensemble.

## CAN DCC

Le bus DCC utilise le TWAI natif à **500 kbit/s**.

### Trames reçues

| ID | Données | Traitement |
|---:|---|---|
| `0x100` | `data[0]` = nombre de bits (1–16), puis nibbles `(bit << 1) \| phase` | Mise en FIFO, puis génération RMT |
| `0x101` | Indicateurs de cutout local/global | Cutout et capture RailCom |
| `0x102` | `{ ID, TYPE, mA/10, mV/100, état }` | Surveillance des défauts des autres boosters |

### Trames émises

| ID | Données | Périodicité |
|---:|---|---|
| `0x102` | `{ ID canton, TYPE=1, mA/10, mV/100, état }` | 1 seconde, seulement après attribution d'un ID |
| `0x103` | Adresse RailCom, little-endian | À la détection |

Le profil de voie est commandé par l'EXCC sur UART; ce firmware ne traite pas de commande CAN `0x120`.

## Configuration logicielle

Les broches sont définies dans [`src/Pins.h`](src/Pins.h), les paramètres dans [`src/Config.h`](src/Config.h).

| Paramètre | Valeur actuelle | Fonction |
|---|---:|---|
| `CFG_BOOSTER_TYPE` | `1` | Type B dans la télémétrie |
| `CFG_CAN_BITRATE` | `500000` | Débit CAN DCC |
| `CFG_FAILSAFE_MS` | `500` | Timeout sans blocs DCC |
| `CFG_MAX_CURRENT_MA` | `2000` | Seuil de surintensité |
| `CFG_MIN_VOLTAGE_MV` | `9000` | Seuil de sous-tension |
| `CFG_CUTOUT_US` | `488` | Fenêtre de capture RailCom |
| `DIGIPOT_N` / `DIGIPOT_HO` | `30` / `23` | Valeurs du digipot pour N / HO |

Les valeurs du digipot et les conversions IPROPI/VRAIL dépendent du matériel et doivent être calibrées. L'ID du Booster n'est plus une constante à modifier dans `Config.h`.

## Compilation et téléversement

Prérequis : PlatformIO, ESP32 Arduino Core et les dépendances déclarées dans [`platformio.ini`](platformio.ini).

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

Le moniteur USB utilise 115200 bauds. Au démarrage, le Booster affiche qu'il attend l'ID du CC; après attribution, il affiche par exemple `ID_B_12`.

Avant un essai UART, téléverser les versions compatibles du Booster et de l'EXCC. Pour les premières mises sous tension, laisser la voie déconnectée.

## Validation au banc

1. Vérifier la compilation et téléverser les deux firmwares UART version 2.
2. Garder la sortie voie déconnectée; connecter uniquement les alimentations de contrôle et les liaisons nécessaires au test.
3. Vérifier que le Booster démarre sans ID et reste OFF.
4. Vérifier que l'ID affiché par le Booster est identique à l'ID canton attribué au CC.
5. Vérifier qu'un ID absent/invalide, un changement d'ID ou une perte du heartbeat maintient la sortie OFF.
6. Vérifier les mesures courant/tension et la coupure de sécurité avec une méthode de banc adaptée.
7. Contrôler les signaux IN1/IN2 et la sortie au moyen d'instruments adaptés avant tout raccordement de voie.

Les essais matériels, la validation RailCom à l'oscilloscope et la validation finale des seuils restent à faire.

## LEDs

| LED | Indication |
|---|---|
| 1 | Vert clignotant si des blocs DCC récents sont reçus; rouge clignotant sinon |
| 2 | Vert à l'état OK, rouge en FAULT, éteinte en OFF/RETRY |
| 3 | Verte pendant 3 secondes après une trame CAN reçue |

La LED 3 indique une activité CAN récente, pas à elle seule la validité complète du bus.

## Structure du firmware

| Fichier | Rôle |
|---|---|
| `BoosterCore.*` | Sécurité, état, flux DCC et contrôle du driver |
| `BoosterUartLink.*` | Heartbeat, attribution d'ID, commandes et mesures UART |
| `UartProtocol.h` | Format des trames UART version 2 |
| `CanBooster.*` / `BoosterCanConfig.h` | Interface et configuration du CAN DCC |
| `DccCanRx.*` | Décodage des trames CAN et surveillance globale |
| `DccRegen.*` / `Drv8874.*` | Génération DCC RMT et pilotage DRV8874 |
| `RailCom.*` | Capture RailCom sur UART2 pendant le cutout |
| `Telemetry.*` | Télémétrie CAN `0x102`/`0x103` |
| `TensionControl.*` | Commande du digipot par I2C |
| `StatusLeds.*` | État des trois LEDs WS2812 |

## Licence et références

Projet propriétaire destiné à l'usage interne des Chemins de Fer du Chablais (CFC).

- [Convertisseur DCC/CAN](https://github.com/prune74/DCC_CAN_RMT-Booster)
- [EXCC](https://github.com/prune74/Extension_CantonController)
- [CanUniversal](https://github.com/pascallanger/CanUniversal)
- [Ws2812Universal](https://github.com/prune74/Ws2812Universal)
