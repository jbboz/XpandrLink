/*
  TimbreTypes.h
  Shared plain-data types for the Timbre Space feature (2-D patch-blending map).
  Kept dependency-light so both TimbreSpaceEngine and PatchBlender can include it
  without a circular include.
*/
#pragma once
#include <map>
#include <array>
#include <cstdint>
#include <JuceHeader.h>

// One library patch, already decoded to the {paramID -> value} form + mod region.
struct TimbrePatch
{
    int                    libraryId = -1;   // stable id (index into the source list is fine)
    juce::String           name;             // 8-char patch name
    std::map<int,int>      params;           // PatchCodec::decode output
    std::array<uint8_t,60> modBytes{};       // decoded mod-matrix bytes (raw[128..187])
};

// A patch's placement in the space: position (x,y) and colour (r,g,b), all [0,1].
struct TimbrePoint
{
    int   libraryId = -1;
    float x = 0.0f, y = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

// A contributing patch and its blend weight. For any query the returned set has
// weights >= 0 that sum to 1. `index` indexes the engine's parallel point/patch lists.
struct WeightedPatch
{
    int   index  = 0;
    float weight = 0.0f;
};
