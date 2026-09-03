#include "hal/ScoreAnnouncer.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Wire.h>
#include <driver/i2s.h>

#include <cstring>

#include "config/BoardPins.h"

namespace bowls {

namespace {

constexpr uint8_t kEs8311Address = 0x18;
constexpr uint32_t kSampleRate = 8000;

bool writeCodecRegister(TwoWire& wire, uint8_t reg, uint8_t value) {
    wire.beginTransmission(kEs8311Address);
    wire.write(reg);
    wire.write(value);
    return wire.endTransmission() == 0;
}

bool configureCodec(TwoWire& wire) {
    static constexpr uint8_t kRegisters[][2] = {
        {0x00, 0x1f}, {0x00, 0x00}, {0x00, 0x80}, {0x01, 0x3f},
        {0x02, 0x08}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
        {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xff}, {0x09, 0x0c},
        {0x0a, 0x0c}, {0x0d, 0x01}, {0x0e, 0x02}, {0x12, 0x00},
        {0x13, 0x10}, {0x1c, 0x6a}, {0x31, 0x00}, {0x32, 0xff},
        {0x37, 0x08},
    };

    for (const auto& registerValue : kRegisters) {
        if (!writeCodecRegister(wire, registerValue[0], registerValue[1])) return false;
    }
    return true;
}

uint16_t readLittleEndian16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLittleEndian32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

int16_t decodeMuLaw(uint8_t encodedSample) {
    const uint8_t value = static_cast<uint8_t>(~encodedSample);
    const int magnitude = ((value & 0x0f) << 3) + 0x84;
    const int sample = (magnitude << ((value & 0x70) >> 4)) - 0x84;
    return value & 0x80 ? static_cast<int16_t>(-sample) : static_cast<int16_t>(sample);
}

}  // namespace

bool ScoreAnnouncer::begin(TwoWire& wire, int sdaPin, int sclPin) {
    (void)sdaPin;
    (void)sclPin;

    pinMode(AUDIO_PA_ENABLE, OUTPUT);
    digitalWrite(AUDIO_PA_ENABLE, HIGH);

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = kSampleRate;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = true;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = kSampleRate * 256;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCK_IO;
    pins.ws_io_num = I2S_WS_IO;
    pins.data_out_num = I2S_DO_IO;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    pins.mck_io_num = I2S_MCK_IO;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    if (!configureCodec(wire)) {
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    wire_ = &wire;
    initialized_ = true;
    return true;
}

void ScoreAnnouncer::setVolumePercent(uint8_t volumePercent) {
    if (!initialized_ || wire_ == nullptr) return;
    const uint8_t clampedVolume = volumePercent > 100 ? 100 : volumePercent;
    const uint8_t codecVolume = clampedVolume == 0 ? 0 : (clampedVolume * 256 / 100) - 1;
    writeCodecRegister(*wire_, 0x32, codecVolume);
}

void ScoreAnnouncer::playScore(int score) {
    if (score < 0) return;
    if (score > 21) score = 21;

    char path[16];
    std::snprintf(path, sizeof(path), "/audio/%d.wav", score);
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("Audio warning: unable to open %s\n", path);
        return;
    }

    uint8_t header[12];
    if (file.read(header, sizeof(header)) != sizeof(header) ||
        std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        Serial.printf("Audio warning: %s is not a WAV file\n", path);
        file.close();
        return;
    }

    bool validFormat = false;
    uint32_t dataSize = 0;
    while (file.available() && dataSize == 0) {
        uint8_t chunkHeader[8];
        if (file.read(chunkHeader, sizeof(chunkHeader)) != sizeof(chunkHeader)) break;
        const uint32_t chunkSize = readLittleEndian32(chunkHeader + 4);
        if (std::memcmp(chunkHeader, "fmt ", 4) == 0 && chunkSize >= 16) {
            uint8_t format[16];
            if (file.read(format, sizeof(format)) != sizeof(format)) break;
            validFormat = readLittleEndian16(format) == 7 && readLittleEndian16(format + 2) == 1 &&
                          readLittleEndian32(format + 4) == kSampleRate &&
                          readLittleEndian16(format + 14) == 8;
            file.seek(file.position() + chunkSize - sizeof(format));
        } else if (std::memcmp(chunkHeader, "data", 4) == 0) {
            dataSize = chunkSize;
        } else {
            file.seek(file.position() + chunkSize);
        }
        if (chunkSize & 1) file.seek(file.position() + 1);
    }

    if (!validFormat || dataSize == 0) {
        Serial.printf("Audio warning: %s must be 8 kHz mono mu-law WAV\n", path);
        file.close();
        return;
    }

    uint8_t encodedSamples[256];
    int16_t pcmSamples[512];
    uint32_t remaining = dataSize;
    while (remaining > 0) {
        const size_t requested = remaining < sizeof(encodedSamples) ? remaining : sizeof(encodedSamples);
        const size_t bytesRead = file.read(encodedSamples, requested);
        if (bytesRead == 0) break;
        for (size_t index = 0; index < bytesRead; ++index) {
            const int16_t sample = decodeMuLaw(encodedSamples[index]);
            pcmSamples[index * 2] = sample;
            pcmSamples[index * 2 + 1] = sample;
        }
        size_t bytesWritten = 0;
        i2s_write(I2S_NUM_0, pcmSamples, bytesRead * 2 * sizeof(int16_t), &bytesWritten,
                  portMAX_DELAY);
        remaining -= bytesRead;
    }
    file.close();
}

void ScoreAnnouncer::announceScore(int homeScore, int awayScore, bool deadEnd) {
    if (!initialized_) return;

    (void)deadEnd;
    playScore(homeScore);
    playScore(awayScore);
}

}  // namespace bowls