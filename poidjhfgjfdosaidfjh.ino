#include "pin_config.h"

#include <PowerDeliveryHUSB238.hpp>
#include <XPowersLib.h>
#include <XPowersLibInterface.hpp>
#include <XPowersParams.hpp>
#include <esp_task_wdt.h>  // Aggiungi per il reset hardware

#include <Arduino.h>
#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <ESP_IOExpander_Library.h>
#include <Wire.h>
#include <SD_MMC.h>
#include "HWCDC.h"
#include <memory>
#include "security.h"
#include "crypto.h"
#include "credentials.h"
#include "keyboard_layouts.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <cstring>  // Necessario per strlen e strncmp
#include "settings.h"
#include <algorithm>  // Per la funzione di ordinamento std::sort
#include <WiFi.h>     // Per ottenere l'ora da internet
#include "time.h"     // Per gestire l'ora
#include "mbedtls/sha256.h"
#include <math.h>

#include "SensorQMI8658.hpp"

// --- DICHIARAZIONE DEL FONT PERSONALIZZATO (con linkage C) ---
#ifdef __cplusplus
extern "C" {
#endif

  LV_FONT_DECLARE(montserrat_18_extended);
  LV_FONT_DECLARE(montserrat_22_extended);

#ifdef __cplusplus
} /*extern "C"*/
#endif
// -----------------------------------------------------------
//LV_IMG_DECLARE(food_hot_pocket_donut_junk_food_icon_261882);
//LV_IMG_DECLARE(food_pizza_junk_food_icon_261884);
LV_IMG_DECLARE(food_potatoes_junk_food_fries_icon_261877);

// =================================================================
// SEZIONE 2: OGGETTI E VARIABILI GLOBALI
// =================================================================

// --- Oggetti Hardware e UI ---
USBHIDKeyboard Keyboard;
HWCDC USBSerial;
SecurityManager securityManager;
Crypto crypto;
CredentialsManager credManager;
XPowersPMU pmu;
SensorQMI8658 qmi;
IMUdata acc;

Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX* gfx = new Arduino_SH8601(bus, -1, 0, false, LCD_WIDTH, LCD_HEIGHT);
ESP_IOExpander* expander = NULL;
std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus = std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);
void Arduino_IIC_Touch_Interrupt(void);
std::unique_ptr<Arduino_IIC> FT3168(new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS,
                                                       DRIVEBUS_DEFAULT_VALUE, TP_INT, Arduino_IIC_Touch_Interrupt));
void Arduino_IIC_Touch_Interrupt(void) {
  FT3168->IIC_Interrupt_Flag = true;
}

// Struttura per l'ordinamento delle credenziali
struct CredentialInfo {
  String title;
  size_t original_index;
};
std::vector<CredentialInfo> sorted_credentials;

enum class ChangePinState {
  AWAITING_OLD_PIN,
  AWAITING_NEW_PIN,
  AWAITING_CONFIRM_PIN
};

// Variabili per l'ora NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // Offset GMT+1
const int daylightOffset_sec = 3600;  // Ora legale


static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LCD_WIDTH * LCD_HEIGHT / 10];
static lv_obj_t* pin_display_label;
static String current_pin_attempt = "";
static lv_obj_t* time_label;
static lv_obj_t* credential_roller;
static ChangePinState current_change_pin_state;
static lv_obj_t* layout_status_label;
static lv_obj_t* os_status_label;
static String old_pin_input;
static String new_pin_storage;
static lv_obj_t* change_pin_label_title;
static lv_obj_t* change_pin_label_dots;
static String change_pin_input_buffer = "";
static lv_obj_t* alphabet_scroller;
static uint32_t g_last_send_press_time = 0;
const uint32_t SEND_BUTTON_COOLDOWN = 1000;  // Cooldown di 1.5 secondi
static bool is_display_off = false;
static lv_obj_t* screensaver_clock_label = NULL;
static bool is_screensaver_active = false;


// =================================================================
// SEZIONE 3: DICHIARAZIONI ANTICIPATE
// =================================================================

void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data);
void example_increase_lvgl_tick(void* arg);
void create_pin_lock_screen();
void create_main_screen();
void change_to_main_screen_cb(lv_timer_t* timer);
void pin_matrix_event_cb(lv_event_t* e);
void create_settings_screen();
void open_usb_mode_screen_cb(lv_event_t* e);
void type_password_with_layout(const char* password);
void create_change_pin_screen();  // Dichiarazione anticipata per la nuova schermata
void checkForAndRunImport();
void change_pin_keypad_event_cb(lv_event_t* e);
void create_change_pin_flow_screen();
void post_pin_change_reboot_cb(lv_timer_t* timer);
void create_layout_selection_screen();
void create_os_selection_screen();
void update_status_bar();
static void quick_jump_event_cb(lv_event_t* e);
void show_serial_mode_warning_popup(lv_timer_t* timer);
void create_wipe_settings_screen();
void handle_inactivity();


// =================================================================
// SEZIONE 2: DICHIARAZIONE ANTICIPATA DELLA FUNZIONE DI TEST
// =================================================================
void run_sd_storage_test();
void run_backend_tests();
//
// =================================================================
// SEZIONE 5: SETUP E LOOP
// =================================================================

void setup() {
  USBSerial.begin(115200);

  settingsManager.begin();

  if (settingsManager.isHidModeEnabled()) {
    USBSerial.println("Modalita' HID (Tastiera) ATTIVA.");
    USB.begin();
    Keyboard.begin();
  } else {
    USBSerial.println("Modalita' HID (Tastiera) DISATTIVATA. Solo seriale.");
  }

  USBSerial.println("Avvio Password Manager - Fase 4 (Backend Test)");

  Wire.begin(IIC_SDA, IIC_SCL);

  // --- INIZIALIZZAZIONE DEL POWER MANAGER  ---
  if (!pmu.init()) {  // La funzione pubblica corretta è init()
    USBSerial.println("ERRORE: Inizializzazione PMU AXP2101 fallita!");
  } else {
    USBSerial.println("OK: PMU AXP2101 inizializzato.");
    // --- ABILITAZIONE TASTO FISICO ---
    // Abilita l'interrupt per la pressione breve del tasto
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
    // Pulisci eventuali interrupt precedenti all'avvio
    pmu.clearIrqStatus();
    // ---------------------------------
  }
  // ---------------------------------------------
  // --- INIZIALIZZAZIONE ACCELEROMETRO QMI8658 ---
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    USBSerial.println("ERRORE: Sensore QMI8658 non trovato!");
  } else {
    USBSerial.println("OK: Sensore QMI8658 inizializzato.");
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
    qmi.enableAccelerometer();
  }
  // ---------------------------------------------

  expander = new ESP_IOExpander_TCA95xx_8bit((i2c_port_t)0, 0x20, IIC_SCL, IIC_SDA);
  expander->init();
  expander->begin();
  expander->pinMode(7, OUTPUT);

  // 1. Inizializza il driver grafico
  gfx->begin();

  // 2. Prepara la schermata nera PRIMA di accendere il display
  gfx->fillScreen(BLACK);

  // 3. ORA che lo schermo è pronto per mostrare il nero, accendilo.
  expander->digitalWrite(7, HIGH);

  // 4. Imposta la luminosità (opzionale, ma buona pratica)
  gfx->Display_Brightness(255);


  expander->digitalWrite(7, HIGH);
  // --- AGGIUNGI QUESTA RIGA ---
  // Diamo alla SD card un momento per stabilizzarsi dopo l'accensione
  delay(2000);
  // ---------------------------
  if (!FT3168->begin()) {
    USBSerial.println("ERRORE: Init Touch fallita!");
  } else {
    FT3168->IIC_Write_Device_State(Arduino_IIC_Touch::Device::TOUCH_POWER_MODE, Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
  }

  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  if (!SD_MMC.begin("/sdcard", true)) {
    USBSerial.println("ATTENZIONE: Mount SD fallito.");
  } else {
    USBSerial.println("OK: SD Card montata.");
    credManager.begin();
  }


  lv_init();
  const esp_timer_create_args_t lvgl_tick_timer_args = { .callback = &example_increase_lvgl_tick, .name = "lvgl_tick" };
  esp_timer_handle_t lvgl_tick_timer;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LCD_WIDTH * LCD_HEIGHT / 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);


  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

  securityManager.begin();
  create_pin_lock_screen();

  USBSerial.println("\nSetup completato. Avvio interfaccia utente.");
}

void loop() {
  lv_timer_handler();

  // --- LOGICA DI CONTROLLO MOVIMENTO ---
  const float SHAKE_THRESHOLD = 1.5;  // Soglia di attivazione (un valore tra 2.5 e 3.5 è un buon punto di partenza)

  // Esegui il controllo solo se il dispositivo è sbloccato per evitare blocchi inutili
  if (securityManager.getState() == SecurityState::UNLOCKED) {
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
      // Calcola la magnitudine del vettore di accelerazione
      float magnitude = sqrt(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);

      // Se la magnitudine supera la nostra soglia, blocca il dispositivo
      if (magnitude > SHAKE_THRESHOLD) {
        USBSerial.printf("!!! MOVIMENTO BRUSCO RILEVATO (Magnitudine: %.2f) !!! Blocco il dispositivo.\n", magnitude);

        securityManager.lock();    // Blocca il gestore di sicurezza
        create_pin_lock_screen();  // Mostra la schermata del PIN

        // Aggiungiamo una piccola pausa per evitare che il loop rilevi
        // lo stesso movimento più volte e tenti di ridisegnare lo schermo all'infinito
        delay(500);
      }
    }
  }
  // 1. Leggi e aggiorna lo stato di tutti gli interrupt del PMIC
  pmu.getIrqStatus();
  // 2. Ora controlla se l'interrupt specifico del tasto è attivo
  if (pmu.isPekeyShortPressIrq()) {
    USBSerial.println("DEBUG: Tasto fisico premuto - IRQ Rilevato!");

    pmu.clearIrqStatus();  // Pulisci l'interrupt per non riattivarlo di continuo

    if (is_display_off) {
      USBSerial.println("Risveglio il display!");

      gfx->Display_Brightness(255);
      is_display_off = false;
      lv_disp_trig_activity(NULL);
    }
  }
  handle_inactivity();  // Aggiungi la chiamata alla nostra nuova funzione

  //delay(5);
}

// =================================================================
// SEZIONE 4: IMPLEMENTAZIONE FUNZIONI UI E CALLBACK
// =================================================================

// Funzione di callback per il tastierino numerico
void pin_matrix_event_cb(lv_event_t* e) {
  lv_obj_t* btnm = lv_event_get_target(e);
  uint32_t id = lv_btnmatrix_get_selected_btn(btnm);
  const char* txt = lv_btnmatrix_get_btn_text(btnm, id);

  if (txt == NULL) return;

  // Gestione del tasto backspace
  if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    if (current_pin_attempt.length() > 0) {
      current_pin_attempt.remove(current_pin_attempt.length() - 1);
    }
  }
  // Gestione dei tasti numerici
  else if (current_pin_attempt.length() < 6) {
    current_pin_attempt += txt;
  }

  // Aggiorna il display con asterischi
  String asterischi = "";
  for (unsigned int i = 0; i < current_pin_attempt.length(); i++) asterischi += "*";
  lv_label_set_text(pin_display_label, asterischi.c_str());

  // Se abbiamo 6 cifre, controlla il PIN
  if (current_pin_attempt.length() == 6) {
    if (securityManager.checkPin(current_pin_attempt)) {
      settingsManager.resetFailedAttempts();

      // 1. Inizializza la crittografia

      crypto.begin(securityManager.getUserKey());

      // 2. Esegui l'importazione (se c'è il file)
      //checkForAndRunImport();

      // 3. ESEGUI IL DEBUG QUI
      debug_print_all_credentials();

      // 4. Prosegui con la UI
      USBSerial.println("SUCCESS: PIN corretto! Sblocco in corso...");
      lv_timer_create(change_to_main_screen_cb, 50, NULL);
    } else {
      settingsManager.incrementFailedAttempts();  // Incrementiamo il contatore
      uint8_t failed_attempts = settingsManager.getFailedAttempts();
      uint8_t max_attempts = settingsManager.getMaxPinAttempts();

      if (failed_attempts >= max_attempts) {
        // SOGLIA SUPERATA: CANCELLA TUTTO
        USBSerial.println("!!! MASSIMO NUMERO DI TENTATIVI RAGGIUNTO !!!");
        USBSerial.println("!!! CANCELLAZIONE CREDENZIALI IN CORSO... !!!");

        credManager.clear();  // Cancella il file credentials.bin

        // Mostra un messaggio definitivo e non cancellabile
        lv_obj_t* mbox = lv_msgbox_create(NULL, "SICUREZZA ATTIVATA",
                                          "Troppi tentativi errati.\n\n"
                                          "Le credenziali sono state\n"
                                          "cancellate in modo sicuro.\n\n"
                                          "Il dispositivo si riavviera'.",
                                          NULL, false);
        lv_obj_center(mbox);

        // Crea un timer per riavviare il dispositivo dopo 5 secondi
        lv_timer_create([](lv_timer_t* timer) {
          pmu.reset();
        },
                        5000, NULL)
          ->repeat_count = 1;

      } else {
        // Mostra il normale messaggio di errore
        USBSerial.printf("FAIL: PIN errato! Tentativo %d di %d.\n", failed_attempts, max_attempts);
        lv_label_set_text_fmt(pin_display_label, "Errato! (%d/%d)", failed_attempts, max_attempts);
        delay(1000);  // Pausa per mostrare l'errore
        current_pin_attempt = "";
        lv_label_set_text(pin_display_label, "");
      }
    }
  }
}

// Funzioni per run_backend_tests
void run_backend_tests() {
  USBSerial.println("\n--- Inizio Test Backend (Fase 4) ---");
  const char* key_string = "0123456789abcdef0123456789abcdef";
  unsigned char test_key[32];
  memcpy(test_key, key_string, 32);
  crypto.begin(test_key);
  USBSerial.println("OK: Crypto inizializzato.");
  const char* test_csv_path = "/test_creds.csv";
  File test_file = SD_MMC.open(test_csv_path, FILE_WRITE);
  if (test_file) {
    test_file.println("TITOLO,UTENTE,PASSWORD");
    test_file.println("Sito Web Famoso,test@email.com,PasswordSuperSegreta123!");
    test_file.println("Account Google,daniele,UnAltraPasswordComplessa");
    test_file.close();
    USBSerial.println("OK: Creato file CSV di test.");
  }
  credManager.begin();
  if (credManager.importFromSD(test_csv_path, crypto)) {
    USBSerial.println("SUCCESS: Importazione dal CSV completata.");
    if (!SD_MMC.exists(test_csv_path)) {
      USBSerial.println("OK: Il file CSV di test e' stato cancellato.");
    } else {
      USBSerial.println("FAIL: Il file CSV di test non e' stato cancellato.");
    }
  } else {
    USBSerial.println("FAIL: Importazione dal CSV fallita.");
    return;
  }
  USBSerial.printf("Numero di credenziali salvate: %d\n", credManager.getCount());
  if (credManager.getCount() == 2) {
    Credential test_cred;
    if (credManager.getCredential(1, &test_cred)) {
      USBSerial.printf("OK: Letta credenziale: Titolo='%s', Utente='%s'\n", test_cred.title, test_cred.username);
      String decrypted_pass = crypto.decrypt(test_cred.encrypted_password);
      USBSerial.printf("Password decifrata: '%s'\n", decrypted_pass.c_str());
      if (decrypted_pass == "UnAltraPasswordComplessa") {
        USBSerial.println("SUCCESS: La decifratura corrisponde! Test superato.");
      } else {
        USBSerial.println("FAIL: La password decifrata non corrisponde!");
      }
    }
  } else {
    USBSerial.println("FAIL: Il numero di credenziali salvate non e' corretto.");
  }
  USBSerial.println("--- Fine Test Backend ---");
}

// Aggiungi questa funzione di debug al tuo file .ino

void debug_print_all_credentials() {
  USBSerial.println("\n--- INIZIO DEBUG CREDENZIALI SALVATE ---");

  size_t count = credManager.getCount();
  if (count == 0) {
    USBSerial.println("RISULTATO: Nessuna credenziale trovata da CredentialsManager (getCount() == 0).");
    USBSerial.println("--- FINE DEBUG ---");
    return;
  }

  USBSerial.printf("Trovate %d credenziali da CredentialsManager.\n", count);

  // Inizializza crypto con la chiave per poter decifrare
  // Assicurati che il dispositivo sia sbloccato prima di chiamare questa funzione
  crypto.begin(securityManager.getUserKey());

  for (size_t i = 0; i < count; i++) {
    Credential temp_cred;
    if (credManager.getCredential(i, &temp_cred)) {
      USBSerial.printf("\n--- Credenziale #%d ---\n", i);
      USBSerial.printf("  - Titolo: '%s'\n", temp_cred.title);
      USBSerial.printf("  - Utente: '%s'\n", temp_cred.username);

      String decrypted_pass = crypto.decrypt(temp_cred.encrypted_password);
      if (decrypted_pass.length() > 0) {
        USBSerial.printf("  - Password (decifrata): '%s'\n", decrypted_pass.c_str());
      } else {
        USBSerial.println("  - ERRORE: Impossibile decifrare la password!");
      }
    } else {
      USBSerial.printf("\nERRORE: Impossibile leggere la credenziale #%d dal file.\n", i);
    }
  }
  USBSerial.println("\n--- FINE DEBUG CREDENZIALI SALVATE ---");
}

// Funzione di test aggiuntiva SD
void run_sd_storage_test() {
  USBSerial.println("\n--- Inizio Test di Scrittura/Lettura su File Binario ---");

  // 1. Pulisce la situazione precedente per un test pulito
  credManager.clear();  // Cancella un eventuale 'credentials.bin' esistente
  USBSerial.println("Step 1: Pulizia completata (eventuale file precedente rimosso).");

  // 2. Crea una credenziale fittizia in memoria
  Credential cred_da_scrivere = { 0 };  // Inizializza a zero per pulizia
  strncpy(cred_da_scrivere.title, "Test Titolo", MAX_TITLE_LEN - 1);
  strncpy(cred_da_scrivere.username, "test_utente", MAX_USERNAME_LEN - 1);
  strncpy(cred_da_scrivere.encrypted_password, "questa-e-una-password-finta-criptata", MAX_ENCRYPTED_PASS_LEN - 1);
  USBSerial.println("Step 2: Credenziale di test creata in memoria.");

  // 3. Scrive la credenziale sul file binario
  File binFile = SD_MMC.open(CREDENTIALS_FILE, FILE_WRITE);
  if (!binFile) {
    USBSerial.println("ERRORE: Impossibile aprire credentials.bin in scrittura!");
    return;
  }
  binFile.write((uint8_t*)&cred_da_scrivere, sizeof(Credential));
  binFile.close();
  USBSerial.println("Step 3: La credenziale e' stata scritta su credentials.bin.");

  // 4. Legge la credenziale appena scritta dal file
  credManager.begin();  // Forza il ricalcolo del numero di credenziali
  if (credManager.getCount() != 1) {
    USBSerial.printf("ERRORE: Il conteggio delle credenziali e' %d, ma dovrebbe essere 1!\n", credManager.getCount());
    return;
  }

  Credential cred_letta = { 0 };
  if (!credManager.getCredential(0, &cred_letta)) {
    USBSerial.println("ERRORE: getCredential(0) ha fallito!");
    return;
  }
  USBSerial.println("Step 4: La credenziale e' stata letta da credentials.bin.");

  // 5. Verifica che i dati corrispondano
  USBSerial.println("Step 5: Verifica dei dati...");
  USBSerial.printf("  - Titolo letto: '%s'\n", cred_letta.title);
  USBSerial.printf("  - Utente letto: '%s'\n", cred_letta.username);
  USBSerial.printf("  - Password letta: '%s'\n", cred_letta.encrypted_password);

  if (strcmp(cred_da_scrivere.title, cred_letta.title) == 0 && strcmp(cred_da_scrivere.username, cred_letta.username) == 0 && strcmp(cred_da_scrivere.encrypted_password, cred_letta.encrypted_password) == 0) {
    USBSerial.println("\nSUCCESS: I dati scritti e letti corrispondono perfettamente!");
    USBSerial.println("La logica di storage su SD Card e' CONFERMATA FUNZIONANTE.");
  } else {
    USBSerial.println("\nFAIL: I dati letti non corrispondono a quelli scritti!");
  }
}


// Funzione per creare la schermata del PIN
void create_pin_lock_screen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t* scr = lv_scr_act();
  current_pin_attempt = "";

  // --- Titolo ---
  lv_obj_t* title = lv_label_create(scr);
  if (securityManager.isPinSet()) {
    lv_label_set_text(title, "Inserisci il PIN");
  } else {
    lv_label_set_text(title, "Crea un nuovo PIN a 6 cifre");
  }
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  // --- ECCO LA RIGA MANCANTE ---
  lv_obj_set_style_text_color(title, lv_color_white(), 0);  // Imposta il colore del testo su bianco

  // --- Display del PIN (asterischi) ---
  pin_display_label = lv_label_create(scr);
  lv_label_set_text(pin_display_label, "");
  lv_obj_set_width(pin_display_label, LV_PCT(100));
  lv_obj_set_style_text_align(pin_display_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(pin_display_label, LV_ALIGN_TOP_MID, 0, 55);
  lv_obj_set_style_text_font(pin_display_label, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_letter_space(pin_display_label, 5, 0);
  lv_obj_set_style_text_color(pin_display_label, lv_color_white(), 0);  // Rendiamo bianchi anche i puntini

  // --- Tastierino numerico (il resto della funzione rimane invariato) ---
  static const char* btnm_map[] = { "1", "2", "3", "\n",
                                    "4", "5", "6", "\n",
                                    "7", "8", "9", "\n",
                                    LV_SYMBOL_BACKSPACE, "0", "", "" };

  lv_obj_t* btnm = lv_btnmatrix_create(scr);
  lv_btnmatrix_set_map(btnm, btnm_map);
  lv_obj_set_width(btnm, lv_pct(95));
  lv_obj_set_height(btnm, lv_pct(60));
  lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, -50);
  lv_obj_add_event_cb(btnm, pin_matrix_event_cb, LV_EVENT_CLICKED, NULL);

  static lv_style_t style_btn;
  lv_style_init(&style_btn);
  lv_style_set_text_font(&style_btn, &lv_font_montserrat_22);
  lv_obj_add_style(btnm, &style_btn, LV_PART_ITEMS);
}


// Funzione per preparare e ordinare i dati per la UI
void prepare_credential_data() {
  sorted_credentials.clear();
  for (size_t i = 0; i < credManager.getCount(); ++i) {
    Credential temp;
    if (credManager.getCredential(i, &temp)) {
      sorted_credentials.push_back({ String(temp.title), i });
    }
  }
  // Ordina il vettore 'sorted_credentials' in base al titolo, ignorando maiuscole/minuscole.
  std::sort(sorted_credentials.begin(), sorted_credentials.end(), [](const CredentialInfo& a, const CredentialInfo& b) {
    return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
  });
}

// Funzione per aggiornare l'orologio
void update_time_task(lv_timer_t* timer) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    lv_label_set_text_fmt(time_label, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    lv_label_set_text(time_label, "--:--");
  }
}

void create_main_screen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // --- 1. NUOVA BARRA DI STATO SUPERIORE ---
  lv_obj_t* status_bar = lv_obj_create(scr);
  lv_obj_remove_style_all(status_bar);  // Rimuovi stili di default per un look pulito
  lv_obj_set_size(status_bar, lv_pct(100), 30);
  lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x1C1C1C), 0);  // Sfondo grigio scuro
  lv_obj_set_style_pad_hor(status_bar, 10, 0);                       // Padding orizzontale

  lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

  // --- MODIFICA POSIZIONE ---
  // Aggiungiamo un padding orizzontale di 15 pixel. Questo spingerà
  // le etichette allineate a sinistra e destra verso l'interno.
  lv_obj_set_style_pad_hor(status_bar, 20, 0);

  // Etichetta per il Layout (a sinistra)
  layout_status_label = lv_label_create(status_bar);
  lv_obj_set_style_text_color(layout_status_label, lv_color_hex(0xAAAAAA), 0);  // Colore grigio chiaro
  lv_obj_align(layout_status_label, LV_ALIGN_LEFT_MID, 0, 0);

  // --- MODIFICA FONT ---
  lv_obj_set_style_text_font(layout_status_label, &lv_font_montserrat_20, 0);

  // Etichetta per l'OS (a destra)
  os_status_label = lv_label_create(status_bar);
  lv_obj_set_style_text_color(os_status_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(os_status_label, LV_ALIGN_RIGHT_MID, 0, 0);

  // --- MODIFICA FONT ---
  lv_obj_set_style_text_font(os_status_label, &lv_font_montserrat_20, 0);

  // Aggiorna il testo delle etichette con i valori correnti
  update_status_bar();
  // ------------------------------------

  // --- 2. Lista Credenziali a Rullo (leggermente modificato) ---
  prepare_credential_data();
  String roller_options = "";
  for (const auto& info : sorted_credentials) {
    roller_options += info.title;
    roller_options += "\n";
  }
  if (!roller_options.isEmpty()) {
    roller_options.remove(roller_options.length() - 1);
  }
  credential_roller = lv_roller_create(scr);
  lv_roller_set_options(credential_roller, roller_options.c_str(), LV_ROLLER_MODE_NORMAL);
  // Riduciamo leggermente la larghezza per fare più spazio
  lv_obj_set_width(credential_roller, lv_pct(88));
  lv_obj_set_height(credential_roller, 220);
  // Spostiamo un po' più a sinistra per bilanciare
  lv_obj_align(credential_roller, LV_ALIGN_CENTER, -25, 15);
  // Stili (invariati)
  lv_roller_set_visible_row_count(credential_roller, 4);
  lv_obj_set_style_bg_color(credential_roller, lv_color_black(), 0);
  lv_obj_set_style_border_width(credential_roller, 0, 0);
  lv_obj_set_style_text_font(credential_roller, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(credential_roller, lv_color_white(), 0);
  lv_obj_set_style_text_opa(credential_roller, LV_OPA_70, 0);
  lv_obj_set_style_text_align(credential_roller, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_style_pad_left(credential_roller, 10, 0);
  lv_obj_set_style_text_font(credential_roller, &lv_font_montserrat_28, LV_PART_SELECTED);
  lv_obj_set_style_text_opa(credential_roller, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_bg_color(credential_roller, lv_color_hex(0x282828), LV_PART_SELECTED);


  // --- 3. SCROLLER ALFABETICO IBRIDO (NUOVA LOGICA) ---
  lv_obj_t* jump_bar = lv_obj_create(scr);
  lv_obj_remove_style_all(jump_bar);
  lv_obj_set_size(jump_bar, 45, 220);
  lv_obj_align_to(jump_bar, credential_roller, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  // Aggiungiamo l'area di tocco invisibile che cattura l'evento di trascinamento
  lv_obj_add_event_cb(jump_bar, continuous_alphabet_scroller_cb, LV_EVENT_PRESSING, NULL);

  // Creiamo le etichette visive (A, N, Z) che NON sono cliccabili
  lv_obj_t* label_a = lv_label_create(jump_bar);
  lv_label_set_text(label_a, "A");
  lv_obj_align(label_a, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_set_style_text_color(label_a, lv_color_hex(0xAAAAAA), 0);

  lv_obj_t* label_n = lv_label_create(jump_bar);
  lv_label_set_text(label_n, "N");
  lv_obj_align(label_n, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(label_n, lv_color_hex(0xAAAAAA), 0);

  lv_obj_t* label_z = lv_label_create(jump_bar);
  lv_label_set_text(label_z, "Z");
  lv_obj_align(label_z, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_text_color(label_z, lv_color_hex(0xAAAAAA), 0);


  // --- 2. Barra di Navigazione Inferiore  ---
  lv_obj_t* nav_bar = lv_obj_create(scr);
  lv_obj_set_size(nav_bar, lv_pct(100), 70);
  lv_obj_align(nav_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0x111111), 0);
  lv_obj_set_style_border_width(nav_bar, 0, 0);
  lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Rimuoviamo la proprietà "scorrevole" dalla barra di navigazione.
  lv_obj_clear_flag(nav_bar, LV_OBJ_FLAG_SCROLLABLE);

  // Pulsante Indietro (Blocca)
  lv_obj_t* btn_back = lv_btn_create(nav_bar);
  lv_obj_set_size(btn_back, 60, 50);
  lv_obj_add_event_cb(
    btn_back, [](lv_event_t* e) {
      securityManager.lock();
      create_pin_lock_screen();
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* label_back = lv_label_create(btn_back);
  lv_label_set_text(label_back, LV_SYMBOL_LEFT);
  lv_obj_center(label_back);

  // --- NUOVO PULSANTE "VISUALIZZA" ---
  lv_obj_t* btn_view = lv_btn_create(nav_bar);
  lv_obj_set_size(btn_view, 60, 50);
  lv_obj_add_event_cb(
    btn_view, [](lv_event_t* e) {
      if (credential_roller == NULL) return;
      uint16_t selected_idx = lv_roller_get_selected(credential_roller);
      size_t original_idx = sorted_credentials[selected_idx].original_index;
      show_credential_details_popup(original_idx);
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* label_view = lv_label_create(btn_view);
  lv_label_set_text(label_view, LV_SYMBOL_EYE_OPEN);  // Icona a forma di occhio
  lv_obj_center(label_view);
  // ------------------------------------

  // --- PULSANTE INVIA CON LOGICA CONDIZIONALE ---
  lv_obj_t* btn_send = lv_btn_create(nav_bar);
  lv_obj_set_size(btn_send, 80, 60);

  // Gestore eventi
  lv_obj_add_event_cb(
    btn_send, [](lv_event_t* e) {
      // Se siamo in modalità tastiera, esegui la normale logica di invio
      if (millis() - g_last_send_press_time < SEND_BUTTON_COOLDOWN) {
        return;
      }
      g_last_send_press_time = millis();

      // Controlla la modalità USB *prima* di tutto
      if (!settingsManager.isHidModeEnabled()) {
        // Se siamo in modalità seriale, non creiamo il popup direttamente.
        // Creiamo un timer "one-shot" che verrà eseguito una sola volta dopo 10ms.
        lv_timer_create(show_serial_mode_warning_popup, 10, NULL)->repeat_count = 1;
        return;
      }


      if (credential_roller == NULL) return;
      uint16_t selected_idx = lv_roller_get_selected(credential_roller);
      size_t original_idx = sorted_credentials[selected_idx].original_index;
      Credential cred;
      if (credManager.getCredential(original_idx, &cred)) {
        USBSerial.printf("Pulsante 'Invia' premuto. Digitazione password per: %s\n", cred.title);
        String password = crypto.decrypt(cred.encrypted_password);
        if (password.length() > 0) {
          type_password_with_layout(password.c_str());
        }
        USBSerial.println("INFO: Digitazione completata.");
      }
    },
    LV_EVENT_CLICKED, NULL);

  // Stile del pulsante
  lv_obj_t* label_send = lv_label_create(btn_send);
  lv_label_set_text(label_send, LV_SYMBOL_UPLOAD);
  lv_obj_center(label_send);
  lv_obj_set_style_text_font(label_send, &lv_font_montserrat_24, 0);

  // NUOVA LOGICA: Applica uno stile "disabilitato" senza disabilitare il click
  if (!settingsManager.isHidModeEnabled()) {
    // Rimuoviamo la riga che disabilitava il pulsante:
    // lv_obj_add_state(btn_send, LV_STATE_DISABLED); // <-- RIMOSSA

    // E applichiamo invece uno stile personalizzato per farlo sembrare grigio
    lv_obj_set_style_bg_color(btn_send, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_opa(btn_send, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(label_send, lv_color_hex(0x999999), 0);
  }
  // ----------------------------------------------------

  // Pulsante Impostazioni
  lv_obj_t* btn_settings = lv_btn_create(nav_bar);
  lv_obj_set_size(btn_settings, 60, 50);
  lv_obj_add_event_cb(
    btn_settings, [](lv_event_t* e) {
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* label_settings = lv_label_create(btn_settings);
  lv_label_set_text(label_settings, LV_SYMBOL_SETTINGS);
  lv_obj_center(label_settings);
}

void change_to_main_screen_cb(lv_timer_t* timer) {
  create_main_screen();
  lv_timer_del(timer);
}

// --- Funzioni di basso livello LVGL (invariate) ---
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);
  lv_disp_flush_ready(disp);
}
// Nel tuo file .ino

// --- SOSTITUISCI LA FUNZIONE ESISTENTE CON QUESTA VERSIONE CORRETTA ---
void my_touchpad_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data) {
  static int32_t last_x = 0;
  static int32_t last_y = 0;

  // --- CONTROLLO LOGICO DELLO STATO ---
  // Se il nostro flag dice che il display è spento, ignora completamente
  // qualsiasi interrupt e segnala a LVGL che non c'è stato alcun tocco.
  if (is_display_off) {
    data->state = LV_INDEV_STATE_REL;  // Stato = Rilasciato
    return;
  }
  // ------------
  // Questa funzione ora si occupa solo di leggere le coordinate quando il touch è attivo
  if (FT3168->IIC_Interrupt_Flag == true) {
    lv_disp_trig_activity(NULL);
    FT3168->IIC_Interrupt_Flag = false;

    last_x = FT3168->IIC_Read_Device_Value(Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    last_y = FT3168->IIC_Read_Device_Value(Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  data->point.x = last_x;
  data->point.y = last_y;
}

void example_increase_lvgl_tick(void* arg) {
  lv_tick_inc(2);
}

void create_usb_mode_screen() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Pulsante Indietro
  lv_obj_t* back_btn = lv_btn_create(scr);
  lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_event_cb(
    back_btn, [](lv_event_t* e) {
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Impostazioni");

  // Testo informativo
  lv_obj_t* info_label = lv_label_create(scr);
  bool is_enabled = settingsManager.isHidModeEnabled();
  lv_label_set_text_fmt(info_label, "Stato Attuale: %s\n\nCambiare questa impostazione richiede un riavvio del dispositivo.", is_enabled ? "Tastiera" : "Solo Seriale");
  lv_obj_set_style_text_color(info_label, lv_color_white(), 0);
  lv_obj_set_width(info_label, lv_pct(90));
  lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 60);
  lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);

  // Pulsante per avviare il cambio di modalità
  lv_obj_t* toggle_btn = lv_btn_create(scr);
  lv_obj_center(toggle_btn);
  lv_obj_set_y(toggle_btn, lv_obj_get_y(toggle_btn) + 40);
  lv_obj_add_event_cb(
    toggle_btn, [](lv_event_t* e) {
      // Definiamo i testi per i pulsanti del nostro popup
      static const char* btns[] = { "Annulla", "OK", "" };

      // Creiamo un popup di conferma che chiede all'utente cosa fare
      lv_obj_t* mbox = lv_msgbox_create(NULL, "Conferma Riavvio", "L'impostazione verra' salvata e il dispositivo sara' riavviato.\n\nProcedere?", btns, true);
      lv_obj_center(mbox);

      // Aggiungiamo un gestore di eventi ai pulsanti del popup
      lv_obj_add_event_cb(
        mbox, [](lv_event_t* event) {
          lv_obj_t* current_mbox = lv_event_get_current_target(event);
          uint16_t btn_id = lv_msgbox_get_active_btn(current_mbox);

          if (btn_id == 0) {  // Annulla
            lv_msgbox_close(current_mbox);
            return;
          }

          if (btn_id == 1) {  // OK
            lv_obj_clear_flag(lv_msgbox_get_btns(current_mbox), LV_OBJ_FLAG_CLICKABLE);
            lv_label_set_text(lv_msgbox_get_text(current_mbox), "Salvataggio e riavvio in corso...");
            lv_timer_handler();

            settingsManager.setHidMode(!settingsManager.isHidModeEnabled());
            delay(100);

            // --- RIAVVIO HARDWARE TRAMITE PMIC (FUNZIONE CORRETTA) ---
            USBSerial.println("Eseguo un riavvio hardware tramite PMU...");
            pmu.reset();
            // ----------------------------------------------------
          }
        },
        LV_EVENT_VALUE_CHANGED, NULL);
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* toggle_label = lv_label_create(toggle_btn);
  lv_label_set_text_fmt(toggle_label, "Passa a: %s", is_enabled ? "Solo Seriale" : "Tastiera");

  lv_scr_load(scr);
}

void create_settings_screen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Titolo della schermata
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Impostazioni");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  // --- CREAZIONE DELLA LISTA ---
  // Questo era il pezzo mancante nel contesto precedente.
  // Creiamo la lista prima di aggiungerci i pulsanti.
  lv_obj_t* settings_list = lv_list_create(scr);
  lv_obj_set_size(settings_list, lv_pct(90), lv_pct(70));
  lv_obj_center(settings_list);

  // --- AGGIUNTA PULSANTI ALLA LISTA ---

  // Sezione Dispositivo
  lv_list_add_text(settings_list, "Dispositivo");
  lv_obj_t* usb_btn = lv_list_add_btn(settings_list, LV_SYMBOL_USB, "Modalita' USB");
  lv_obj_add_event_cb(usb_btn, open_usb_mode_screen_cb, LV_EVENT_CLICKED, NULL);

  // --- NUOVO PULSANTE SICUREZZA ---
  lv_obj_t* wipe_btn = lv_list_add_btn(settings_list, LV_SYMBOL_WARNING, "Auto-distruzione");
  lv_obj_add_event_cb(
    wipe_btn, [](lv_event_t* e) {
      create_wipe_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  // --------------------------------

// --- NUOVO PULSANTE PER L'IMPORTAZIONE MANUALE ---
    lv_obj_t* import_btn = lv_list_add_btn(settings_list, LV_SYMBOL_DOWNLOAD, "Importa da CSV");
    lv_obj_add_event_cb(import_btn, [](lv_event_t* e){
        // Mostra un popup di conferma
        static const char* btns[] = {"Annulla", "Importa", ""};
        lv_obj_t* mbox = lv_msgbox_create(NULL, "Importa Credenziali", 
                                          "Cercare il file 'import.csv' sulla SD Card e aggiungere le nuove credenziali?\n\n(Le credenziali esistenti non verranno modificate)", 
                                          btns, true);
        lv_obj_center(mbox);

        lv_obj_add_event_cb(mbox, [](lv_event_t * event) {
            lv_obj_t* current_mbox = lv_event_get_current_target(event);
            uint16_t btn_id = lv_msgbox_get_active_btn(current_mbox);
            if (btn_id == 1) { // Se l'utente preme "Importa"
                checkForAndRunImport(); // Avvia l'importazione
                // Dopo l'importazione, torniamo alla schermata principale che si aggiornerà
                create_main_screen();
            }
            lv_msgbox_close(current_mbox);
        }, LV_EVENT_VALUE_CHANGED, NULL);

    }, LV_EVENT_CLICKED, NULL);
    // ------------------------------------------------


  // Sezione Input
  lv_list_add_text(settings_list, "Input");

  lv_obj_t* os_btn = lv_list_add_btn(settings_list, LV_SYMBOL_SETTINGS, "Sistema Operativo");
  lv_obj_add_event_cb(
    os_btn, [](lv_event_t* e) {
      create_os_selection_screen();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* layout_btn = lv_list_add_btn(settings_list, LV_SYMBOL_KEYBOARD, "Layout Tastiera");
  // Al momento la selezione del layout non è implementata, quindi il pulsante è disabilitato
  lv_obj_add_state(layout_btn, LV_STATE_DISABLED);
  // lv_obj_add_event_cb(layout_btn, [](lv_event_t* e){ create_layout_selection_screen(); }, LV_EVENT_CLICKED, NULL);


  // --- PULSANTE "INDIETRO" PIÙ ROBUSTO ---
  lv_obj_t* back_btn = lv_btn_create(scr);
  lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);

  // 1. Portiamo il pulsante in primo piano per assicurarci che riceva il click
  lv_obj_move_foreground(back_btn);

  lv_obj_add_event_cb(
    back_btn, [](lv_event_t* e) {
      // 2. Aggiungiamo un messaggio di debug per essere sicuri che venga chiamata la funzione giusta
      USBSerial.println("DEBUG: Pulsante 'Indietro' da Impostazioni premuto. Chiamo create_main_screen().");
      create_main_screen();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Indietro");
}


// --- FUNZIONE MANCANTE, ORA AGGIUNTA IN FONDO AL FILE ---
void type_password_with_layout(const char* password) {
  delay(200);

  KeyboardLayout current_layout = settingsManager.getKeyboardLayout();
  TargetOS current_os = settingsManager.getTargetOS();

  size_t map_size = 0;
  const KeyMapping* map = get_layout_map(current_layout, current_os, &map_size);

  int i = 0;
  int pass_len = strlen(password);

  while (i < pass_len) {
    bool typed_specially = false;

    if (map != nullptr) {
      for (size_t j = 0; j < map_size; j++) {
        const char* char_to_find = map[j].character_utf8;
        int char_len = strlen(char_to_find);

        if (i + char_len <= pass_len && strncmp(&password[i], char_to_find, char_len) == 0) {

          // --- NUOVA LOGICA DI PRESSIONE MANUALE E CONTROLLATA ---

          // 1. Premi i tasti modificatori
          if (map[j].modifier1 != KEY_NO_MOD) Keyboard.press(map[j].modifier1);
          delay(10);
          if (map[j].modifier2 != KEY_NO_MOD) Keyboard.press(map[j].modifier2);
          delay(10);


          // 2. "Tocca" (premi e rilascia) il tasto principale mentre i modificatori sono premuti
          Keyboard.press(map[j].primary_key);
          delay(40);  // Un piccolo delay per assicurare che la pressione sia registrata
          Keyboard.release(map[j].primary_key);
          delay(10);


          // 3. Rilascia i tasti modificatori nell'ordine inverso
          if (map[j].modifier2 != KEY_NO_MOD) Keyboard.release(map[j].modifier2);
          delay(10);
          if (map[j].modifier1 != KEY_NO_MOD) Keyboard.release(map[j].modifier1);
          delay(10);
          // --------------------------------------------------------

          typed_specially = true;
          i += char_len;
          break;
        }
      }
    }

    if (!typed_specially) {
      // La gestione dei caratteri normali rimane la stessa
      Keyboard.print(password[i]);
      i++;
    }

    delay(30);
  }
}
// Aggiungi anche questo callback
void open_usb_mode_screen_cb(lv_event_t* e) {
  create_usb_mode_screen();
}


void checkForAndRunImport() {
  const char* import_filepath = "/passwords.csv";
  if (SD_MMC.exists(import_filepath)) {
    USBSerial.printf("Trovato file da importare dopo sblocco: %s\n", import_filepath);

    lv_obj_t* mbox = lv_msgbox_create(NULL, "Importazione", "Trovato file 'import.csv'.\nImportazione in corso...", NULL, false);
    lv_obj_center(mbox);
    lv_timer_handler();
    delay(50);

    bool import_success = credManager.importFromSD(import_filepath, crypto);

    if (import_success) {
      // --- ECCO IL PASSAGGIO CHIAVE ---
      // Ora che l'importazione è finita, diciamo esplicitamente
      // al manager di ricaricare il suo stato dal file.
      USBSerial.println("INFO: L'importazione sembra riuscita. Ricarico lo stato del CredentialsManager...");
      credManager.begin();
      // -----------------------------

      USBSerial.println("SUCCESS: Importazione e ricarica completate!");

      String imported_path = String(import_filepath) + ".imported";
      if (SD_MMC.exists(imported_path)) {
        SD_MMC.remove(imported_path);
      }
      SD_MMC.rename(import_filepath, imported_path);
      lv_label_set_text(lv_msgbox_get_text(mbox), "Importazione completata con successo!");

    } else {
      USBSerial.println("FAIL: Errore durante l'importazione.");
      lv_label_set_text(lv_msgbox_get_text(mbox), "Errore durante l'importazione.\nControlla la console seriale.");
    }

    delay(2500);
    lv_msgbox_close(mbox);
  }
}

// Callback per il tastierino numerico della schermata di cambio PIN
void change_pin_keypad_event_cb(lv_event_t* e) {
  lv_obj_t* btnm = lv_event_get_target(e);
  uint32_t id = lv_btnmatrix_get_selected_btn(btnm);
  const char* txt = lv_btnmatrix_get_btn_text(btnm, id);
  if (txt == NULL) return;

  if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    if (change_pin_input_buffer.length() > 0) {
      change_pin_input_buffer.remove(change_pin_input_buffer.length() - 1);
    }
  } else if (change_pin_input_buffer.length() < 6) {
    change_pin_input_buffer += txt;
  }

  String dots = "";
  for (unsigned int i = 0; i < change_pin_input_buffer.length(); i++) dots += "• ";
  lv_label_set_text(change_pin_label_dots, dots.c_str());

  if (change_pin_input_buffer.length() == 6) {
    lv_timer_handler();
    delay(50);

    switch (current_change_pin_state) {
      case ChangePinState::AWAITING_OLD_PIN:
        {
          old_pin_input = change_pin_input_buffer;
          current_change_pin_state = ChangePinState::AWAITING_NEW_PIN;
          lv_label_set_text(change_pin_label_title, "Inserisci il NUOVO PIN");
          break;
        }

      case ChangePinState::AWAITING_NEW_PIN:
        {
          new_pin_storage = change_pin_input_buffer;
          current_change_pin_state = ChangePinState::AWAITING_CONFIRM_PIN;
          lv_label_set_text(change_pin_label_title, "CONFERMA il nuovo PIN");
          break;
        }

      case ChangePinState::AWAITING_CONFIRM_PIN:
        {
          if (change_pin_input_buffer == new_pin_storage) {
            bool success = securityManager.changePin(old_pin_input, new_pin_storage, credManager, crypto);
            if (success) {
              lv_obj_t* mbox = lv_msgbox_create(NULL, "Successo!", "PIN cambiato.\n\nIl dispositivo si blocchera' a breve.", NULL, false);
              lv_obj_center(mbox);

              // --- CORREZIONE DEL BUG DEL WATCHDOG ---
              // Usiamo un timer non bloccante invece di delay()
              lv_timer_create(post_pin_change_reboot_cb, 2000, mbox);

            } else {
              // Mostra un popup di errore
              lv_obj_t* mbox = lv_msgbox_create(NULL, "Errore", "Cambio PIN fallito.\nControlla il vecchio PIN.", NULL, true);
              lv_obj_center(mbox);
              // Dopo l'errore, torna alla schermata impostazioni per ricominciare
              delay(2000);  // Qui un delay è accettabile perché è un caso di errore terminale per questo flusso
              create_settings_screen();
            }
          } else {
            lv_label_set_text(change_pin_label_title, "I PIN non corrispondono!\n\nRiprova con il NUOVO PIN");
            current_change_pin_state = ChangePinState::AWAITING_NEW_PIN;
          }
          break;
        }
    }
    change_pin_input_buffer = "";
    lv_label_set_text(change_pin_label_dots, "");
  }
}

void create_change_pin_flow_screen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  current_change_pin_state = ChangePinState::AWAITING_OLD_PIN;
  change_pin_input_buffer = "";
  old_pin_input = "";
  new_pin_storage = "";

  // --- Titolo dinamico ---
  change_pin_label_title = lv_label_create(scr);
  lv_label_set_text(change_pin_label_title, "Inserisci il VECCHIO PIN");
  lv_obj_set_style_text_font(change_pin_label_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(change_pin_label_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(change_pin_label_title, lv_pct(90));
  lv_obj_align(change_pin_label_title, LV_ALIGN_TOP_MID, 0, 30);

  // --- ECCO LA RIGA MANCANTE ---
  lv_obj_set_style_text_color(change_pin_label_title, lv_color_white(), 0);  // Imposta il colore del testo su bianco

  // --- Label per i pallini/asterischi ---
  change_pin_label_dots = lv_label_create(scr);
  lv_label_set_text(change_pin_label_dots, "");
  lv_obj_set_style_text_font(change_pin_label_dots, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_letter_space(change_pin_label_dots, 8, 0);
  lv_obj_set_width(change_pin_label_dots, LV_PCT(100));
  lv_obj_set_style_text_align(change_pin_label_dots, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(change_pin_label_dots, LV_ALIGN_TOP_MID, 0, 85);
  lv_obj_set_style_text_color(change_pin_label_dots, lv_color_white(), 0);  // Rendiamo bianchi anche i pallini

  // --- Tastierino numerico e pulsante Annulla (il resto della funzione rimane invariato) ---
  static const char* btnm_map[] = { "1", "2", "3", "\n",
                                    "4", "5", "6", "\n",
                                    "7", "8", "9", "\n",
                                    LV_SYMBOL_BACKSPACE, "0", "", "" };

  lv_obj_t* btnm = lv_btnmatrix_create(scr);
  lv_btnmatrix_set_map(btnm, btnm_map);
  lv_obj_set_size(btnm, lv_pct(95), lv_pct(60));
  lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(btnm, change_pin_keypad_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t* cancel_btn = lv_btn_create(scr);
  lv_obj_align(cancel_btn, LV_ALIGN_TOP_LEFT, 10, 10);
  lv_obj_add_event_cb(
    cancel_btn, [](lv_event_t* e) {
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* cancel_label = lv_label_create(cancel_btn);
  lv_label_set_text(cancel_label, LV_SYMBOL_LEFT);
}

void post_pin_change_reboot_cb(lv_timer_t* timer) {
  // 1. Recupera l'oggetto mbox dai dati utente del timer
  lv_obj_t* mbox = (lv_obj_t*)timer->user_data;

  // 2. ECCO IL PASSAGGIO CHIAVE: Chiudi esplicitamente il popup
  lv_msgbox_close(mbox);

  // 3. Ora esegui le azioni che servono (blocca e cambia schermo)
  securityManager.lock();
  create_pin_lock_screen();

  // 4. Elimina il timer per renderlo "one-shot"
  lv_timer_del(timer);
}

void create_layout_selection_screen() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_clean(scr);

  // ... (Crea una lista con i vari layout: ITALIANO, TEDESCO, etc.)
  // Esempio per un pulsante:
  lv_obj_t* btn_it = lv_btn_create(scr);
  // ...
  lv_obj_add_event_cb(
    btn_it, [](lv_event_t* e) {
      settingsManager.setKeyboardLayout(KeyboardLayout::ITALIANO);
      // Mostra messaggio e torna alle impostazioni
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  // ...
}

// --- Schermata per selezionare l'OS ---
void create_os_selection_screen() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Seleziona S.O.");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  lv_obj_t* list = lv_list_create(scr);
  lv_obj_set_size(list, lv_pct(90), lv_pct(70));
  lv_obj_center(list);

  lv_obj_t* btn_win = lv_list_add_btn(list, LV_SYMBOL_DRIVE, "Windows / Linux");
  lv_obj_add_event_cb(
    btn_win, [](lv_event_t* e) {
      settingsManager.setTargetOS(TargetOS::WINDOWS_LINUX);
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);

  lv_obj_t* btn_mac = lv_list_add_btn(list, LV_SYMBOL_DRIVE, "macOS");
  lv_obj_add_event_cb(
    btn_mac, [](lv_event_t* e) {
      settingsManager.setTargetOS(TargetOS::MACOS);
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
}

void update_status_bar() {
  if (!layout_status_label || !os_status_label) return;  // Controllo di sicurezza

  // Aggiorna l'etichetta del Layout
  KeyboardLayout current_layout = settingsManager.getKeyboardLayout();
  const char* layout_text = "N/D";
  switch (current_layout) {
    case KeyboardLayout::ITALIANO: layout_text = "Layout: IT"; break;
    case KeyboardLayout::TEDESCO: layout_text = "Layout: DE"; break;
    case KeyboardLayout::FRANCESE: layout_text = "Layout: FR"; break;
    case KeyboardLayout::SPAGNOLO: layout_text = "Layout: ES"; break;
    case KeyboardLayout::USA: layout_text = "Layout: US"; break;
  }
  lv_label_set_text(layout_status_label, layout_text);

  // Aggiorna l'etichetta del Sistema Operativo
  TargetOS current_os = settingsManager.getTargetOS();
  const char* os_text = "OS: N/D";
  switch (current_os) {
    case TargetOS::WINDOWS_LINUX: os_text = "OS: WIN"; break;
    case TargetOS::MACOS: os_text = "OS: MAC"; break;
  }
  lv_label_set_text(os_status_label, os_text);
}

// Funzione di callback per lo scroller alfabetico
static void alphabet_scroller_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t* obj = lv_event_get_target(e);  // Questa è l'etichetta dell'alfabeto

  if (code == LV_EVENT_PRESSING) {
    lv_point_t point;
    // Ottieni le coordinate assolute del tocco sullo schermo
    lv_indev_get_point(lv_indev_get_act(), &point);

    lv_area_t area;
    // Ottieni le coordinate della nostra etichetta
    lv_obj_get_coords(obj, &area);

    // Calcola la posizione Y relativa del tocco all'interno dell'etichetta
    int32_t relative_y = point.y - area.y1;

    // Ottieni il font e l'altezza di una riga (carattere + spazio)
    const lv_font_t* font = lv_obj_get_style_text_font(obj, 0);
    lv_coord_t line_height = lv_font_get_line_height(font) + lv_obj_get_style_text_line_space(obj, 0);

    // Evita una divisione per zero se l'altezza della riga è 0
    if (line_height <= 0) return;

    // Calcola l'indice della lettera toccata (0=A, 1=B, etc.)
    int16_t letter_index = relative_y / line_height;

    // Limita l'indice tra 0 e 25 per sicurezza
    if (letter_index < 0) letter_index = 0;
    if (letter_index > 25) letter_index = 25;

    // Esegui il resto della logica solo se il dito si è spostato su una nuova lettera
    static int16_t last_letter_index = -1;
    if (letter_index == last_letter_index) return;
    last_letter_index = letter_index;

    // Ottieni il carattere selezionato
    char selected_char = 'A' + letter_index;

    // Cerca la prima credenziale che inizia con quella lettera
    for (size_t i = 0; i < sorted_credentials.size(); ++i) {
      if (sorted_credentials[i].title.length() > 0 && toupper(sorted_credentials[i].title[0]) == selected_char) {
        // Trovata! Imposta il roller su questa posizione
        lv_roller_set_selected(credential_roller, i, LV_ANIM_OFF);  // LV_ANIM_OFF per un salto istantaneo
        return;                                                     // Esci dopo aver trovato la prima occorrenza
      }
    }
  }
}

// Funzione di callback per i pulsanti di salto rapido (A, N, Z)
static void quick_jump_event_cb(lv_event_t* e) {
  // Ottieni la lettera dal testo del pulsante che è stato premuto
  const char* letter_char = lv_label_get_text(lv_obj_get_child(lv_event_get_target(e), 0));
  if (!letter_char) return;
  char selected_char = letter_char[0];

  // Cerca la prima credenziale che inizia con quella lettera
  for (size_t i = 0; i < sorted_credentials.size(); ++i) {
    if (sorted_credentials[i].title.length() > 0 && toupper(sorted_credentials[i].title[0]) == selected_char) {
      // Trovata! Imposta il roller su questa posizione
      lv_roller_set_selected(credential_roller, i, LV_ANIM_ON);  // Usiamo un'animazione per un effetto più gradevole
      return;
    }
  }
}

// Aggiungi o sostituisci questa funzione nel tuo .ino

static void continuous_alphabet_scroller_cb(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target(e);  // L'area di tocco invisibile

  lv_point_t point;
  lv_indev_get_point(lv_indev_get_act(), &point);

  lv_area_t area;
  lv_obj_get_coords(obj, &area);

  // Calcola la posizione Y del tocco (da 0.0 a 1.0) all'interno dell'area
  float press_ratio = (float)(point.y - area.y1) / (float)lv_area_get_height(&area);

  // Limita il valore tra 0 e 1 per sicurezza
  if (press_ratio < 0.0f) press_ratio = 0.0f;
  if (press_ratio > 1.0f) press_ratio = 1.0f;

  // Mappa il rapporto sulla lettera dell'alfabeto (da 0 a 25)
  int16_t letter_index = (int16_t)(press_ratio * 25.0f);

  // Esegui il resto della logica solo se il dito si è spostato su una nuova lettera
  static int16_t last_letter_index = -1;
  if (letter_index == last_letter_index) return;
  last_letter_index = letter_index;

  char selected_char = 'A' + letter_index;

  // Cerca la prima credenziale che inizia con quella lettera
  for (size_t i = 0; i < sorted_credentials.size(); ++i) {
    if (sorted_credentials[i].title.length() > 0 && toupper(sorted_credentials[i].title[0]) == selected_char) {
      lv_roller_set_selected(credential_roller, i, LV_ANIM_OFF);  // Salto istantaneo per reattività
      return;
    }
  }
}

void show_serial_mode_warning_popup(lv_timer_t* timer) {
  // Crea il popup
  static const char* btns[] = { "OK", "" };
  lv_obj_t* mbox = lv_msgbox_create(NULL, "Modalita' Seriale Attiva", "Per inviare la password, attiva la modalita' tastiera dalle impostazioni.", btns, true);
  lv_obj_center(mbox);

  // Aggiungi l'evento per chiudere il popup quando si preme "OK"
  lv_obj_add_event_cb(
    mbox, [](lv_event_t* event) {
      lv_msgbox_close(lv_event_get_current_target(event));
    },
    LV_EVENT_VALUE_CHANGED, NULL);

  // Non è necessario cancellare il timer perché è già one-shot
}

// Aggiungi questa nuova funzione al tuo file .ino

void show_credential_details_popup(size_t credential_index) {
  // 1. Recupera la credenziale completa
  Credential cred;
  if (!credManager.getCredential(credential_index, &cred)) {
    // Mostra un errore se non riusciamo a leggere la credenziale
    lv_obj_t* err_box = lv_msgbox_create(NULL, "Errore", "Impossibile recuperare i dati della credenziale.", NULL, true);
    lv_obj_center(err_box);
    return;
  }

  // 2. Decifra la password
  String password = crypto.decrypt(cred.encrypted_password);
  if (password.length() == 0) {
    password = "[Errore Decifratura]";
  }

  // --- NUOVA LOGICA DI CREAZIONE DEL POPUP ---

  // 2. Crea un "message box" di base senza testo, solo con il titolo e il pulsante Chiudi.
  static const char* btns[] = { "Chiudi", "" };
  lv_obj_t* mbox = lv_msgbox_create(NULL, cred.title, "", btns, true);

  // Impostiamo la larghezza del popup all'85% dello schermo
  lv_obj_set_width(mbox, lv_pct(85));
  lv_obj_center(mbox);  // Ria-centriamo dopo aver impostato la larghezza


  // Aggiungi subito l'evento per chiudere il popup
  lv_obj_add_event_cb(
    mbox, [](lv_event_t* event) {
      lv_msgbox_close(lv_event_get_current_target(event));
    },
    LV_EVENT_VALUE_CHANGED, NULL);

  // Ottieni il contenitore del testo del message box
  lv_obj_t* content = lv_msgbox_get_content(mbox);

  // 3. Usa un layout a colonne per organizzare le etichette
  lv_obj_set_layout(content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_gap(content, 10, 0);  // Spazio tra le righe

  // 4. Aggiungi le etichette una per una

  // Etichetta "Utente:"
  lv_obj_t* user_title_label = lv_label_create(content);
  lv_label_set_text(user_title_label, "Utente:");
  lv_obj_set_style_text_color(user_title_label, lv_color_hex(0xAAAAAA), 0);  // Grigio chiaro

  // Etichetta con il nome utente effettivo
  lv_obj_t* user_value_label = lv_label_create(content);
  lv_label_set_text(user_value_label, cred.username);
  lv_obj_set_style_text_font(user_value_label, &montserrat_18_extended, 0);  // Usa il font esteso

  // Etichetta "Password:"
  lv_obj_t* pass_title_label = lv_label_create(content);
  lv_label_set_text(pass_title_label, "Password:");
  lv_obj_set_style_text_color(pass_title_label, lv_color_hex(0xAAAAAA), 0);

  // Etichetta con la password effettiva
  lv_obj_t* pass_value_label = lv_label_create(content);
  lv_label_set_text(pass_value_label, password.c_str());
  lv_obj_set_style_text_font(pass_value_label, &montserrat_18_extended, 0);
}

void create_wipe_settings_screen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

  // Titolo
  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text(title, "Auto-distruzione");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

  // Etichetta descrittiva
  lv_obj_t* info_label = lv_label_create(scr);
  lv_label_set_text(info_label, "Numero di tentativi PIN errati\nprima di cancellare le credenziali:");
  lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(info_label, lv_pct(90));
  lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 60);

  // Slider per selezionare il numero di tentativi
  lv_obj_t* slider = lv_slider_create(scr);
  lv_obj_set_width(slider, lv_pct(70));
  lv_obj_center(slider);
  lv_slider_set_range(slider, 3, 10);  // Imposta il range da 3 a 10
  lv_slider_set_value(slider, settingsManager.getMaxPinAttempts(), LV_ANIM_OFF);

  // Etichetta per mostrare il valore corrente dello slider
  lv_obj_t* slider_label = lv_label_create(scr);
  lv_label_set_text_fmt(slider_label, "%d", settingsManager.getMaxPinAttempts());
  lv_obj_align_to(slider_label, slider, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

  // Evento per lo slider
  lv_obj_add_event_cb(
    slider, [](lv_event_t* e) {
      lv_obj_t* slider = lv_event_get_target(e);
      int32_t value = lv_slider_get_value(slider);

      // Aggiorna l'etichetta e salva l'impostazione
      lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
      lv_label_set_text_fmt(label, "%d", value);
      settingsManager.setMaxPinAttempts(value);
    },
    LV_EVENT_VALUE_CHANGED, slider_label);

  // Pulsante Indietro
  lv_obj_t* back_btn = lv_btn_create(scr);
  lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
  lv_obj_add_event_cb(
    back_btn, [](lv_event_t* e) {
      create_settings_screen();
    },
    LV_EVENT_CLICKED, NULL);
  lv_obj_t* back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Indietro");
}

void handle_inactivity() {
  const uint32_t LOCK_SCREEN_TIMEOUT_MS = 10000;
  const uint32_t UNLOCKED_TIMEOUT_MS = 15000;

  if (is_display_off) {
    return;
  }

  uint32_t inactive_time_ms = lv_disp_get_inactive_time(NULL);

  if (securityManager.getState() == SecurityState::LOCKED) {
    if (inactive_time_ms > LOCK_SCREEN_TIMEOUT_MS) {
      USBSerial.println("Inattività su schermo bloccato: spegnimento display via IO Expander.");
      // Usiamo il comando originale, ora reso più stabile dalla nuova velocità I2C
      //expander->digitalWrite(7, LOW);
      gfx->Display_Brightness(0);

      is_display_off = true;
    }
  }
  // CASO 2: Schermo SBLOCCATO
  else if (securityManager.getState() == SecurityState::UNLOCKED) {
    // Controlla se lo screensaver è GIA' attivo. Se sì, non fare nulla.
    if (is_screensaver_active) {
      return;
    }

    if (inactive_time_ms > UNLOCKED_TIMEOUT_MS) {
      USBSerial.println("Inattività su schermo sbloccato: avvio screensaver.");
      create_screensaver_screen();  // Avvia lo screensaver
    }
  }
}

// Sostituisci la tua vecchia create_screensaver_screen con questa versione

void create_screensaver_screen() {
    is_screensaver_active = true;

    lv_obj_clean(lv_scr_act());
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // --- NUOVA LOGICA: CREA UN OGGETTO IMMAGINE ---
    lv_obj_t* screensaver_img = lv_img_create(scr);
    lv_img_set_src(screensaver_img, &food_potatoes_junk_food_fries_icon_261877); // Imposta la tua icona come sorgente
    lv_obj_center(screensaver_img); // Centra l'icona all'inizio
    // ---------------------------------------------

    // Crea un timer che si occuperà solo di spostare l'immagine
    lv_timer_t* movement_timer = lv_timer_create([](lv_timer_t* timer) {
        // Recupera l'oggetto immagine dai dati utente del timer
        lv_obj_t* img_to_move = (lv_obj_t*)timer->user_data;

        // Sposta l'immagine in una posizione casuale per evitare burn-in
        int16_t x_pos = random(0, LCD_WIDTH - lv_obj_get_width(img_to_move));
        int16_t y_pos = random(0, LCD_HEIGHT - lv_obj_get_height(img_to_move));
        lv_obj_set_pos(img_to_move, x_pos, y_pos);

    }, 5000, screensaver_img); // Si attiva ogni 5 secondi, passando l'oggetto immagine

    // Aggiungi un evento per uscire dallo screensaver al tocco
    lv_obj_add_event_cb(scr, [](lv_event_t * e) {
        lv_timer_t* timer_to_delete = (lv_timer_t*)lv_event_get_user_data(e);
        if (timer_to_delete) {
            lv_timer_del(timer_to_delete);
        }
        
        is_screensaver_active = false; 

        // Crea un timer one-shot per cambiare schermata in sicurezza
        lv_timer_create([](lv_timer_t* t){
            create_main_screen();
        }, 10, NULL)->repeat_count = 1;

    }, LV_EVENT_CLICKED, movement_timer); // Passiamo il timer di movimento per poterlo cancellare
}