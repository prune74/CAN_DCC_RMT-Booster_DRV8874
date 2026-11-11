# CAN_DCC_RMT-Booster_DRV8874

**Booster DCC avec RailCom et gestion de tension pour Chemins de Fer du Chablais (CFC)**

Ce projet implémente un **booster DCC** basé sur un **ESP32** avec gestion du protocole **RailCom HF**, communication **CAN bus** (TWAI natif), et **réglage de la tension voie** via digipot (MCP4551/4561). Il fait partie de l'écosystème **DCC_CAN_RMT-Booster** et s'intègre dans l'architecture multi-cartes des **Chemins de Fer du Chablais (CFC)**.

---

## 📌 Description

Ce booster **réceptionne les trames CAN** émises par le **convertisseur DCC_CAN_RMT-Booster** (qui capture le signal DCC via RMT), **régénère le signal DCC** sur la voie via RMT, **capture les adresses RailCom HF** pendant les cutouts pour les retransmettre sur le bus CAN, et **gère deux profils de tension** (HO 15V / N 12V) via un digipot.

### ✨ Fonctionnalités principales
- **Régénération DCC** via **RMT** (Remote Control) de l'ESP32
- **Lecture RailCom HF** pendant les cutouts (capture DMA I2S sur ADC1)
- **Communication CAN bus** à 500 kbit/s (TWAI natif, compatible avec le convertisseur)
- **Gestion de la sécurité** :
  - Surintensité (seuil configurable)
  - Sous-tension (seuil configurable)
  - Failsafe (timeout sur réception des trames CAN)
- **Télémétrie** : Envoi périodique de l'état du booster (ID, TYPE, courant, tension, état)
- **Signalisation** : 3 LEDs WS2812 pour l'état du booster
- **Gestion des profils de tension** : Commutation HO (15V) / N (12V) via digipot MCP4551/4561
- **Architecture multi-boosters** : Chaque booster a un ID unique (1..32) sur le bus CAN

---

## 🏗️ Architecture Matérielle

### 📌 Brochage ESP32 (par défaut)

| Fonction | GPIO | Description | Constante (Pins.h) |
|----------|------|-------------|---------------------|
| **CAN TX** | 5 | TWAI natif (bus CAN) | `PIN_CAN_TX` |
| **CAN RX** | 4 | TWAI natif (bus CAN) | `PIN_CAN_RX` |
| **DCC PH** | 32 | Signal DCC (RMT) → PH du DRV8874 | `PIN_DCC_PH` |
| **DRV EN** | 33 | Autorisation sortie (0 = coast/arrêt) | `PIN_DRV_EN` |
| **DRV NSLEEP** | 27 | Veille du driver (1 = actif) | `PIN_DRV_NSLEEP` |
| **IPROPI** | 34 | ADC - Mesure du courant voie | `PIN_IPROPI` |
| **VRAIL** | 35 | ADC - Mesure de la tension voie | `PIN_VRAIL` |
| **RailCom ADC** | 36 | ADC1_CH0 - Capture RailCom HF | `PIN_RAILCOM_ADC` |
| **WS2812** | 26 | Strip 3 LEDs d'état | `PIN_WS2812` |
| **I2C SDA** | 21 | Bus I2C pour digipot | `PIN_I2C_SDA` |
| **I2C SCL** | 22 | Bus I2C pour digipot | `PIN_I2C_SCL` |

### 🔌 Composants requis
- **ESP32** (testé avec ESP32-DOIT-DEVKIT-V1)
- **DRV8874** : Driver de puissance pour la voie DCC (2A max, PMODE=0)
- **Digipot MCP4551/4561** : Réglage de la tension voie (I2C)
- **Circuit de conditionnement RailCom** : 
  - Optocoupleur TLP2361 (isolement galvanique)
  - Amplificateur différentiel TLV2372
  - Comparateur TLV3501
  - Circuit RC pour adaptation du signal HF
- **Pont diviseur de tension** : Pour la mesure de la tension voie (VRAIL)
- **Shunt ou capteur de courant** : Pour la mesure du courant (IPROPI)
- **Strip WS2812** (3 LEDs) : Indicateur d'état

---

## 🛠️ Configuration

### 📁 Organisation des fichiers de configuration

Le projet utilise **deux fichiers distincts** pour une meilleure séparation des responsabilités :

| Fichier | Rôle | Contenu |
|---------|------|---------|
| **`src/Pins.h`** | **Broches matérielles** | Définitions des GPIOs (ex: `PIN_CAN_TX = 5`) |
| **`src/Config.h`** | **Valeurs de configuration** | Paramètres logiciels (ex: `CFG_BOOSTER_ID = 1`) |

**`Pins.h`** contient **uniquement les broches** :
```cpp
// --- Bus CAN ---
static const int PIN_CAN_TX = 5;
static const int PIN_CAN_RX = 4;

// --- DRV8874 ---
static const int PIN_DCC_PH = 32;
static const int PIN_DRV_EN = 33;
static const int PIN_DRV_NSLEEP = 27;
static const int PIN_IPROPI = 34;
static const int PIN_VRAIL = 35;

// --- RailCom ---
static const int PIN_RAILCOM_ADC = 36;  // ADC1_CH0

// --- Canaux RMT ---
static const int RMT_DCC_CHANNEL = 5;
static const int RMT_WS2812_CHANNEL = 7;

// --- LED d'état ---
static const int PIN_WS2812 = 26;
static const int WS2812_COUNT = 3;

// --- Digipot (MCP4551/4561) ---
static const int PIN_I2C_SDA = 21;
static const int PIN_I2C_SCL = 22;
```

**`Config.h`** contient **uniquement les valeurs** :
```cpp
// --- Identité sur le bus CAN (trame 0x102) ---
static const uint8_t CFG_BOOSTER_ID = 1;     // ID_B_01 (1..32, UNIQUE sur le bus)
static const uint8_t CFG_BOOSTER_TYPE = 1;     // TYPE B = booster (ID_B_xx)

// --- Bus CAN ---
static const uint32_t CFG_CAN_BITRATE = 500000; // 500 kbit/s

// --- RailCom HF ---
static const uint16_t CFG_RAILCOM_ADC_ZERO = 2048; // Zéro HF à calibrer (12 bits)

// --- Sécurité ---
static const uint32_t CFG_FAILSAFE_MS = 500;   // Timeout sans trame 0x100 → OFF
static const uint16_t CFG_MAX_CURRENT_MA = 2000; // Seuil coupure (mA)
static const uint16_t CFG_MIN_VOLTAGE_MV = 9000; // Sous-tension → FAULT
static const uint32_t CFG_CUTOUT_US = 454;   // Cutout nominal régénéré (RailCom)

// --- LED d'état ---
static const uint8_t CFG_WS2812_BRIGHTNESS = 40; // Luminosité (0-255)

// --- Digipot (MCP4551/4561) ---
static constexpr uint8_t DIGIPOT_ADDR = 0x2E;   // Adresse I2C du digipot
static constexpr uint8_t DIGIPOT_REG = 0x00;    // Registre du wiper
static constexpr uint8_t DIGIPOT_HO = 23;       // Profil HO (15V) - valeur déjà calibrée
static constexpr uint8_t DIGIPOT_N = 30;        // Profil N (12V) - valeur déjà calibrée
```

---

## 📦 Structure du Projet

```
src/
├── Pins.h                  # 🆕 Définition des broches matérielles (GPIO)
├── Config.h                # 🆕 Valeurs de configuration (paramètres logiciels)
├── Types.h                 # Types communs (BoosterState, BoosterTelemetry)
│
├── BoosterCanConfig.h      # Configuration CAN (provider CanUniversal)
│
├── CanBooster.h            # Interface CAN (déclarations)
├── CanBooster.cpp          # Interface CAN (implémentations)
│
├── DccCanRx.h              # Décodage des trames CAN DCC (déclarations)
├── DccCanRx.cpp            # Décodage des trames CAN DCC (implémentations)
│
├── DccRegen.h              # Régénération du signal DCC (déclarations)
├── DccRegen.cpp            # Régénération du signal DCC (implémentations)
│
├── Drv8874.h               # Pilotage du DRV8874 (déclarations)
├── Drv8874.cpp             # Pilotage du DRV8874 (implémentations)
│
├── RailCom.h               # Lecture et décodage RailCom HF (déclarations)
├── RailCom.cpp             # Lecture et décodage RailCom HF (implémentations)
│
├── TensionControl.h        # 🆕 Gestion du digipot (déclarations)
├── TensionControl.cpp      # 🆕 Gestion du digipot (implémentations)
│
├── BoosterCore.h           # Cœur du booster (déclarations)
├── BoosterCore.cpp         # Cœur du booster (implémentations)
│
├── Telemetry.h             # Télémétrie (déclarations)
├── Telemetry.cpp           # Télémétrie (implémentations)
│
├── StatusLeds.h            # Gestion des LEDs WS2812 (déclarations)
├── StatusLeds.cpp          # Gestion des LEDs WS2812 (implémentations)
│
└── main.cpp                # Point d'entrée
```

**Architecture logicielle** :
- **Séparation claire** entre déclarations (`.h`) et implémentations (`.cpp`)
- **`Pins.h`** et **`Config.h`** centralisent toute la configuration
- Chaque classe a son propre fichier `.cpp` pour les implémentations

---

## 📡 Protocole CAN (Compatible CFC)

Ce booster respecte le **protocole multi-cartes CFC** défini dans le projet [DCC_CAN_RMT-Booster](https://github.com/prune74/DCC_CAN_RMT-Booster).

### 🔹 Trames reçues (depuis le convertisseur)

| ID (hex) | Description | Format | Action du booster |
|----------|-------------|--------|-------------------|
| **0x100** | Bloc de bits DCC | `data[0]` = N bits (1..16), puis nibbles `(bit << 1) | phase` | Stocke dans la FIFO, régénère via RMT |
| **0x101** | Cutout réel | `{ local, global }` | Régénère un cutout de 454 µs + **capture RailCom** |
| **0x120** | Commande profil tension | `data[0]` = 0 (N) ou 1 (HO) | **Change le profil via `setTrackProfile()`** |

### 🔹 Trames émises (vers le bus CAN)

| ID (hex) | Description | Format | Périodicité |
|----------|-------------|--------|-------------|
| **0x102** | Télémétrie | `{ ID, TYPE, mA/10, mV/100, état }` | Toutes les 1 seconde |
| **0x103** | Adresse RailCom | `{ addr_LSB, addr_MSB }` (little-endian) | À la détection |

### 🔹 États du booster (trame 0x102, `data[4]`)

| Valeur | État | Description | Nom CFC |
|--------|------|-------------|---------|
| 0 | **OK** | Fonctionnement normal | `BOOSTER_OK` |
| 1 | **FAULT** | Surintensité ou sous-tension | `BOOSTER_FAULT` |
| 2 | **OVERHEAT** | Surchauffe | `BOOSTER_OVERHEAT` |
| 3 | **OFF** | DCC perdu (failsafe) | `BOOSTER_OFF` |
| 4 | **RETRY** | Réservé (essais locaux) | `BOOSTER_RETRY` |

### 🔹 Nommage des modules (CFC)

Chaque module sur le bus CAN est identifié par un **couple (TYPE, ID)** :

| TYPE | Lettre | Nom | Rôle |
|------|--------|-----|------|
| 0 | C | `ID_C_00` | **Convertisseur** DCC → CAN (unique, ID=0) |
| 1 | B | `ID_B_01`..`ID_B_32` | **Booster** (ton projet) |
| 2 | S | `ID_S_xx` | Aiguillages (réservé) |
| 3 | D | `ID_D_xx` | Détection (réservé) |
| 5 | H | `ID_H_01`..`ID_H_nn` | Hub de distribution |

**⚠️ Important** : Chaque booster doit avoir un **`CFG_BOOSTER_ID` unique** (1..32) et **`CFG_BOOSTER_TYPE = 1`** pour être reconnu comme `ID_B_xx`.

---

## 🏗️ Architecture Logicielle

### Flux de données

```
Convertisseur (ID_C_00)          Booster (ID_B_01)
     │                                  │
     ▼                                  ▼
[RMT RX] → [Décodage DCC]     [CAN RX] → [DccCanRx]
     │                                  │
     ▼                                  ▼
[Trame 0x100/0x101] ──────────► [FIFO bits] → [RMT TX]
     │                                  │
     │                                  ▼
     │                           [RailCom Capture]
     │                                  │
     │                                  ▼
     │                           [TensionControl] ← [0x120]
     │                                  │
     │                                  ▼
     │                           [Trame 0x102/0x103]
     │                                  │
     └──────────────────────────────────┘
                    Bus CAN 500 kbit/s
```

### Rôle de chaque module

| Module | Rôle | Implementation |
|--------|------|----------------|
| **`CanBooster`** | Gère la réception CAN et l'envoi de télémétrie | `CanBooster.h/cpp` |
| **`BoosterCanConfig`** | Configuration du bus CAN (provider) | `BoosterCanConfig.h` |
| **`DccCanRx`** | Décode les trames CAN 0x100/0x101 | `DccCanRx.h/cpp` |
| **`BoosterCore`** | Orchestration (sécurité, cutout, RMT, tension) | `BoosterCore.h/cpp` |
| **`DccRegen`** | Régénère le signal DCC via RMT | `DccRegen.h/cpp` |
| **`Drv8874`** | Pilote le driver de puissance | `Drv8874.h/cpp` |
| **`RailCom`** | Capture et décode le RailCom HF | `RailCom.h/cpp` |
| **`TensionControl`** | 🆕 Gère le digipot pour les profils HO/N | `TensionControl.h/cpp` |
| **`Telemetry`** | Envoie l'état sur le bus CAN | `Telemetry.h/cpp` |
| **`StatusLeds`** | Gère les 3 LEDs WS2812 | `StatusLeds.h/cpp` |

---

## 🚀 Utilisation

### 📥 Dépendances

- **PlatformIO** (recommandé) ou **Arduino IDE**
- **Bibliothèques requises** :
  - [CanUniversal](https://github.com/pascallanger/CanUniversal) (v3+)
  - [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) (2.0.x+)
  - [Ws2812Universal](https://github.com/prune74/Ws2812Universal) (pour les LEDs)
  - **Wire.h** (inclus avec ESP32 Arduino Core) (pour le digipot)

### 🔌 Configuration PlatformIO

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
lib_deps =
    pascallanger/CanUniversal@^3.0.0
    prune74/Ws2812Universal
```

### ⚡ Flashage

1. Brancher l'ESP32 en USB
2. Configurer **`CFG_BOOSTER_ID`** dans `Config.h` (doit être **unique** sur le bus CAN)
3. Vérifier que **`CFG_BOOSTER_TYPE = 1`** (TYPE B = booster)
4. Vérifier que **`DIGIPOT_HO`** et **`DIGIPOT_N`** sont correctement calibrés
5. Compiler et flasher :
   ```bash
   pio run -t upload
   ```

### 📊 Monitoring

Ouvrir le moniteur série (115200 bauds) :
```
CAN_DCC_RMT-BoosterDRV8874 — ID_B_01 (CFC)
```

---

## 🎛️ Gestion des Profils de Tension

Le booster gère **deux profils de tension** via un **digipot MCP4551/4561** :

| Profil | Tension | Valeur digipot | Constante |
|--------|---------|----------------|-----------|
| **N** | 12V | 30 | `DIGIPOT_N` |
| **HO** | 15V | 23 | `DIGIPOT_HO` |

### Commande CAN pour changer de profil

| ID (hex) | Description | Format | Action |
|----------|-------------|--------|--------|
| **0x120** | Changer de profil | `data[0]` = 0 (N) ou 1 (HO) | `booster.setTrackProfile(data[0])` |

### Exemple de réception de commande

Dans `CanBooster::poll()` (ou similaire) :
```cpp
void CanBooster::poll() {
    if (!bus->isValid()) return;
    CanMsg msg;
    while (bus->receive(msg)) {
        // ... traitement des trames DCC ...
        
        // Gestion de la commande de profil
        if (msg.id == 0x120 && msg.dlc >= 1) {
            bool hoProfile = (msg.data[0] == 1);
            core->setTrackProfile(hoProfile);
        }
    }
}
```

---

## 🔌 Câblage RailCom

### Schéma de conditionnement (KiCad)

Ton projet inclut un **circuit de conditionnement RailCom** complet avec :

1. **Filtrage différentiel** (TLV2372 - U1A)
   - Entrées Rail A/B avec protection (BAT54J + BZV55B3V3)
   - Filtrage RC (R1/R2 = 4.7kΩ, C1/C2 = 100nF)
   - Amplification différentielle (gain = 151)

2. **Comparateur** (TLV3501 - U2)
   - Seuil défini par pont diviseur (R5 = 150kΩ, R6 = 56kΩ → ~0.89V)
   - Sortie numérique 0/3.3V

3. **Isolement galvanique** (TLP2361 - U3)
   - LED côté RailCom (anode via R7 = 1kΩ)
   - Phototransistor côté ESP32 (collecteur → RAILCOM_ISO)
   - Pull-up R8 = 10kΩ sur RAILCOM_ISO

4. **Signal final**
   - **RAILCOM_ISO** → **`PIN_RAILCOM_ADC`** (ex: GPIO 36)
   - Signal **actif bas** (LOW pendant trame RailCom)

### ⚠️ Points critiques

| Élément | Valeur | Rôle |
|---------|--------|------|
| R1/R2 | 4.7kΩ | Limitation du courant d'entrée |
| C1/C2 | 100nF | Filtrage HF (passe le RailCom 250 kHz) |
| D1/D2 | BAT54J | Protection contre les inversions |
| D3/D4 | BZV55B3V3 | Protection contre les surtensions (>3.3V) |
| TLV2372 | Amp-op | Amplification différentielle |
| TLV3501 | Comparateur | Conversion analogique → numérique |
| TLP2361 | Optocoupleur | Isolement galvanique |
| R8 | 10kΩ | Pull-up sur RAILCOM_ISO |

### 🎯 Branchement final

| Broche du schéma | Broche ESP32 | Constante (Pins.h) |
|-------------------|--------------|---------------------|
| **RAILCOM_ISO** | GPIO 36 | `PIN_RAILCOM_ADC` |
| **+3V3** | 3.3V | - |
| **GND** | GND | - |

---

## 🔍 Dépannage

### ⚠️ Problèmes courants

| Symptôme | Cause possible | Solution |
|----------|----------------|----------|
| **Pas de signal DCC en sortie** | RMT mal configuré | Vérifier `PIN_DCC_PH` et `RMT_DCC_CHANNEL` |
| **Pas de réception CAN** | Broches TX/RX inversées | Vérifier `PIN_CAN_TX` (GPIO 5) et `PIN_CAN_RX` (GPIO 4) |
| **RailCom non détecté** | Signal non conditionné | Vérifier le circuit opto + RC, calibrer `CFG_RAILCOM_ADC_ZERO` |
| **Surchauffe DRV8874** | Courant trop élevé | Vérifier `CFG_MAX_CURRENT_MA`, refroidissement, et le câblage |
| **Erreurs CAN** | Bitrate incompatible | Vérifier `CFG_CAN_BITRATE` (500000 = 500 kbit/s) |
| **Failsafe déclenché** | Pas de trame 0x100 | Vérifier la connexion CAN avec le convertisseur |
| **Adresse RailCom incorrecte** | Bruit sur le signal | Ajouter un condensateur de découplage près de `PIN_RAILCOM_ADC` |
| **Tension voie incorrecte** | Digipot non initialisé | Vérifier `tensionControl.begin()` dans `setup()` |

### 📌 Calibrage

#### 1. Zéro RailCom (`CFG_RAILCOM_ADC_ZERO`)
```cpp
// Dans setup(), avant de démarrer :
uint16_t zero = analogRead(PIN_RAILCOM_ADC);
// Puis mettre cette valeur dans Config.h
static const uint16_t CFG_RAILCOM_ADC_ZERO = zero; // Ex: 2048
```

#### 2. Seuil de courant (`CFG_MAX_CURRENT_MA`)
- Mesurer le courant maximal supporté par ton alimentation
- Régler à **90%** de cette valeur pour éviter les coupures intempestives
- Exemple : Alimentation 3A → `CFG_MAX_CURRENT_MA = 2700`

#### 3. Seuil de tension (`CFG_MIN_VOLTAGE_MV`)
- Mesurer la tension minimale acceptable pour ton système
- Exemple : Alimentation 12V → `CFG_MIN_VOLTAGE_MV = 10000` (10V)

#### 4. Digipot (MCP4551/4561)
- Les valeurs **DIGIPOT_HO = 23** et **DIGIPOT_N = 30** sont **déjà calibrées**
- Pas de modification nécessaire sauf si tu changes de digipot ou de circuit

#### 5. Brightness LED (`CFG_WS2812_BRIGHTNESS`)
- Régler entre 0 (éteint) et 255 (max)
- Valeur recommandée : 40-80

---

## 📄 Signalisation LED

Les **3 LEDs WS2812** (GPIO 26) indiquent l'état du booster :

| LED | Couleur | État | Signification |
|-----|---------|------|----------------|
| **LED 1** | 🟢 Vert clignotant (1,5s) | DCC actif | **Flux DCC présent** |
| **LED 1** | 🔴 Rouge clignotant (2s) | DCC inactif | **Failsafe (OFF)** |
| **LED 2** | 🟢 Vert fixe | OK | **Booster opérationnel** |
| **LED 2** | 🔴 Rouge fixe | FAULT | **Surintensité/sous-tension** |
| **LED 2** | ⚫ Éteint | OFF/RETRY | **Booster à l'arrêt** |
| **LED 3** | 🟢 Vert discret | Bus valide | **CAN fonctionnel** |

---

## 📄 Licence

Ce projet est **propriétaire** et destiné à un usage interne aux **Chemins de Fer du Chablais (CFC)**.

---

## 📚 Documentation Complémentaire

- **Convertisseur amont** : [DCC_CAN_RMT-Booster](https://github.com/prune74/DCC_CAN_RMT-Booster)
- **Hub de distribution** : [DCC_HUB_DISTRIB-Booster](https://github.com/prune74/DCC_HUB_DISTRIB-Booster)
- **Bibliothèque CAN** : [CanUniversal](https://github.com/pascallanger/CanUniversal)
- **Bibliothèque LED** : [Ws2812Universal](https://github.com/prune74/Ws2812Universal)

---

## 🙏 Remerciements

- Inspiré du projet **CAN_DCC-Booster** de la communauté DCC++
- Adapté pour l'architecture **ESP32 + DRV8874 + RailCom HF + Digipot**
- Compatible avec l'écosystème **CFC** (Chemins de Fer du Chablais)

---

## 📞 Support

Pour toute question ou problème, contacter l'équipe technique CFC.
