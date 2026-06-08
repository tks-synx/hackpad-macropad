#include QMK_KEYBOARD_H
#include <stdio.h> // Required for sprintf formatting

/* ==========================================================================
   MACRO PAD MAC SIDE INSTALLATION & AUTOMATION BLUEPRINT
   
   PREREQUISITES:
   1. Open Terminal on your Mac and install the audio CLI tool via Homebrew:
      brew install switchaudio-source

   2. Open your favorite Mac automation utility (e.g., BetterTouchTool or 
      Keyboard Maestro) and build 5 separate triggers.

   ==========================================================================
   TRIGGER DETAILS & BASH SCRIPTS:
   ==========================================================================

   --- TRIGGER 1: KEY 10 (F13) - AUDIO DEVICE SWITCHER ---
   Type: Execute Shell Script
   Script:
      #!/bin/bash
      SwitchAudioSource -n
      NEW_DEV=$(SwitchAudioSource -c)
      XIAO_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
      if [ ! -z "$XIAO_PORT" ]; then
          echo -ne "DEV:$NEW_DEV\r" > "$XIAO_PORT"
      fi

   --- TRIGGER 2: ENCODER CLOCKWISE (F14) - VOLUME UP ---
   Type: Execute Shell Script
   Script:
      #!/bin/bash
      osascript -e "set volume output volume ((output volume of (get audio status)) + 5)"
      NEW_VOL=$(osascript -e "output volume of (get audio status)")
      XIAO_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
      if [ ! -z "$XIAO_PORT" ]; then
          echo -ne "VOL:$NEW_VOL%\r" > "$XIAO_PORT"
      fi

   --- TRIGGER 3: ENCODER COUNTER-CLOCKWISE (F15) - VOLUME DOWN ---
   Type: Execute Shell Script
   Script:
      #!/bin/bash
      osascript -e "set volume output volume ((output volume of (get audio status)) - 5)"
      NEW_VOL=$(osascript -e "output volume of (get audio status)")
      XIAO_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
      if [ ! -z "$XIAO_PORT" ]; then
          echo -ne "VOL:$NEW_VOL%\r" > "$XIAO_PORT"
      fi

   --- TRIGGER 4: ENCODER CLOCKWISE (F16) - BRIGHTNESS UP ---
   Type: Execute Shell Script
   Script:
      #!/bin/bash
      osascript -e 'tell application "System Events" to key code 144'
      XIAO_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
      if [ ! -z "$XIAO_PORT" ]; then
          echo -ne "BRT:Up\r" > "$XIAO_PORT"
      fi

   --- TRIGGER 5: ENCODER COUNTER-CLOCKWISE (F17) - BRIGHTNESS DOWN ---
   Type: Execute Shell Script
   Script:
      #!/bin/bash
      osascript -e 'tell application "System Events" to key code 145'
      XIAO_PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
      if [ ! -z "$XIAO_PORT" ]; then
          echo -ne "BRT:Down\r" > "$XIAO_PORT"
      fi
   ========================================================================== */

enum custom_keycodes {
    MC_RECORD = SAFE_RANGE,
    MC_KNOB_MODE 
};

// Global buffers and state trackers
char audio_device_name[32] = "Default Audio";
char alert_msg[32]         = "";
uint32_t alert_timer       = 0;

// Hardware state trackers
bool is_recording          = false; 
uint32_t record_start_time = 0;     
char timer_string[16]      = "00:00";

// Tracks what the knob currently controls (True = Volume, False = Brightness)
bool is_volume_mode        = true; 

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        // Row 1: Key 1 (Screenshot), Key 2 (Record), Keys 3-4 (Blank), Key 5 (Mode Toggle!)
        SCMD(KC_3),       MC_RECORD,  KC_NO,      KC_NO,        MC_KNOB_MODE,
        
        // Row 2: Keys 6-8 (Blank), Key 9 (New Note), Key 10 (Audio Switch)
        KC_NO,            KC_NO,      KC_NO,      LALT(LCMD(KC_N)), KC_F13
    )
};

void trigger_oled_alert(const char *msg) {
    strncpy(alert_msg, msg, sizeof(alert_msg) - 1);
    alert_timer = timer_read32();
}

void receive_ascii_from_host(const char *string) {
    if (strncmp(string, "DEV:", 4) == 0) {
        strncpy(audio_device_name, string + 4, sizeof(audio_device_name) - 1);
        trigger_oled_alert("Audio Switched!");
    } 
    else if (strncmp(string, "VOL:", 4) == 0 || strncmp(string, "BRT:", 4) == 0) {
        trigger_oled_alert(string); 
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case SCMD(KC_3):
                trigger_oled_alert("Screenshot Taken");
                break;
                
            case MC_RECORD:
                if (!is_recording) {
                    tap_code16(SCMD(KC_5)); 
                    wait_ms(200); 
                    tap_code(KC_ENT); 
                    
                    is_recording = true; 
                    record_start_time = timer_read32(); 
                    trigger_oled_alert("Recording Started");
                } else {
                    tap_code16(LCTL(LCMD(KC_ESC))); 
                    is_recording = false; 
                    trigger_oled_alert("Recording Saved!");
                }
                return false; 
                
            case MC_KNOB_MODE:
                is_volume_mode = !is_volume_mode; 
                if (is_volume_mode) {
                    trigger_oled_alert("Mode: Volume");
                } else {
                    trigger_oled_alert("Mode: Brightness");
                }
                break;

            case LALT(LCMD(KC_N)):
                trigger_oled_alert("New Apple Note");
                break;
                
            case KC_F13:
                trigger_oled_alert("Switching Audio...");
                break;
        }
    }
    return true;
}

#ifdef ENCODER_ENABLE
bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) {
        if (is_volume_mode) {
            if (clockwise) { tap_code(KC_F14); } 
            else           { tap_code(KC_F15); }
        } else {
            if (clockwise) { tap_code(KC_F16); } 
            else           { tap_code(KC_F17); }
        }
    }
    return true;
}
#endif

#ifdef OLED_ENABLE
bool oled_task_user(void) {
    oled_write_ln("Hackpad Keycab", false);
    
    // PRIORITY 1: Temporary Alerts / Real-time Volume/Brightness Pops
    if (alert_msg[0] != '\0' && timer_elapsed32(alert_timer) < 3000) {
        if (strncmp(alert_msg, "VOL:", 4) == 0) {
            oled_write("Volume: ", false);
            oled_write_ln(alert_msg + 4, false); 
        } 
        else if (strncmp(alert_msg, "BRT:", 4) == 0) {
            oled_write("Brightness: ", false);
            oled_write_ln(alert_msg + 4, false); 
        } 
        else {
            oled_write("Alert: ", false);
            oled_write_ln(alert_msg, false);
        }
    } 
    // PRIORITY 2: Active Recording Ticker
    else if (is_recording) {
        uint32_t total_seconds = timer_elapsed32(record_start_time) / 1000;
        uint32_t minutes = total_seconds / 60;
        uint32_t seconds = total_seconds % 60;
        sprintf(timer_string, "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
        
        oled_write("Recording: ", false);
        oled_write_ln(timer_string, false);
    } 
    // PRIORITY 3: Default Baseline Display
    else {
        oled_write("Audio: ", false);
        oled_write_ln(audio_device_name, false);
        oled_write_ln("", false); 
        
        if (is_volume_mode) {
            oled_write_ln("[Knob: Volume]", false);
        } else {
            oled_write_ln("[Knob: Brightness]", false);
        }
    }
    return false;
}
#endif