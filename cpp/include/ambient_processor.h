#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "rgb.h"

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