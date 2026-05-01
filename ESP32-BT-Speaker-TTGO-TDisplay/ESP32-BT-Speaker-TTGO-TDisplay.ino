/*
  This version of ESP32_BT_Speaker is based on a code by VolosR and Phil Schatzmann.
  Certain parts of the code were partially rewritten by Claude (AI).

  Code is licensed under GNU GPL 3.0 - For a full text, please refer to www.gnu.org/licenses.

  Please install ESP32 > v3.0.0, as this code relies on the new version of the AudioTools library.
  Before uploading, please set the partition scheme to "Huge APP"
*/
#include "AudioTools.h"
#include "AudioTools/Concurrency/RTOS.h"
#include "AudioTools/Communication/A2DPStream.h"
#include "riffsound2.h"
#include "DisplayImg.h"
#include <TFT_eSPI.h> 

TFT_eSPI tft = TFT_eSPI(); 
TFT_eSprite spr = TFT_eSprite(&tft);

// --- Audio config ------------------------------------------------------------
AudioInfo info(44100, 2, 16);
I2SStream kit;
BluetoothA2DPSink a2dp_sink;

// --- Single shared buffer: BT audio and prompts both feed into this ---------
// When a prompt is active, BT data is discarded and prompt fills the buffer.
// When idle, BT data fills it normally.
BufferRTOS<uint8_t> sharedBuffer(1024 * 20);
QueueStream<uint8_t> sharedQueue(sharedBuffer);

// --- StreamCopy drives the loop: queue → kit (I2S) --------------------------
StreamCopy copier(kit, sharedQueue, 512);

// --- Prompt queue ------------------------------------------------------------
#define MAX_PROMPT_QUEUE 5

struct PromptSound {
    const unsigned char* data;
    unsigned int length;
};

PromptSound promptQueue[MAX_PROMPT_QUEUE];
volatile int pqHead = 0, pqTail = 0, pqCount = 0;
volatile bool promptActive = false;

int curVolume = 63;
String iConn="Not Connected";
String song="";
String playing="Idle";

const int pwmFreq = 5000;
const int pwmResolution = 8;
const int pwmLedChannelTFT = 0;

Task copyTask("copyTask", 4000, 10, 1);

// --- Helpers -----------------------------------------------------------------
void enqueuePrompt(const unsigned char* data, unsigned int length) {
    if (pqCount >= MAX_PROMPT_QUEUE) {
        ESP_LOGW("Prompt", "Queue full, dropping sound");
        return;
    }
    promptQueue[pqTail] = { data, length };
    pqTail = (pqTail + 1) % MAX_PROMPT_QUEUE;
    pqCount++;
    ESP_LOGI("Prompt", "Enqueued, count=%d", pqCount);
}

// Called from loop() — drains one prompt into the shared buffer
void servicePromptQueue() {
    if (pqCount == 0 || promptActive) return;

    promptActive = true;
    PromptSound p = promptQueue[pqHead];
    pqHead = (pqHead + 1) % MAX_PROMPT_QUEUE;
    pqCount--;

    ESP_LOGI("Prompt", "Writing prompt to buffer, %u bytes", p.length);

    // Write prompt data directly into the shared buffer in chunks
    const size_t CHUNK = 512;
    size_t pos = 0;
    while (pos < p.length) {
        size_t chunk = min(CHUNK, p.length - pos);
        // Block until there is space — avoids dropping prompt data
        while (sharedBuffer.writeArray(p.data + pos, chunk) == 0) {
            delay(1); // yield, let copier drain the buffer
        }
        pos += chunk;
    }

    promptActive = false;
    ESP_LOGI("Prompt", "Prompt write done");
}

void espDelay(int ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}
// --- A2DP data callback -------------------------------------------------------
// Only writes BT data when no prompt is active; otherwise silently discards it
// so the prompt audio is not interleaved with music mid-prompt.
void readDataStream(const uint8_t* data, uint32_t length) {
    // if (promptActive || pqCount > 0) return; // prompt has priority
    if (promptActive) return;
    sharedBuffer.writeArray(data, length);
}

// --- A2DP connection callback -------------------------------------------------
void connectionStatusChanged(esp_a2d_connection_state_t state, void*) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            ESP_LOGI("BT", "Connected");
            iConn="Connected";
            enqueuePrompt(connected, connected_len);
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            ESP_LOGI("BT", "Disconnected");
            iConn="Disconnected";
            enqueuePrompt(disconnected, disconnected_len);
            break;
        default:
            break;
    }
}

void draw()
{
    tft.pushImage(0, 0, 240, 135, mainmenu);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(iConn, 92, 43, 2);
    tft.drawString(playing, 92, 84);
    tft.drawString(song.substring(0, 30), 47, 118, 2);
}

void metadata_callback(uint8_t id, const uint8_t *text) {
  Serial.print("ID: ");
  Serial.print(id);
  Serial.print(" -> ");
  Serial.println((char*)text);
  if(id==1){
        song=(char*)text; 
        draw();
    }
}

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    Serial.println("Playing");
    playing="Playing";
  } 
  else{
    Serial.println("Stopped");
    playing="Idle";
  }
  draw();
}

// --- Setup --------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(3);
    tft.setSwapBytes(true);
    ledcAttach(TFT_BL, pwmFreq, pwmResolution);
    ledcWrite(TFT_BL, 70);
    tft.pushImage(0,0,240,135,bootimg);
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

    sharedQueue.begin();

    auto cfg = kit.defaultConfig();
    cfg.copyFrom(info);
    cfg.pin_bck  = 33;
    cfg.pin_ws   = 32;
    cfg.pin_data = 25;
    cfg.pin_data_rx = -1;
    kit.begin(cfg);

    a2dp_sink.set_on_connection_state_changed(connectionStatusChanged);
    a2dp_sink.set_avrc_metadata_callback(metadata_callback);
    a2dp_sink.set_on_audio_state_changed(audio_state_changed);
    a2dp_sink.set_auto_reconnect(true, 4); // Only try 4 times instead of 2000
    a2dp_sink.set_stream_reader(readDataStream, false);
    a2dp_sink.start("ESP32 BT-SPK");
    a2dp_sink.set_volume(curVolume);

    copyTask.begin([]() {
        copier.copy(); 
    });

    enqueuePrompt(poweron, poweron_len);
    espDelay(2000);
    ESP_LOGI("System", "Ready");
}

// --- Loop ---------------------------------------------------------------------
void loop() {
    servicePromptQueue(); // writes next prompt into buffer if one is pending
    copier.copy();
}