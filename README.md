# CAN_DCC_RMT-Booster_DRV8874

Firmware du booster de voie ESP32 basé sur le pont en H DRV8874. La carte
régénère le signal DCC reçu sur son CAN local, protège sa propre sortie de
voie, mesure courant et tension, et transmet ses informations à l'EXCC associé
par UART.

## Fonctions

- Reçoit les blocs DCC et les marqueurs de cutout sur TWAI à 500 kbit/s.
- Régénère le signal DCC par RMT et commande le DRV8874.
- Coupe sa sortie si le DCC est perdu, si aucune identité de canton n'est
  attribuée, si l'EXCC cesse de répondre ou si une protection locale se
  déclenche.
- Mesure le courant et la tension de sa propre sortie.
- Transmet état, mesures et événements RailCom à l'EXCC par UART.
- Publie une télémétrie compacte sur son CAN local.

La télémétrie reçue d'un autre booster n'est pas utilisée pour commander cette
sortie. Chaque carte prend ses décisions à partir de ses propres protections
et des commandes reçues de son EXCC.

## Interfaces

### CAN local

| ID CAN | Sens | Fonction |
|---|---|---|
| `0x100` | CAN → booster | Bits DCC empaquetés dans leur ordre chronologique; premier bit dans le nibble haut |
| `0x101` | CAN → booster | Marqueurs de cutout local et global |
| `0x102` | Booster → CAN | `{ID canton, TYPE=4, courant/10, tension/100, état}` |

La télémétrie `0x102` est envoyée lors d'un changement d'état et avec un
heartbeat de 5 s. États : `0=OK`, `1=FAULT`, `2=OVERHEAT`, `3=OFF`.
L'identité de canton, de 1 à 252, est attribuée par le CC via l'EXCC; elle
n'est conservée qu'en RAM. `0xFF` signifie qu'aucune identité n'est attribuée.

### UART vers l'EXCC

UART1, 460800 bauds, 8N1, logique 3,3 V. Les trames utilisent le protocole
version 2, un numéro de séquence et un CRC-16/CCITT-FALSE.

| Type | Sens | Fonction |
|---|---|---|
| `0x01` | EXCC → booster | Heartbeat, période 100 ms |
| `0x02` | EXCC → booster | Autorisation de puissance ON/OFF |
| `0x03` | EXCC → booster | Profil N ou HO |
| `0x04` | Booster → EXCC | Accusé de réception |
| `0x05` | EXCC → booster | Attribution ou retrait de l'ID canton |
| `0x10` | Booster → EXCC | Mesures et état |
| `0x11` | Booster → EXCC | Événement RailCom |

L'EXCC doit utiliser le même protocole UART version 2. Les masses doivent
être communes; TX et RX sont croisés.

## Protections locales

- Au démarrage, la sortie est désactivée jusqu'à réception d'un ID valide et
  d'une commande d'autorisation.
- Plus de 250 ms sans heartbeat UART valide : sortie coupée et identité
  effacée.
- Plus de 500 ms sans bloc DCC valide : sortie à OFF.
- Courant maximal : 2000 mA. Tension minimale : 9000 mV.
- Après un défaut électrique, une tentative locale est faite après 100 ms si
  les mesures sont revenues sous les seuils.
- La durée de cutout régénérée est réglée à 488 µs dans `src/Config.h`.

## Brochage

| Fonction | GPIO | Détail |
|---|---:|---|
| CAN TX / RX | 5 / 4 | TWAI natif, 500 kbit/s |
| UART TX / RX vers EXCC | 17 / 16 | UART1, TX/RX croisés |
| DRV8874 IN1 / IN2 | 32 / 33 | RMT canal 5; IN2 inversé par matrice GPIO |
| DRV8874 nSLEEP | 27 | Niveau haut actif; PMODE strapé en mode PWM |
| Mesure courant IPROPI | 34 | ADC, résistance 3,6 kΩ |
| Mesure tension VRAIL | 35 | ADC, pont 68 kΩ / 10 kΩ |
| Entrée RailCom isolée | 36 | RX UART2 |
| WS2812 | 26 | Trois LEDs, RMT canal 7 |
| Digipot SDA / SCL | 21 / 22 | MCP4551/4561 |

## LEDs

| LED | Signification |
|---|---|
| 1 | DCC présent : vert clignotant; DCC absent : rouge clignotant |
| 2 | Sortie active : vert; défaut : rouge; sortie OFF : éteinte |
| 3 | Activité CAN récente : vert; aucune activité récente : éteinte |

## Compilation

Projet PlatformIO : `pio run` pour compiler, `pio run -t upload` pour
téléverser et `pio device monitor -b 115200` pour ouvrir le moniteur série.
Les broches sont dans `src/Pins.h`; les réglages sont dans `src/Config.h`.

## Licence

Voir [LICENSE](LICENSE).
