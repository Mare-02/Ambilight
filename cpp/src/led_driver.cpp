// cpp/src/led_driver.cpp
#include "led_driver.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <iomanip>

// -----------------------------
// Konfiguration / Defaults
// -----------------------------
static constexpr uint32_t DEFAULT_SPI_SPEED_HZ = 8000000; // 8MHz (WS2801 safe)
static constexpr uint8_t DEFAULT_SPI_MODE = SPI_MODE_0;
static constexpr int DEFAULT_BITS_PER_WORD = 8;
static constexpr useconds_t LATCH_US = 500; // small pause to latch WS2801

// -----------------------------
// Hilfsfunktionen
// -----------------------------
static inline uint8_t clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<uint8_t>(v);
}

// -----------------------------
// LEDDriver Implementation
// -----------------------------
LEDDriver::LEDDriver(const std::string& spi_dev, int num_leds)
    : spiDev_(spi_dev),
      spiFd_(-1),
      numLeds_(std::max(1, num_leds)),
      buffer_(numLeds_ * 3, 0),
      lastBuffer_(numLeds_ * 3, 0),
      gamma_(2.2f),
      brightness_(1.0f),
      smoothingAlpha_(0.25f)
{
    // allocate float history for smoothing (better precision)
    lastFloatBuffer_.assign(numLeds_ * 3, 0.0f);

    openSPI();
    buildGammaLUT(gamma_);
    // ensure initial state clear
    clear();
    show();
}

LEDDriver::~LEDDriver() {
    // clear LEDs before exit
    clear();
    show();
    closeSPI();
}

// -----------------------------
// SPI open/close
// -----------------------------
void LEDDriver::openSPI() {
    spiFd_ = open(spiDev_.c_str(), O_RDWR);
    if (spiFd_ < 0) {
        perror(("open SPI " + spiDev_).c_str());
        throw std::runtime_error("Failed to open SPI device: " + spiDev_);
    }

    // mode
    if (ioctl(spiFd_, SPI_IOC_WR_MODE, &DEFAULT_SPI_MODE) < 0) {
        perror("SPI set mode");
    }

    // bits per word
    if (ioctl(spiFd_, SPI_IOC_WR_BITS_PER_WORD, &DEFAULT_BITS_PER_WORD) < 0) {
        perror("SPI set bits per word");
    }

    // max speed
    uint32_t speed = DEFAULT_SPI_SPEED_HZ;
    if (ioctl(spiFd_, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("SPI set max speed");
    }

    // Good to know
    std::cerr << "[LEDDriver] Opened SPI " << spiDev_ << " fd=" << spiFd_
              << " speed=" << speed << "Hz\n";
}

void LEDDriver::closeSPI() {
    if (spiFd_ >= 0) {
        close(spiFd_);
        spiFd_ = -1;
    }
}

// -----------------------------
// Gamma LUT
// -----------------------------
void LEDDriver::buildGammaLUT(float gamma) {
    gamma_ = gamma;
    gammaLUT_.resize(256);
    for (int i = 0; i < 256; ++i) {
        float normalized = i / 255.0f;
        float corrected = powf(normalized, gamma_);
        gammaLUT_[i] = corrected * 255.0f;
    }
}

// -----------------------------
// apply gamma + brightness to lastFloatBuffer_ => buffer_ (uint8_t)
// -----------------------------
void LEDDriver::applyGammaAndBrightness() {
    std::lock_guard<std::mutex> lock(mutex_);

    // convert lastFloatBuffer_ (0..255 floats) through gamma LUT and brightness
    for (size_t i = 0; i < buffer_.size(); ++i) {
        float val = lastFloatBuffer_[i];
        // clamp float to 0..255
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;

        // nearest index into LUT
        int idx = static_cast<int>(val + 0.5f);
        if (idx < 0) idx = 0;
        if (idx > 255) idx = 255;

        float gammaed = gammaLUT_[idx] * brightness_; // scaled 0..255 * brightness
        int out = static_cast<int>(gammaed + 0.5f);
        buffer_[i] = clamp255(out);
    }
}

// -----------------------------
// Smoothing (EMA): updates lastFloatBuffer_ using newbuf (raw RGB bytes)
// -----------------------------
void LEDDriver::doSmoothing(const std::vector<uint8_t>& newbuf) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (newbuf.size() != lastFloatBuffer_.size()) {
        std::cerr << "[LEDDriver] doSmoothing: size mismatch\n";
        return;
    }

    // EMA: last = last + alpha * (new - last)
    const float a = std::clamp(smoothingAlpha_, 0.0f, 1.0f);
    for (size_t i = 0; i < lastFloatBuffer_.size(); ++i) {
        float n = static_cast<float>(newbuf[i]);
        float &l = lastFloatBuffer_[i];
        l = l + a * (n - l);
    }
}

// -----------------------------
// High-level API
// -----------------------------
void LEDDriver::setAll(uint8_t r, uint8_t g, uint8_t b) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Byte order: assume strip is RGB order. Many strips are GRB; adapt if needed.
    for (int i = 0; i < numLeds_; ++i) {
        size_t off = i * 3;
        // Store as raw order (we choose R, G, B)
        lastFloatBuffer_[off + 0] = static_cast<float>(r);
        lastFloatBuffer_[off + 1] = static_cast<float>(g);
        lastFloatBuffer_[off + 2] = static_cast<float>(b);
    }
    // Apply gamma & brightness to produce buffer_
    applyGammaAndBrightness();
}

void LEDDriver::setPixel(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= numLeds_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    size_t off = idx * 3;
    lastFloatBuffer_[off + 0] = static_cast<float>(r);
    lastFloatBuffer_[off + 1] = static_cast<float>(g);
    lastFloatBuffer_[off + 2] = static_cast<float>(b);
    applyGammaAndBrightness();
}

void LEDDriver::show() {
    // ensure buffer_ is consistent with lastFloatBuffer_
    applyGammaAndBrightness();

    // write buffer to SPI
    if (spiFd_ < 0) {
        std::cerr << "[LEDDriver] show: SPI not opened\n";
        return;
    }

    ssize_t written = write(spiFd_, buffer_.data(), buffer_.size());
    if (written < 0) {
        perror("SPI write");
    } else if (static_cast<size_t>(written) != buffer_.size()) {
        std::cerr << "[LEDDriver] Warning: partial SPI write " << written << "/" << buffer_.size() << "\n";
    }

    // small pause for latch
    usleep(LATCH_US);
}

void LEDDriver::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < lastFloatBuffer_.size(); ++i) lastFloatBuffer_[i] = 0.0f;
    for (size_t i = 0; i < buffer_.size(); ++i) buffer_[i] = 0;
    // send immediately
    show();
}

// -----------------------------
// Config setters
// -----------------------------
void LEDDriver::setGamma(float gamma) {
    if (gamma <= 0.01f) return;
    std::lock_guard<std::mutex> lock(mutex_);
    buildGammaLUT(gamma);
    // rebuild output with new gamma
    applyGammaAndBrightness();
}

void LEDDriver::setBrightness(float brightness) {
    std::lock_guard<std::mutex> lock(mutex_);
    brightness_ = std::clamp(brightness, 0.0f, 1.0f);
    applyGammaAndBrightness();
}

void LEDDriver::setSmoothingAlpha(float alpha) {
    std::lock_guard<std::mutex> lock(mutex_);
    smoothingAlpha_ = std::clamp(alpha, 0.0f, 1.0f);
}

// -----------------------------
// Command parser & handler (simple ASCII commands)
// Supported commands:
//  COLOR r g b
//  PIX idx r g b
//  BRIGHT percent_or_0to1  (e.g., BRIGHT 80  or BRIGHT 0.8)
//  GAMMA value
//  SMOOTH alpha (0..1)
//  SHOW
//  CLEAR
//  STATUS
// -----------------------------
void LEDDriver::handleCommand(const std::string &cmd) {
    std::istringstream iss(cmd);
    std::string token;
    if (!(iss >> token)) return;

    if (token == "COLOR") {
        int r,g,b;
        if (iss >> r >> g >> b) {
            // update smoothing target and do single smoothing step
            std::vector<uint8_t> newbuf(numLeds_ * 3);
            for (int i = 0; i < numLeds_; ++i) {
                size_t off = i*3;
                newbuf[off+0] = clamp255(r);
                newbuf[off+1] = clamp255(g);
                newbuf[off+2] = clamp255(b);
            }
            doSmoothing(newbuf);
            applyGammaAndBrightness();
            show();
        }
    }
    else if (token == "PIX") {
        int idx, r, g, b;
        if (iss >> idx >> r >> g >> b) {
            setPixel(idx, clamp255(r), clamp255(g), clamp255(b));
            show();
        }
    }
    else if (token == "BRIGHT") {
        std::string val;
        if (iss >> val) {
            try {
                if (val.find('.') != std::string::npos) {
                    float b = std::stof(val);
                    setBrightness(b);
                } else {
                    int bi = std::stoi(val);
                    // allow percent 0..100
                    if (bi > 1) {
                        setBrightness(std::clamp(bi / 100.0f, 0.0f, 1.0f));
                    } else {
                        setBrightness(std::clamp(static_cast<float>(bi), 0.0f, 1.0f));
                    }
                }
                applyGammaAndBrightness();
                show();
            } catch (...) {}
        }
    }
    else if (token == "GAMMA") {
        float g;
        if (iss >> g) {
            setGamma(g);
            // immediate update
            applyGammaAndBrightness();
            show();
        }
    }
    else if (token == "SMOOTH") {
        float alpha;
        if (iss >> alpha) {
            setSmoothingAlpha(alpha);
        }
    }
    else if (token == "SHOW") {
        show();
    }
    else if (token == "CLEAR") {
        clear();
    }
    else if (token == "STATUS") {
        std::ostringstream oss;
        oss << "LEDs=" << numLeds_ << " brightness=" << brightness_
            << " gamma=" << gamma_ << " smooth=" << smoothingAlpha_;
        std::string s = oss.str();
        // if called from socket handler, we might want to return or print
        std::cout << "[LEDDriver STATUS] " << s << std::endl;
    }
    else {
        std::cerr << "[LEDDriver] Unknown command: " << token << " (full: " << cmd << ")\n";
    }
}
