# CAN_DCC_RMT-Booster_DRV8874

> Suivi : [fiche Booster](../../Suivi/Cartes/SUIVI_BOOSTER_DRV8874.md) · [avancement général](../../Suivi/ETAT_AVANCEMENT.md)

Firmware ESP32 du booster de voie DRV8874. Il régénère localement le signal
DCC reçu du convertisseur, commande le pont en H, applique le profil transmis
par l’EXCC et surveille ses protections locales.

## CAN Booster

Le format DCC partagé est `v0.4.0`. Le flux utilise l’identifiant standard
`0x100` uniquement pour les enregistrements DCC et cutout. Chaque enregistrement
porte le marqueur `0xD2`, une séquence 16 bits, un type et une charge utile.
Les blocs DCC contiennent 1 à 16 bits empaquetés MSB-first; ils peuvent marquer
le début d’un paquet. Le cutout transporte sa durée mesurée dans le même flux.

Le Booster contrôle le format, la taille, les champs réservés et la continuité
de séquence. Une trame invalide ou une séquence manquante coupe la sortie;
la reprise attend un début de paquet reconnu. La sortie est également coupée
après 50 ms sans enregistrement valide. Ce délai est défini par le protocole,
mais reste à confirmer sur banc.

La télémétrie historique `0x102` est encore émise par ce firmware sur le CAN
local pour l’intégration actuelle avec l’EXCC/convertisseur. Son remplacement
par la supervision `0xB6` sur CAN Service reste à intégrer avant que le CAN
Booster ne transporte réellement `0x100` seulement.

## Référence DCC par UART

Le Booster reçoit le profil de LaBox par UART sous `DCC_REFERENCE (0x06)`,
relayé par l’EXCC. Le profil `v0.4.0` contient la version, la séquence, les
drapeaux `VALID`/`MAIN_ON`, les demi-bits, le tick et la longueur du préambule.
Le Booster applique les durées des demi-bits à son générateur RMT. Une
référence absente, invalide ou périmée après 1 500 ms interdit la sortie;
`MAIN OFF` provoque un arrêt normal. Le préambule reste informatif côté Booster,
car les bits DCC sont déjà reconstruits par le convertisseur.

Le firmware compile avec `CFC_CAN_Discovery_Protocol v0.4.0`. La réception
UART réelle, la circulation CAN complète et le comportement électrique sur la
voie n’ont pas encore été validés sur banc.

## Mesures et limites actuelles

Le capteur IPROPI mesure le courant du booster. GPIO35 mesure l’alimentation
du DRV8874, pas la tension différentielle sur les rails après le pont en H.
La mesure de tension de voie et son éventuelle calibration automatique ne sont
pas implémentées.

Le convertisseur transmet le cutout observé à l’entrée dans l’enregistrement
`0x100`; le Booster utilise cette durée pour sa fenêtre de coupure/capture
RailCom. La comparaison avec une référence de cutout LaBox reste inactive :
`0xB8` n’est pas encore mesuré sur la sortie HUB. Le réglage logiciel ne prouve
pas la durée électrique réelle sur les rails.

## UART EXCC ↔ Booster

UART1, 460800 bauds, 8N1, logique 3,3 V. Les trames utilisent le protocole
version 2, un numéro de séquence et un CRC-16/CCITT-FALSE.

| Type | Sens | Fonction |
|---|---|---|
| `0x01` | EXCC → Booster | Heartbeat, période 100 ms |
| `0x02` | EXCC → Booster | Autorisation de puissance ON/OFF |
| `0x03` | EXCC → Booster | Profil N ou HO |
| `0x04` | Booster → EXCC | Accusé de réception |
| `0x05` | EXCC → Booster | Attribution ou retrait de l’ID canton |
| `0x06` | EXCC → Booster | Référence DCC LaBox |
| `0x10` | Booster → EXCC | Mesures et état |
| `0x11` | Booster → EXCC | Événement RailCom |
| `0x12` | Booster → EXCC | Diagnostic DCC — défini, réception applicative à intégrer |

TX/RX sont croisés et les masses doivent être communes.

## Protections locales

- Sortie désactivée au démarrage jusqu’à attribution d’un ID et autorisation.
- Perte de heartbeat UART pendant 250 ms : sortie coupée et identité effacée.
- Perte/incohérence du flux CAN ou absence de profil frais : sortie coupée.
- Courant maximal configuré : 2 000 mA; sous-tension alimentation : 9 000 mV.
- Après un défaut électrique, une nouvelle tentative locale est possible après
  100 ms si les mesures sont revenues dans les seuils.

## Brochage principal

| Fonction | GPIO | Détail |
|---|---:|---|
| CAN TX / RX | 5 / 4 | TWAI natif, 500 kbit/s |
| UART TX / RX vers EXCC | 17 / 16 | UART1, TX/RX croisés |
| DRV8874 IN1 / IN2 | 32 / 33 | RMT; IN2 inversé par matrice GPIO |
| ADC alimentation | 35 | Mesure avant pont en H |
| Injection défaut | 25 | Réservée à la sortie DAC vers DISTRIB selon câblage |

Compiler avec PlatformIO : `pio run`. Le firmware de cette carte ne modifie
pas le code source DCC-EX.
