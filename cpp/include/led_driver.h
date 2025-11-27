#pragma once

#include <vector>
#include <string>
#include <cstdint>

// ----------------------------------------------------------
// RGB struct
// ----------------------------------------------------------
struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    RGB() : r(0), g(0), b(0) {}
    RGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

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

// ----------------------------------------------------------
// Ambient Processor – berechnet die Farben aus dem Bild
// ----------------------------------------------------------
class AmbientProcessor
{
public:
    AmbientProcessor(int ledCount);

    std::vector<RGB> processFrame(const uint8_t* frameData, int width, int height);

    void setSmoothing(int frames);
    void setBrightness(float b);

private:
    int _ledCount;
    int _smoothing = 3;
    float _brightness = 1.0f;

    std::vector<std::vector<RGB>> _history;

    RGB averageSegment(const std::vector<RGB>& seg);
};