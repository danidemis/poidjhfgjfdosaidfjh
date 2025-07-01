#pragma once

#include <cstdint>
#include <cstddef> // Necessario per size_t
#include "settings.h" // Includiamo settings.h per avere KeyboardLayout e TargetOS

// Definiamo i codici per i tasti modificatori per chiarezza
#define KEY_LEFT_CTRL   0x80
#define KEY_L_SHIFT     0x81 // Usiamo il codice HID standard per Left Shift
#define KEY_LEFT_ALT    0x82
#define KEY_R_SHIFT     0x85
#define KEY_R_ALT       0x86 // Usiamo il codice HID standard per Right Alt
#define KEY_NO_MOD      0x00

// Nuova struttura che usa una stringa per il carattere, per supportare UTF-8
struct KeyMapping {
    const char* character_utf8; // Il carattere come stringa (es. "€")
    uint8_t modifier1;
    uint8_t modifier2;
    uint8_t primary_key;
};

// --- MAPPA LAYOUT ITALIANO (IT) ---
const KeyMapping italian_layout_win[] = {
    // --- Caratteri Normali (senza modificatori) ---
    {"è", KEY_NO_MOD,  KEY_NO_MOD, '['}, //OK      // Tasto fisico: [
    {"ò", KEY_NO_MOD,  KEY_NO_MOD, ';'}, //OK      // Tasto fisico: ;
    {"à", KEY_NO_MOD,  KEY_NO_MOD, '\''},//OK     // Tasto fisico: '
    {"ù", KEY_NO_MOD,  KEY_NO_MOD, '\\'},//OK     // Tasto fisico: 
    {"ì", KEY_NO_MOD,  KEY_NO_MOD, '='},      // Tasto fisico: =
    {"-", KEY_NO_MOD,  KEY_NO_MOD, '/'}, //OK
    {",", KEY_NO_MOD,  KEY_NO_MOD, ','}, //OK
    {".", KEY_NO_MOD,  KEY_NO_MOD, '.'}, //OK
    {"'", KEY_NO_MOD,  KEY_NO_MOD, '-'},      // L'apostrofo è sul tasto fisico /
    {"+", KEY_NO_MOD,  KEY_NO_MOD, ']'},      // Il + non shiftato è sul tasto fisico =
    {"\\", KEY_NO_MOD, KEY_NO_MOD, '`'},     // Tasto ISO extra < > vicino a shift sx

    // --- Caratteri con Shift ---
    {"é", KEY_L_SHIFT, KEY_NO_MOD, '['}, //OK
    {"ç", KEY_L_SHIFT, KEY_NO_MOD, ';'}, //OK
    {"°", KEY_L_SHIFT, KEY_NO_MOD, '\''},//OK
    {"§", KEY_L_SHIFT, KEY_NO_MOD, '\\'},//OK
    {"?", KEY_L_SHIFT, KEY_NO_MOD, '-'},
    {"*", KEY_L_SHIFT, KEY_NO_MOD, ']'},
    {"_", KEY_L_SHIFT, KEY_NO_MOD, '/'}, //OK
    {";", KEY_L_SHIFT, KEY_NO_MOD, ','}, //OK
    {":", KEY_L_SHIFT, KEY_NO_MOD, '.'}, //OK
    {"|", KEY_L_SHIFT, KEY_NO_MOD, '`'}, //OK     // Tasto ISO extra < >
    {"!", KEY_L_SHIFT, KEY_NO_MOD, '1'}, //OK
    {"\"",KEY_L_SHIFT, KEY_NO_MOD, '2'}, //OK
    {"£", KEY_L_SHIFT, KEY_NO_MOD, '3'}, //OK
    {"$", KEY_L_SHIFT, KEY_NO_MOD, '4'}, //OK
    {"%", KEY_L_SHIFT, KEY_NO_MOD, '5'}, //OK
    {"&", KEY_L_SHIFT, KEY_NO_MOD, '6'}, //OK
    {"/", KEY_L_SHIFT, KEY_NO_MOD, '7'}, //OK
    {"(", KEY_L_SHIFT, KEY_NO_MOD, '8'}, //OK
    {")", KEY_L_SHIFT, KEY_NO_MOD, '9'}, //OK
    {"=", KEY_L_SHIFT, KEY_NO_MOD, '0'}, //OK
    {"^", KEY_L_SHIFT, KEY_NO_MOD, '='},      // L'accento circonflesso è Shift + ì (tasto fisico /)

    // --- Caratteri con AltGr (KEY_R_ALT) ---
    {"@", KEY_R_ALT,   KEY_NO_MOD, ';'}, //OK      // @ = AltGr + ò
    {"#", KEY_R_ALT,   KEY_NO_MOD, '\''},//OK     // # = AltGr + à
    {"[", KEY_R_ALT,   KEY_NO_MOD, '['}, //OK      // [ = AltGr + è
    {"]", KEY_R_ALT,   KEY_NO_MOD, ']'},      // ] = AltGr + +
    {"€", KEY_R_ALT,   KEY_NO_MOD, 'e'},
    {"{", KEY_L_SHIFT, KEY_R_ALT, '['}, //OK
    {"}", KEY_L_SHIFT, KEY_R_ALT, ']'}, //OK

    // Tasto speciale ISO
    {"<", KEY_NO_MOD,  KEY_NO_MOD, 0x64},
    {">", KEY_L_SHIFT, KEY_NO_MOD, 0x64}
};

// --- MAPPA ITALIANO - MACOS ---
const KeyMapping italian_layout_mac[] = {
    // Caratteri con modificatore (Option/AltGr)
    // La logica è: Option + [Tasto dove si troverebbe il carattere su una tastiera Mac ITA]
    {"@", KEY_R_ALT,   KEY_NO_MOD, 0x33}, // @ = Option + ò (che è il tasto ';' su una tastiera US)
    {"#", KEY_R_ALT,   KEY_NO_MOD, 0x34}, // # = Option + à (che è il tasto "'" su una tastiera US)
    {"[", KEY_R_ALT,   KEY_NO_MOD, 0x2F}, // [ = Option + è (che è il tasto '[' su una tastiera US)
    {"]", KEY_R_ALT,   KEY_NO_MOD, 0x2E}, // ] = Option + + (che è il tasto '=' su una tastiera US)
    {"€", KEY_R_ALT,   KEY_NO_MOD, 'e'},  // € = Option + e (la 'e' è nella stessa posizione)
    {"{", KEY_L_SHIFT, KEY_R_ALT, 0x2F}, // { = Shift + Option + è
    {"}", KEY_L_SHIFT, KEY_R_ALT, 0x2E}, // } = Shift + Option + +

    // Caratteri con solo Shift (spesso uguali a Windows/US)
    {"!", KEY_L_SHIFT, KEY_NO_MOD, '1'},
    {"\"",KEY_L_SHIFT, KEY_NO_MOD, '2'},
    {"£", KEY_L_SHIFT, KEY_NO_MOD, '3'},
    {"$", KEY_L_SHIFT, KEY_NO_MOD, '4'},
    {"%", KEY_L_SHIFT, KEY_NO_MOD, '5'},
    {"&", KEY_L_SHIFT, KEY_NO_MOD, '6'},
    {"/", KEY_L_SHIFT, KEY_NO_MOD, '7'},
    {"(", KEY_L_SHIFT, KEY_NO_MOD, '8'},
    {")", KEY_L_SHIFT, KEY_NO_MOD, '9'},
    {"=", KEY_L_SHIFT, KEY_NO_MOD, '0'},
    {"?", KEY_L_SHIFT, KEY_NO_MOD, 0x32}, // ? in ITA è Shift + ' (apostrofo)

    // Caratteri speciali che non richiedono modificatori
    {"è", KEY_NO_MOD,  KEY_NO_MOD, 0x2F}, // Il tasto fisico per 'è' è lo stesso del '[' su una tastiera US
    {"é", KEY_L_SHIFT, KEY_NO_MOD, 0x2F}, // Stesso tasto con Shift
    {"à", KEY_NO_MOD,  KEY_NO_MOD, 0x34}, // Il tasto fisico per 'à' è lo stesso del "'" (apostrofo) su una tastiera US
    {"°", KEY_L_SHIFT, KEY_NO_MOD, 0x34}, // Stesso tasto con Shift
    {"ò", KEY_NO_MOD,  KEY_NO_MOD, 0x33}, // Il tasto fisico per 'ò' è lo stesso del ';' su una tastiera US
    {"ç", KEY_L_SHIFT, KEY_NO_MOD, 0x33}, // Stesso tasto con Shift
    {"ù", KEY_NO_MOD,  KEY_NO_MOD, 0x35}, // Il tasto fisico per 'ù' è spesso ` (backtick) su una tastiera US

    // Altri caratteri comuni
    {";", KEY_L_SHIFT, KEY_NO_MOD, ','},
    {":", KEY_L_SHIFT, KEY_NO_MOD, '.'},
    {"_", KEY_L_SHIFT, KEY_NO_MOD, '-'},
    {"<", KEY_NO_MOD,  KEY_NO_MOD, 0x64}, // Tasto ISO extra, se presente
    {">", KEY_L_SHIFT, KEY_NO_MOD, 0x64}
};

// --- MAPPA LAYOUT TEDESCO (DE - QWERTZ) ---
const KeyMapping german_layout_win[] = {
    // Scambio Z/Y
    {"y", KEY_NO_MOD,  KEY_NO_MOD, 'z'}, {"Y", KEY_L_SHIFT, KEY_NO_MOD, 'z'},
    {"z", KEY_NO_MOD,  KEY_NO_MOD, 'y'}, {"Z", KEY_L_SHIFT, KEY_NO_MOD, 'y'},

    // AltGr
    {"@", KEY_R_ALT,   KEY_NO_MOD, 'q'},
    {"€", KEY_R_ALT,   KEY_NO_MOD, 'e'},
    {"[", KEY_R_ALT,   KEY_NO_MOD, '8'},
    {"]", KEY_R_ALT,   KEY_NO_MOD, '9'},
    {"{", KEY_R_ALT,   KEY_NO_MOD, '7'},
    {"}", KEY_R_ALT,   KEY_NO_MOD, '0'},
    {"\\",KEY_R_ALT,   KEY_NO_MOD, 0x2D}, // Tasto ß?\
    {"~", KEY_R_ALT,   KEY_NO_MOD, 0x30}, // Tasto +*~

    // Shift
    {"!", KEY_L_SHIFT, KEY_NO_MOD, '1'}, {"\"",KEY_L_SHIFT, KEY_NO_MOD, '2'},
    {"§", KEY_L_SHIFT, KEY_NO_MOD, '3'}, {"$", KEY_L_SHIFT, KEY_NO_MOD, '4'},
    {"%", KEY_L_SHIFT, KEY_NO_MOD, '5'}, {"&", KEY_L_SHIFT, KEY_NO_MOD, '6'},
    {"/", KEY_L_SHIFT, KEY_NO_MOD, '7'}, {"(", KEY_L_SHIFT, KEY_NO_MOD, '8'},
    {")", KEY_L_SHIFT, KEY_NO_MOD, '9'}, {"=", KEY_L_SHIFT, KEY_NO_MOD, '0'},
    {"?", KEY_L_SHIFT, KEY_NO_MOD, 0x2D},
    {"°", KEY_L_SHIFT, KEY_NO_MOD, 0x35}, // Tasto ^°
    {";", KEY_L_SHIFT, KEY_NO_MOD, ','}, {":", KEY_L_SHIFT, KEY_NO_MOD, '.'},
    {"_", KEY_L_SHIFT, KEY_NO_MOD, '-'},

    // Tasti singoli
    {"ü", KEY_NO_MOD,  KEY_NO_MOD, 0x34}, {"ö", KEY_NO_MOD,  KEY_NO_MOD, 0x2F},
    {"ä", KEY_NO_MOD,  KEY_NO_MOD, 0x33}, {"ß", KEY_NO_MOD,  KEY_NO_MOD, 0x2D},
    {"+", KEY_NO_MOD,  KEY_NO_MOD, 0x30}, {"#", KEY_NO_MOD,  KEY_NO_MOD, 0x32},
    {"'", KEY_L_SHIFT, KEY_NO_MOD, 0x32}, {"*", KEY_L_SHIFT, KEY_NO_MOD, 0x30},
    {"<", KEY_NO_MOD,  KEY_NO_MOD, 0x64}, {">", KEY_L_SHIFT, KEY_NO_MOD, 0x64},
};


// --- MAPPA LAYOUT FRANCESE (FR - AZERTY) ---
const KeyMapping french_layout_win[] = {
    // Scambi di tasti
    {"a", KEY_NO_MOD, KEY_NO_MOD, 'q'}, {"A", KEY_L_SHIFT, KEY_NO_MOD, 'q'},
    {"q", KEY_NO_MOD, KEY_NO_MOD, 'a'}, {"Q", KEY_L_SHIFT, KEY_NO_MOD, 'a'},
    {"z", KEY_NO_MOD, KEY_NO_MOD, 'w'}, {"Z", KEY_L_SHIFT, KEY_NO_MOD, 'w'},
    {"w", KEY_NO_MOD, KEY_NO_MOD, 'z'}, {"W", KEY_L_SHIFT, KEY_NO_MOD, 'z'},
    {"m", KEY_NO_MOD, KEY_NO_MOD, ';'}, {"M", KEY_L_SHIFT, KEY_NO_MOD, ';'},

    // Riga dei numeri
    {"&", KEY_NO_MOD, KEY_NO_MOD, 0x1E}, {"1", KEY_L_SHIFT, KEY_NO_MOD, 0x1E},
    {"é", KEY_NO_MOD, KEY_NO_MOD, 0x1F}, {"2", KEY_L_SHIFT, KEY_NO_MOD, 0x1F},
    {"\"",KEY_NO_MOD, KEY_NO_MOD, 0x20}, {"3", KEY_L_SHIFT, KEY_NO_MOD, 0x20},
    {"\'",KEY_NO_MOD, KEY_NO_MOD, 0x21}, {"4", KEY_L_SHIFT, KEY_NO_MOD, 0x21},
    {"(", KEY_NO_MOD, KEY_NO_MOD, 0x22}, {"5", KEY_L_SHIFT, KEY_NO_MOD, 0x22},
    {"-", KEY_NO_MOD, KEY_NO_MOD, 0x23}, {"6", KEY_L_SHIFT, KEY_NO_MOD, 0x23},
    {"è", KEY_NO_MOD, KEY_NO_MOD, 0x24}, {"7", KEY_L_SHIFT, KEY_NO_MOD, 0x24},
    {"_", KEY_NO_MOD, KEY_NO_MOD, 0x25}, {"8", KEY_L_SHIFT, KEY_NO_MOD, 0x25},
    {"ç", KEY_NO_MOD, KEY_NO_MOD, 0x26}, {"9", KEY_L_SHIFT, KEY_NO_MOD, 0x26},
    {"à", KEY_NO_MOD, KEY_NO_MOD, 0x27}, {"0", KEY_L_SHIFT, KEY_NO_MOD, 0x27},

    // AltGr
    {"@", KEY_R_ALT, KEY_NO_MOD, 0x27}, {"#", KEY_R_ALT, KEY_NO_MOD, 0x20},
    {"€", KEY_R_ALT, KEY_NO_MOD, 'e'}, {"[", KEY_R_ALT, KEY_NO_MOD, 0x22},
    {"]", KEY_R_ALT, KEY_NO_MOD, 0x2D},

    // Altri tasti
    {")", KEY_NO_MOD, KEY_NO_MOD, 0x2D}, {"°", KEY_L_SHIFT, KEY_NO_MOD, 0x2D},
    {"=", KEY_NO_MOD, KEY_NO_MOD, 0x30}, {"+", KEY_L_SHIFT, KEY_NO_MOD, 0x30},
    {"}", KEY_R_ALT, KEY_NO_MOD, 0x30},
    {"^", KEY_NO_MOD, KEY_NO_MOD, 0x34}, {"¨", KEY_L_SHIFT, KEY_NO_MOD, 0x34},
    {"$", KEY_NO_MOD, KEY_NO_MOD, 0x33}, {"£", KEY_L_SHIFT, KEY_NO_MOD, 0x33},
    {"ù", KEY_NO_MOD, KEY_NO_MOD, 0x32}, {"%", KEY_L_SHIFT, KEY_NO_MOD, 0x32},
    {"*", KEY_NO_MOD, KEY_NO_MOD, 0x33},
    {",", KEY_NO_MOD, KEY_NO_MOD, 'm'}, {"?", KEY_L_SHIFT, KEY_NO_MOD, 'm'},
    {";", KEY_NO_MOD, KEY_NO_MOD, ','}, {".", KEY_L_SHIFT, KEY_NO_MOD, ','},
    {":", KEY_NO_MOD, KEY_NO_MOD, '.'}, {"/", KEY_L_SHIFT, KEY_NO_MOD, '.'},
    {"<", KEY_NO_MOD, KEY_NO_MOD, 0x64}, {">", KEY_L_SHIFT, KEY_NO_MOD, 0x64},
};


// --- MAPPA LAYOUT SPAGNOLO (ES) ---
const KeyMapping spanish_layout_win[] = {
    // AltGr
    {"@", KEY_R_ALT, KEY_NO_MOD, '2'}, {"#", KEY_R_ALT, KEY_NO_MOD, '3'},
    {"€", KEY_R_ALT, KEY_NO_MOD, 'e'}, {"[", KEY_R_ALT, KEY_NO_MOD, 0x34},
    {"]", KEY_R_ALT, KEY_NO_MOD, 0x30}, {"{", KEY_R_ALT, KEY_NO_MOD, 0x35},
    {"}", KEY_R_ALT, KEY_NO_MOD, 0x2D}, {"\\",KEY_R_ALT, KEY_NO_MOD, 0x38},
    {"|", KEY_R_ALT, KEY_NO_MOD, '1'},

    // Shift
    {"!", KEY_L_SHIFT, KEY_NO_MOD, '1'}, {"\"",KEY_L_SHIFT, KEY_NO_MOD, '2'},
    {"·", KEY_L_SHIFT, KEY_NO_MOD, '3'}, {"$", KEY_L_SHIFT, KEY_NO_MOD, '4'},
    {"%", KEY_L_SHIFT, KEY_NO_MOD, '5'}, {"&", KEY_L_SHIFT, KEY_NO_MOD, '6'},
    {"/", KEY_L_SHIFT, KEY_NO_MOD, '7'}, {"(", KEY_L_SHIFT, KEY_NO_MOD, '8'},
    {")", KEY_L_SHIFT, KEY_NO_MOD, '9'}, {"=", KEY_L_SHIFT, KEY_NO_MOD, '0'},
    {"?", KEY_L_SHIFT, KEY_NO_MOD, '\''},{"¿", KEY_L_SHIFT, KEY_NO_MOD, 0x2D},
    {";", KEY_L_SHIFT, KEY_NO_MOD, ','}, {":", KEY_L_SHIFT, KEY_NO_MOD, '.'},
    {"_", KEY_L_SHIFT, KEY_NO_MOD, '-'},
    {"ª", KEY_L_SHIFT, KEY_NO_MOD, 0x38},
    {"º", KEY_NO_MOD,  KEY_NO_MOD, 0x38},
    {"Ç", KEY_L_SHIFT, KEY_NO_MOD, 0x33},
    
    // Tasti singoli
    {"ñ", KEY_NO_MOD,  KEY_NO_MOD, ';'}, // Il tasto ;: in US è ñÑ in ES
    {"Ñ", KEY_L_SHIFT, KEY_NO_MOD, ';'},
    {"ç", KEY_NO_MOD,  KEY_NO_MOD, 0x33}, // Tasto ç}
    {"<", KEY_NO_MOD,  KEY_NO_MOD, 0x64},
    {">", KEY_L_SHIFT, KEY_NO_MOD, 0x64},
    {"à", KEY_NO_MOD, KEY_NO_MOD, 0x34}, // ´[
    {"`", KEY_NO_MOD, KEY_NO_MOD, 0x34}, // ´[
    {"´", KEY_NO_MOD, KEY_NO_MOD, 0x34}, // ´[
    {"+", KEY_NO_MOD, KEY_NO_MOD, 0x30},
    {"*", KEY_L_SHIFT, KEY_NO_MOD, 0x30},
};


// --- STRUTTURA PER SELEZIONARE LA MAPPA CORRETTA ---
const KeyMapping* get_layout_map(KeyboardLayout layout, TargetOS os, size_t* map_size) {
    if (layout == KeyboardLayout::ITALIANO) {
        if (os == TargetOS::MACOS) {
            *map_size = sizeof(italian_layout_mac) / sizeof(KeyMapping);
            return italian_layout_mac;
        } else { // WINDOWS_LINUX
            *map_size = sizeof(italian_layout_win) / sizeof(KeyMapping);
            return italian_layout_win;
        }
    }
    
    if (layout == KeyboardLayout::TEDESCO) {
        if (os == TargetOS::MACOS) {
            // TO IMPLEMENT //
            *map_size = sizeof(german_layout_win) / sizeof(KeyMapping);
            return german_layout_win;
        } else { // WINDOWS_LINUX
            *map_size = sizeof(german_layout_win) / sizeof(KeyMapping);
            return german_layout_win;
        }
    }

    if (layout == KeyboardLayout::FRANCESE) {
        if (os == TargetOS::MACOS) {
            *map_size = sizeof(french_layout_win) / sizeof(KeyMapping);
            return french_layout_win;
        } else { // WINDOWS_LINUX
            *map_size = sizeof(french_layout_win) / sizeof(KeyMapping);
            return french_layout_win;
        }
    }

    if (layout == KeyboardLayout::SPAGNOLO) {
        if (os == TargetOS::MACOS) {
            *map_size = sizeof(spanish_layout_win) / sizeof(KeyMapping);
            return spanish_layout_win;
        } else { // WINDOWS_LINUX
            *map_size = sizeof(spanish_layout_win) / sizeof(KeyMapping);
            return spanish_layout_win;
        }
    }

    // Default: nessuna mappa speciale (es. layout USA)
    *map_size = 0;
    return nullptr;
}

// const size_t layout_map_sizes[] = {
//     0,                                              // USA
//     sizeof(italian_layout_win) / sizeof(KeyMapping),    // ITALIANO
//     sizeof(german_layout_win) / sizeof(KeyMapping),     // TEDESCO
//     sizeof(french_layout_win) / sizeof(KeyMapping),     // FRANCESE
//     sizeof(spanish_layout_win) / sizeof(KeyMapping)     // SPAGNOLO
// };