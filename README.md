# CAN_DCC_RMT-Booster_DRV8874

> Suivi inter-discussions : voir [`Suivi/Cartes/SUIVI_BOOSTER_DRV8874.md`](../../Suivi/Cartes/SUIVI_BOOSTER_DRV8874.md) et le [tableau de bord général](../../Suivi/ETAT_AVANCEMENT.md).

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
| `0x101` | CAN → booster | START avec intervalle long mesuré (DLC 4); END (DLC 2) |
| `0x102` | Booster → CAN | `{ID canton, TYPE=5 (BOOSTER), courant/10, tension alim/100, état}` |
| `0x104` | Convertisseur → booster | Profil LaBox version 1, DLC 8; les demi-bits reçus règlent la génération RMT et MAIN OFF coupe la sortie |

La télémétrie `0x102` est envoyée lors d'un changement d'état et avec un
heartbeat de 5 s. États : `0=OK`, `1=FAULT`, `2=OVERHEAT`, `3=OFF`.
Les capteurs locaux de courant et de tension sont échantillonnés par le
firmware à 1 kHz pour la supervision/protection ; cela ne signifie pas que
les timings DCC ni le cutout de la voie sont mesurés électriquement. Le contrôle
immédiat de concordance est logiciel : le convertisseur transmet à START
`0x101` l’intervalle long observé par le convertisseur; le booster l'utilise
pour la durée BRAKE et sa fenêtre de capture RailCom. Cela ne démontre pas que
cette durée observée équivaut au profil logiciel LaBox (`29+215 µs` RMT,
timer `160 µs × 4 phases`) ni à la durée électrique mesurée sur la voie.

`0x104` ne contient pas les paramètres de génération cutout LaBox `0x22`.
Le booster conserve le repli 58/100 µs si aucun profil n'a été reçu. Le champ
tick et le préambule sont relayés mais ne sont pas encore utilisés par le
générateur du booster. `0x103` reste réservé dans la coordination au retour
d'adresse RailCom.
Le format est documenté dans
[`Suivi/ERM_Liaison_DCC/PLAN_ANALYSE_DCC_CUTOUT_AUTOMATIQUE.md`](../../Suivi/ERM_Liaison_DCC/PLAN_ANALYSE_DCC_CUTOUT_AUTOMATIQUE.md).

La comparaison actuelle s'appuie sur les timings numériques reçus par CAN et
sur les événements cutout transmis par le convertisseur. Aucun analyseur
autonome externe n'est intégré au projet CFC et la capture électrique de la
sortie booster n'est pas implémentée. Le format START DLC 4 est en cours de
validation; les anciens START DLC 2 utilisent le repli logiciel de 488 µs,
qui n'est pas une référence de durée validée par LaBox.
L'identité de canton, de 1 à 252, est attribuée par le CC via l'EXCC; elle
n'est conservée qu'en RAM. `0xFF` signifie qu'aucune identité n'est attribuée.

Les cibles prévues sont 12 V en N et 15 V en HO sur la voie sous charge.
La mesure ADC actuelle (GPIO35) est raccordée à la sortie de l'alimentation,
pas aux rails après le DRV8874. Elle ne mesure donc pas ces cibles de voie.
La NMRA S-9.1 donne une enveloppe de 8,5 à 22 V pour le signal de sortie N/HO.
Le seuil logiciel de sous-tension de l'alimentation reste 9 V. Les valeurs de
digipot actuelles sont fixes et aucune calibration automatique n’est
implémentée. Le seuil d’analyse DCC pleine amplitude / RailCom reste à
caractériser ; le dossier de suivi étudie provisoirement 5 V différentiels
avec hystérésis, soit 3,5 V sous le minimum normatif, sans figer le schéma ni
les composants. Il n'existe pas actuellement de mesure ADC de la tension
différentielle des rails ni de retour permettant de vérifier la consigne.

Pour la future calibration automatique de tension, le suivi propose un circuit
distinct de mesure de l’amplitude `|OUT1−OUT2|` après le DRV8874 : redressement
différentiel ou détection de crête, filtrage lent, mise à l’échelle et
protection avant l’ADC 3,3 V. L’échelle 10:1 est seulement un exemple (22 V
deviendraient 2,2 V avant correction du redresseur). Ce circuit ne remplace pas
le capteur rapide nécessaire à l’analyse des demi-bits et du cutout. Voir la
[spécification de mesure](../../Suivi/ERM_Liaison_DCC/SPECIFICATION_MESURES_BOOSTER.md).
Le document de référence `G:\Windows 10\Téléchargement\Mesure_DCC.pdf`
présente un pont redresseur avec 4,7 kΩ pour mesure VDC au multimètre. Sa
sortie n’est pas compatible directement avec l’ADC : un diviseur, un filtre
et une protection doivent être dimensionnés, en tenant compte du courant
prélevé sur la voie et de l’effet possible sur RailCom.
L’architecture de travail retenue pour cette mesure lente est un pont
redresseur suivi d’un diviseur résistif, d’un filtre et d’une protection ADC,
sans optocoupleur. Le rapport du diviseur, le courant prélevé et l’effet sur
RailCom restent à dimensionner et vérifier. Cette voie analogique ne remplace
pas le capteur rapide du DCC/cutout. Voir la spécification de mesure ERM
Liaison DCC.

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
- Le code actuel possède encore un repli historique de 488 µs pour les messages START `0x101` sans durée (DLC 2). Le START DLC 4 transporte l'intervalle observé par le convertisseur, mais celui-ci n'est pas contractuellement équivalent au profil LaBox publié sur le CAN de supervision. Définir ce raccordement avant de déclarer le cutout booster conforme à LaBox.

## Brochage

| Fonction | GPIO | Détail |
|---|---:|---|
| CAN TX / RX | 5 / 4 | TWAI natif, 500 kbit/s |
| UART TX / RX vers EXCC | 17 / 16 | UART1, TX/RX croisés |
| DRV8874 IN1 / IN2 | 32 / 33 | RMT canal 5; IN2 inversé par matrice GPIO |
| DRV8874 nSLEEP | 27 | Niveau haut actif; PMODE strapé en mode PWM |
| Mesure courant IPROPI | 34 | ADC, résistance 3,6 kΩ |
| Mesure sortie alimentation | 35 | ADC, diviseur 68 kΩ / 10 kΩ; ne mesure pas la voie DCC |
| Entrée RailCom isolée | 36 | RX UART2 |
| Capture DCC isolée (réservée) | 25 | Entrée MCPWM à ajouter; GPIO libre dans le brochage logiciel actuel |
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
Dans le moniteur, envoyer `diag` puis Entrée pour afficher la référence reçue
et appliquée, les demi-bits configurés, les durées cutout reçues/appliquées,
ainsi que les compteurs de trames invalides, hors plage et événements ignorés.
Les broches sont dans `src/Pins.h`; les réglages sont dans `src/Config.h`.

## Licence

Le dépôt ne contient pas actuellement de fichier de licence.
