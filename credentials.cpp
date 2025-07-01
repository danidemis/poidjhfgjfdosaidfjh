#include "credentials.h"
#include <SD_MMC.h>
#include <Arduino.h>

extern HWCDC USBSerial;

CredentialsManager::CredentialsManager() : m_credential_count(0) {}

void CredentialsManager::begin() {
    USBSerial.println("DEBUG CredMan: Esecuzione di begin()...");
    if (SD_MMC.exists(CREDENTIALS_FILE)) {
        File file = SD_MMC.open(CREDENTIALS_FILE, FILE_READ);
        if (file) {
            long file_size = file.size();
            USBSerial.printf("DEBUG CredMan: Trovato credentials.bin. Dimensione: %ld bytes.\n", file_size);
            if (file_size > 0 && file_size % CREDENTIAL_RECORD_SIZE == 0) {
                 m_credential_count = file_size / CREDENTIAL_RECORD_SIZE;
            } else {
                USBSerial.printf("ATTENZIONE CredMan: La dimensione del file (%ld) non e' un multiplo di %d. Imposto conteggio a 0.\n", file_size, CREDENTIAL_RECORD_SIZE);
                m_credential_count = 0;
            }
            file.close();
        } else {
            USBSerial.println("ERRORE CredMan: Impossibile aprire credentials.bin, anche se esiste. Imposto conteggio a 0.");
            m_credential_count = 0;
        }
    } else {
        USBSerial.println("DEBUG CredMan: Il file credentials.bin non esiste. Imposto conteggio a 0.");
        m_credential_count = 0;
    }
    USBSerial.printf("INFO CredMan: Conteggio credenziali impostato a %d.\n", m_credential_count);
}


bool CredentialsManager::importFromSD(const char* filepath, Crypto& crypto) {
    // --- 1. Carica le credenziali esistenti in memoria ---
    begin(); // Assicurati che il conteggio sia aggiornato
    std::vector<Credential> existing_creds;
    if (m_credential_count > 0) {
        File existing_file = SD_MMC.open(CREDENTIALS_FILE, FILE_READ);
        if (existing_file) {
            existing_creds.resize(m_credential_count);
            existing_file.read((uint8_t*)existing_creds.data(), m_credential_count * sizeof(Credential));
            existing_file.close();
            USBSerial.printf("DEBUG Import: Caricate %d credenziali esistenti in memoria per il controllo.\n", m_credential_count);
        }
    }

    // --- 2. Apri il file CSV da importare ---
    File csvFile = SD_MMC.open(filepath);
    if (!csvFile) {
        USBSerial.printf("ERRORE CredMan: Impossibile aprire il file CSV '%s'\n", filepath);
        return false;
    }
    
    // Apri il file binario in modalità APPEND.
    File binFile = SD_MMC.open(CREDENTIALS_FILE, FILE_APPEND);
    if (!binFile) {
        csvFile.close();
        USBSerial.println("ERRORE CredMan: Impossibile aprire il file binario in modalità append!");
        return false;
    }

    // Salta l'intestazione
    if (csvFile.available()) {
        csvFile.readStringUntil('\n');
    }

    int record_count = 0;
    int skipped_count = 0;
    while (csvFile.available()) {
        String line = csvFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int first_comma = line.indexOf(',');
        int second_comma = line.indexOf(',', first_comma + 1);

        if (first_comma > 0 && second_comma > first_comma) {
            String title = line.substring(0, first_comma);
            String username = line.substring(first_comma + 1, second_comma);
            
            // --- 3. CONTROLLO DUPLICATI ---
            bool is_duplicate = false;
            for (const auto& existing : existing_creds) {
                // Un duplicato è definito da titolo E utente uguali.
                if (title == existing.title && username == existing.username) {
                    is_duplicate = true;
                    break;
                }
            }

            if (is_duplicate) {
                skipped_count++;
                USBSerial.printf("  - DUPLICATO: La credenziale '%s' con utente '%s' esiste gia'. Saltata.\n", title.c_str(), username.c_str());
                continue; // Salta al prossimo ciclo del while
            }

            // --- 4. Aggiungi solo se non è un duplicato ---
            String plain_pass = line.substring(second_comma + 1);
            String encrypted_pass = crypto.encrypt(plain_pass);

            if (encrypted_pass.length() > 0 && encrypted_pass.length() < MAX_ENCRYPTED_PASS_LEN) {
                Credential cred = {0};
                strncpy(cred.title, title.c_str(), MAX_TITLE_LEN - 1);
                strncpy(cred.username, username.c_str(), MAX_USERNAME_LEN - 1);
                strncpy(cred.encrypted_password, encrypted_pass.c_str(), MAX_ENCRYPTED_PASS_LEN - 1);
                binFile.write((uint8_t*)&cred, sizeof(Credential));
                record_count++;
            } else {
                USBSerial.printf("  - ATTENZIONE: Password vuota per '%s'. Credenziale saltata.\n", title.c_str());
            }
        }
    }
    csvFile.close();
    binFile.close();
    
    USBSerial.printf("DEBUG Import: Importazione terminata. Aggiunti %d nuovi record. Saltati %d duplicati.\n", record_count, skipped_count);

    // Ricalcola il conteggio finale
    begin();
    return true;
}
size_t CredentialsManager::getCount() const { return m_credential_count; }

bool CredentialsManager::getCredential(size_t index, Credential* cred) const {
    if (index >= m_credential_count || !cred) return false;
    File file = SD_MMC.open(CREDENTIALS_FILE);
    if (!file) return false;
    file.seek(index * CREDENTIAL_RECORD_SIZE);
    bool success = file.read((uint8_t*)cred, sizeof(Credential)) == sizeof(Credential);
    file.close();
    return success;
}

void CredentialsManager::clear() {
    if (SD_MMC.exists(CREDENTIALS_FILE)) SD_MMC.remove(CREDENTIALS_FILE);
    m_credential_count = 0;
}