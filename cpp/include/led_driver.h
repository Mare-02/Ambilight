#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "rgb.h"

// ----------------------------------------------------------
// LED Controller – steuert den Strip über SPI
// ----------------------------------------------------------
class LEDController
{
public:
    LEDController(int numLeds, const std::string& spiDevice = "/dev/spidev0.0");
    ~LEDController();

    void setPixel(int index, const RGB& color);
    void setPixels(const std::vector<RGB>& colors);

    void applyGamma(float gamma);
    void send();                     // schreibt über SPI
    void clear();                    // alle LEDs aus

private:
    int _numLeds;
    int _spiFd;                      // SPI file descriptor
    std::vector<RGB> _leds;
    float _gamma = 2.2f;

    bool openSpi(const std::string& device);
    void closeSpi();
    uint8_t gammaCorrect(uint8_t v) const;
};
