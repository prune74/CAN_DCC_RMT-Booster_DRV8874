#include <Arduino.h>
#include "Config.h"
#include "BoosterCore.h"
#include "StatusLeds.h"
#include "CanBooster.h"
#include "BoosterUartLink.h"
#include <string.h>

BoosterCore  booster;
CanBooster   canLink;
StatusLeds   leds;
BoosterUartLink uartLink;

static void printDccDiagnostics() {
    DccCanRx &rx = booster.getCanRx();
    Serial.println("--- Diagnostic DCC booster ---");
    if (booster.hasDccReference()) {
        Serial.printf("Profil UART DCC_REFERENCE 0x06 reçu: oui; valide=%s; frais=%s; seq=%u; âge=%lu ms; MAIN=%s; demi-bits=%u/%u µs\n",
                      booster.dccReferenceValid() ? "oui" : "non",
                      booster.dccReferenceFresh() ? "oui" : "non",
                      static_cast<unsigned>(booster.dccReferenceSequence()),
                      (unsigned long)booster.dccReferenceAgeMs(),
                      booster.dccReferenceMainOn() ? "ON" : "OFF",
                      static_cast<unsigned>(booster.dccReferenceHalfBit1Us()),
                      static_cast<unsigned>(booster.dccReferenceHalfBit0Us()));
    } else {
        Serial.println("Profil UART DCC_REFERENCE 0x06 reçu: non; sortie interdite");
    }
    Serial.printf("Profil LaBox appliqué au RMT: %s; seq=%u; valeurs RMT=%u/%u µs; applications=%lu; trames valides/invalides=%lu/%lu\n",
                  booster.hasAppliedDccProfile() ? "oui" : "non",
                  static_cast<unsigned>(booster.appliedProfileSequence()),
                  static_cast<unsigned>(booster.appliedHalfBit1Us()),
                  static_cast<unsigned>(booster.appliedHalfBit0Us()),
                  (unsigned long)booster.profileApplications(),
                  (unsigned long)booster.dccReferenceValidFrames(),
                  (unsigned long)booster.dccReferenceInvalidFrames());
    Serial.printf("Cutout dans 0x100: reçus=%lu; appliqués=%lu; dernier reçu=",
                  (unsigned long)rx.cutoutStartsReceived(),
                  (unsigned long)booster.cutoutsApplied());
    if (rx.lastCutoutReceivedMeasured())
        Serial.printf("%u µs", static_cast<unsigned>(rx.lastCutoutReceivedUs()));
    else
        Serial.print("durée absente (trame invalide)");
    if (booster.cutoutsApplied() > 0)
        Serial.printf("; dernier appliqué=%u µs (%s)",
                      static_cast<unsigned>(booster.lastCutoutAppliedUs()),
                      booster.lastCutoutWasMeasured() ? "mesuré" : "non mesuré");
    else
        Serial.print("; aucun cutout appliqué depuis le démarrage");
    Serial.printf("; hors plage=%lu; ignorés état/portée=%lu; malformés=%lu; FIFO perdus=%lu\n",
                  (unsigned long)booster.cutoutsRejectedRange(),
                  (unsigned long)booster.cutoutsSkipped(),
                  (unsigned long)rx.cutoutMalformedFrames(),
                  (unsigned long)rx.cutoutQueueDrops());
}

static void pollDiagnosticConsole() {
    static char line[24];
    static size_t length = 0;
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            line[length] = '\0';
            if (strcmp(line, "diag") == 0) printDccDiagnostics();
            else if (strcmp(line, "help") == 0 || length > 0)
                Serial.println("Commandes: diag (profil DCC et cutouts); help");
            length = 0;
        } else if (length + 1 < sizeof(line)) {
            line[length++] = c;
        } else {
            length = 0;
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("CAN_DCC_RMT-BoosterDRV8874 — en attente de l'ID du CC (CFC)");

    booster.begin();
    canLink.begin(booster, leds);
    leds.begin();
    uartLink.begin();
}

void loop() {
    pollDiagnosticConsole();
    canLink.poll();       // pompe le CAN → décodage du flux 0x100 v0.4.0
    booster.update();     // failsafe + sécurité + cutout + flux RMT
    uartLink.update(booster); // commandes, heartbeat et mesures à 1 kHz
    leds.update(booster); // WS2812 non bloquant
}
