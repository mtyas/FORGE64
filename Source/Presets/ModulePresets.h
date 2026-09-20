#pragma once
#include <JuceHeader.h>
#include "../Engine/PadDefs.h"
#include <vector>

namespace f64 {

struct ModulePreset
{
    const char* name;
    float tune;
    float fx1;
    float fx2;
    float fx3;
    float decay;
    float drive;
};

class ModulePresetManager
{
public:
    static std::vector<ModulePreset> presetsFor(int srcType)
    {
        switch (srcType)
        {
            case SRC_KICK:
                return {
                    { "808 Deep Sub",     0.0f,  0.35f, 0.20f, 0.85f, 1.40f, 0.15f },
                    { "Tight Punch",      2.0f,  0.75f, 0.65f, 0.30f, 0.45f, 0.25f },
                    { "Hard Distorted",  -1.0f,  0.80f, 0.85f, 0.70f, 0.90f, 0.80f },
                    { "Acoustic Thump",   1.5f,  0.50f, 0.40f, 0.20f, 0.60f, 0.05f },
                    { "Electro Click",    4.0f,  0.90f, 0.95f, 0.40f, 0.35f, 0.40f },
                    { "Trap Boom 808",   -2.0f,  0.40f, 0.25f, 0.95f, 2.20f, 0.30f }
                };

            case SRC_SNARE:
                return {
                    { "808 Classic",      0.0f,  0.60f, 0.50f, 0.40f, 0.35f, 0.10f },
                    { "Crisp Studio",     2.0f,  0.85f, 0.75f, 0.70f, 0.25f, 0.20f },
                    { "Fat Body Snare",  -2.0f,  0.45f, 0.35f, 0.30f, 0.65f, 0.30f },
                    { "Loose Wires",      1.0f,  0.95f, 0.40f, 0.20f, 0.80f, 0.15f },
                    { "Electric Clack",   5.0f,  0.30f, 0.90f, 0.90f, 0.18f, 0.50f },
                    { "Industrial Crack", 0.0f,  0.90f, 0.65f, 0.85f, 0.40f, 0.75f }
                };

            case SRC_HAT_CLOSED:
                return {
                    { "808 Tight",        0.0f,  0.45f, 0.55f, 0.50f, 0.04f, 0.05f },
                    { "Trap Sizzle",      3.0f,  0.85f, 0.40f, 0.75f, 0.06f, 0.20f },
                    { "Metallic Ring",   -2.0f,  0.25f, 0.90f, 0.60f, 0.08f, 0.15f },
                    { "Damped Choke",     1.0f,  0.30f, 0.30f, 0.30f, 0.02f, 0.00f },
                    { "Dark Studio Hat", -4.0f,  0.20f, 0.70f, 0.25f, 0.05f, 0.10f }
                };

            case SRC_HAT_OPEN:
                return {
                    { "808 Open Sizzle",  0.0f,  0.65f, 0.60f, 0.50f, 0.45f, 0.10f },
                    { "Long Shimmer",     2.0f,  0.80f, 0.75f, 0.80f, 0.90f, 0.15f },
                    { "Metallic Bright",  4.0f,  0.50f, 0.90f, 0.90f, 0.35f, 0.25f },
                    { "Lo-Fi Ring",      -3.0f,  0.35f, 0.85f, 0.35f, 0.60f, 0.40f }
                };

            case SRC_CLAP:
                return {
                    { "808 Classic",      0.0f,  0.50f, 0.50f, 0.50f, 0.35f, 0.10f },
                    { "Wide Stereo",      1.5f,  0.85f, 0.60f, 0.85f, 0.45f, 0.20f },
                    { "Tight Snap",       3.0f,  0.20f, 0.75f, 0.20f, 0.18f, 0.15f },
                    { "Room Clap",       -1.0f,  0.65f, 0.40f, 0.95f, 0.70f, 0.25f }
                };

            case SRC_TOM:
                return {
                    { "Floor Tom Low",   -7.0f,  0.40f, 0.35f, 0.20f, 0.80f, 0.15f },
                    { "Mid Rack Tom",     0.0f,  0.50f, 0.50f, 0.40f, 0.60f, 0.10f },
                    { "High Tom",         7.0f,  0.60f, 0.65f, 0.50f, 0.45f, 0.10f },
                    { "Electronic Bend",  2.0f,  0.95f, 0.80f, 0.10f, 0.50f, 0.35f },
                    { "Thunder Tom",     -5.0f,  0.70f, 0.55f, 0.30f, 1.20f, 0.40f }
                };

            case SRC_CRASH:
                return {
                    { "808 Long Wash",    0.0f,  0.60f, 0.50f, 0.60f, 1.80f, 0.10f },
                    { "Dark Shimmer",    -3.0f,  0.40f, 0.70f, 0.30f, 2.20f, 0.20f },
                    { "Fast Choke",       3.0f,  0.80f, 0.40f, 0.75f, 0.35f, 0.15f },
                    { "Bright Splash",    5.0f,  0.90f, 0.85f, 0.90f, 0.80f, 0.05f }
                };

            case SRC_RIDE:
                return {
                    { "Bell Ping",        0.0f,  0.85f, 0.65f, 0.40f, 1.20f, 0.05f },
                    { "Dry Studio",       2.0f,  0.65f, 0.40f, 0.25f, 0.70f, 0.05f },
                    { "Wash Edge",       -2.0f,  0.40f, 0.80f, 0.85f, 1.60f, 0.15f }
                };

            case SRC_RIM:
                return {
                    { "Acoustic Wood",    0.0f,  0.60f, 0.65f, 0.50f, 0.12f, 0.05f },
                    { "Electronic Snap",  3.0f,  0.90f, 0.40f, 0.75f, 0.08f, 0.20f },
                    { "Deep Shell",      -4.0f,  0.45f, 0.85f, 0.35f, 0.18f, 0.10f }
                };

            case SRC_BELL:
                return {
                    { "808 Cowbell",      0.0f,  0.50f, 0.50f, 0.50f, 0.45f, 0.10f },
                    { "Latin Agogo",      4.0f,  0.75f, 0.80f, 0.40f, 0.30f, 0.05f },
                    { "Industrial Ding", -2.0f,  0.90f, 0.65f, 0.85f, 0.80f, 0.50f }
                };

            case SRC_CONGA:
                return {
                    { "Low Conga",       -4.0f,  0.45f, 0.50f, 0.60f, 0.55f, 0.05f },
                    { "High Bongo",       5.0f,  0.75f, 0.65f, 0.40f, 0.30f, 0.05f },
                    { "Slap Accent",      1.0f,  0.95f, 0.80f, 0.30f, 0.22f, 0.20f }
                };

            case SRC_SAMPLE:
            default:
                return {
                    { "Default One-Shot", 0.0f,  0.01f, 0.00f, 0.00f, 1.00f, 0.00f },
                    { "Punchy Transient", 0.0f,  0.00f, 0.05f, 0.00f, 0.35f, 0.15f },
                    { "Slow Pad / Bow",   0.0f,  0.60f, 0.30f, 0.00f, 2.50f, 0.10f },
                    { "Sustained Loop",   0.0f,  0.05f, 0.50f, 0.00f, 4.00f, 0.05f }
                };
        }
    }
};

} // namespace f64
