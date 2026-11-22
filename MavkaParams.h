// MAVKA - parameter table and factory presets (shared by the plugin and the offline tests)
#pragma once

#include <cstddef>
#include <cstring>

namespace mavka
{

enum class PType { Float, Choice, Bool };
enum class Unit  { Percent, Bipolar, Ink, Slant, Db, Hz, Ms, Sec, Tone, None };

struct ParamDef
{
    const char* id;
    const char* name;
    PType type;
    float minV, maxV, defV;
    float centre;              // value shown at the middle of a knob (0 = linear)
    Unit unit;
    const char* const* choices;
    int numChoices;
};

static const char* const kRateChoices[]     = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16" };
static const char* const kRatioChoices[]    = { "Unison 1:1", "Octaves 1:2:4", "Fifths 2:3", "Major 4:5:6",
                                                "Minor 10:12:15", "Sus 6:8:9", "Poly 3:4", "Golden" };
static const char* const kKeyChoices[]      = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char* const kChordChoices[]    = { "Octaves", "Power", "Major", "Minor", "Sus2", "Sus4", "Maj7",
                                                "Min7", "Min9", "Dom7", "Ukr Dorian", "Hijaz" };
static const char* const kRegisterChoices[] = { "Low", "Mid", "High" };
static const char* const kOsChoices[]       = { "Auto", "1x", "2x", "4x", "8x", "16x" };
static const char* const kEchoTimeChoices[] = { "Slap", "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
static const char* const kForestChoices[]   = { "Room", "Plate", "Hall", "Cathedral" };
static const char* const kTwinChoices[]     = { "Tight", "Wide", "Lush", "Choir", "Octave" };
static const char* const kDuckChoices[]     = { "Spectral", "Classic", "Off" };
static const char* const kCrushChoices[]    = { "Harmonic", "Digital", "Clock" };

enum ParamIndex
{
    // macros
    P_DRIVE, P_GRIT, P_MELODY, P_MOTION, P_INK, P_MIX, P_OUTPUT,
    // handwriting
    P_INKTIME, P_SLANT, P_LIGATURE,
    // impossible hardware
    P_LAMP, P_GLASS, P_FOLD, P_DOPPLER, P_BIAS, P_GHOSTDOWN, P_GHOSTUP,
    // voice
    P_CONSIST, P_FOCUS, P_SMOOTH, P_LOWCUT, P_HIGHCUT, P_PRETILT,
    // chord clock
    P_RATE, P_RATIO, P_WIDTH, P_VIBRATO, P_ENSEMBLE, P_BREATH, P_PULSE, P_ORBIT, P_WARP,
    // harp
    P_KEY, P_CHORD, P_REGISTER, P_DECAY, P_BRIGHT, P_HARPDRIVE,
    // output / system
    P_TONE, P_AUTOGAIN, P_OS,
    // aura (nonlinear send effects)
    P_ECHO, P_ECHOTIME, P_ECHOFB, P_FOREST, P_FORESTTYPE, P_FORESTDECAY, P_FORESTTUNE, P_FORESTBLOOM,
    P_TWIN, P_TWINTYPE, P_SPIN, P_FEED, P_DUCK, P_DUCKDEPTH,
    // crusher
    P_CRUSH, P_CRUSHMODE,
    P_COUNT
};

inline const ParamDef* paramDefs() noexcept
{
    static const ParamDef d[P_COUNT] =
    {
        { "drive",     "Drive",        PType::Float,  0.0f,   1.0f,     0.40f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "grit",      "Grit",         PType::Float,  0.0f,   1.0f,     0.15f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "melody",    "Melody",       PType::Float,  0.0f,   1.0f,     0.40f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "motion",    "Motion",       PType::Float,  0.0f,   1.0f,     0.45f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "ink",       "Ink",          PType::Float, -1.0f,   1.0f,     0.50f,   0.0f,   Unit::Ink,     nullptr, 0 },
        { "mix",       "Mix",          PType::Float,  0.0f,   1.0f,     0.45f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "output",    "Output",       PType::Float, -24.0f,  24.0f,    0.0f,    0.0f,   Unit::Db,      nullptr, 0 },

        { "inkTime",   "Ink Time",     PType::Float,  30.0f,  600.0f,   170.0f,  150.0f, Unit::Ms,      nullptr, 0 },
        { "slant",     "Slant",        PType::Float, -1.0f,   1.0f,     0.40f,   0.0f,   Unit::Slant,   nullptr, 0 },
        { "ligature",  "Ligature",     PType::Float,  0.0f,   1.0f,     0.30f,   0.0f,   Unit::Percent, nullptr, 0 },

        { "lamp",      "Lamp",         PType::Float,  0.0f,   1.0f,     1.00f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "glass",     "Glass",        PType::Float,  0.0f,   1.0f,     0.65f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "fold",      "Fold",         PType::Float,  0.0f,   1.0f,     0.30f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "doppler",   "Doppler",      PType::Float,  0.0f,   1.0f,     0.45f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "bias",      "Bias",         PType::Float, -1.0f,   1.0f,     0.30f,   0.0f,   Unit::Bipolar, nullptr, 0 },
        { "ghostDown", "Ghost Down",   PType::Float,  0.0f,   1.0f,     0.20f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "ghostUp",   "Ghost Up",     PType::Float,  0.0f,   1.0f,     0.15f,   0.0f,   Unit::Percent, nullptr, 0 },

        { "consist",   "Consistency",  PType::Float,  0.0f,   1.0f,     0.75f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "focus",     "Vowel Focus",  PType::Float,  0.0f,   1.0f,     0.50f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "smooth",    "Smooth",       PType::Float,  0.0f,   1.0f,     0.40f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "lowCut",    "Low Cut",      PType::Float,  20.0f,  800.0f,   90.0f,   120.0f, Unit::Hz,      nullptr, 0 },
        { "highCut",   "High Cut",     PType::Float,  2000.0f,20000.0f, 15000.0f,8000.0f,Unit::Hz,      nullptr, 0 },
        { "preTilt",   "Pre Tilt",     PType::Float, -12.0f,  12.0f,    0.0f,    0.0f,   Unit::Db,      nullptr, 0 },

        { "rate",      "Clock Rate",   PType::Choice, 0.0f,   6.0f,     2.0f,    0.0f,   Unit::None,    kRateChoices,  7 },
        { "ratio",     "Clock Chord",  PType::Choice, 0.0f,   7.0f,     3.0f,    0.0f,   Unit::None,    kRatioChoices, 8 },
        { "width",     "Width",        PType::Float,  0.0f,   1.0f,     0.60f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "vibrato",   "Vibrato",      PType::Float,  0.0f,   1.0f,     0.30f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "ensemble",  "Ensemble",     PType::Float,  0.0f,   1.0f,     0.50f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "breath",    "Breath",       PType::Float,  0.0f,   1.0f,     0.40f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "pulse",     "Pulse",        PType::Float,  0.0f,   1.0f,     0.25f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "orbit",     "Orbit",        PType::Float,  0.0f,   1.0f,     0.35f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "warp",      "Warp",         PType::Float,  0.0f,   1.0f,     0.15f,   0.0f,   Unit::Percent, nullptr, 0 },

        { "key",       "Harp Key",     PType::Choice, 0.0f,   11.0f,    0.0f,    0.0f,   Unit::None,    kKeyChoices,   12 },
        { "chord",     "Harp Chord",   PType::Choice, 0.0f,   11.0f,    3.0f,    0.0f,   Unit::None,    kChordChoices, 12 },
        { "register",  "Harp Register",PType::Choice, 0.0f,   2.0f,     1.0f,    0.0f,   Unit::None,    kRegisterChoices, 3 },
        { "decay",     "Harp Decay",   PType::Float,  0.1f,   8.0f,     1.8f,    1.5f,   Unit::Sec,     nullptr, 0 },
        { "bright",    "Harp Bright",  PType::Float,  0.0f,   1.0f,     0.50f,   0.0f,   Unit::Percent, nullptr, 0 },
        { "harpDrive", "Harp Drive",   PType::Float,  0.0f,   1.0f,     0.35f,   0.0f,   Unit::Percent, nullptr, 0 },

        { "tone",      "Tone",         PType::Float, -1.0f,   1.0f,     0.0f,    0.0f,   Unit::Tone,    nullptr, 0 },
        { "autoGain",  "Auto Gain",    PType::Bool,   0.0f,   1.0f,     1.0f,    0.0f,   Unit::None,    nullptr, 0 },
        { "os",        "Oversampling", PType::Choice, 0.0f,   5.0f,     0.0f,    0.0f,   Unit::None,    kOsChoices,    6 },

        { "echo",        "Echo",          PType::Float,  0.0f, 1.0f,   0.0f,  0.0f, Unit::Percent, nullptr, 0 },
        { "echoTime",    "Echo Time",     PType::Choice, 0.0f, 6.0f,   4.0f,  0.0f, Unit::None,    kEchoTimeChoices, 7 },
        { "echoFb",      "Echo Feedback", PType::Float,  0.0f, 0.92f,  0.40f, 0.0f, Unit::Percent, nullptr, 0 },
        { "forest",      "Forest",        PType::Float,  0.0f, 1.0f,   0.15f, 0.0f, Unit::Percent, nullptr, 0 },
        { "forestType",  "Forest Type",   PType::Choice, 0.0f, 3.0f,   1.0f,  0.0f, Unit::None,    kForestChoices, 4 },
        { "forestDecay", "Forest Decay",  PType::Float,  0.3f, 12.0f,  2.4f,  2.0f, Unit::Sec,     nullptr, 0 },
        { "forestTune",  "Forest Tune",   PType::Float,  0.0f, 1.0f,   0.40f, 0.0f, Unit::Percent, nullptr, 0 },
        { "forestBloom", "Forest Bloom",  PType::Float,  0.0f, 1.0f,   0.50f, 0.0f, Unit::Percent, nullptr, 0 },
        { "twin",        "Twin",          PType::Float,  0.0f, 1.0f,   0.12f, 0.0f, Unit::Percent, nullptr, 0 },
        { "twinType",    "Twin Type",     PType::Choice, 0.0f, 4.0f,   1.0f,  0.0f, Unit::None,    kTwinChoices, 5 },
        { "spin",        "Spin",          PType::Float,  0.0f, 1.0f,   0.25f, 0.0f, Unit::Percent, nullptr, 0 },
        { "feed",        "Feed",          PType::Float,  0.0f, 1.0f,   0.50f, 0.0f, Unit::Percent, nullptr, 0 },
        { "duck",        "Duck Mode",     PType::Choice, 0.0f, 2.0f,   0.0f,  0.0f, Unit::None,    kDuckChoices, 3 },
        { "duckDepth",   "Duck Depth",    PType::Float,  0.0f, 1.0f,   0.60f, 0.0f, Unit::Percent, nullptr, 0 },
        { "crush",       "Crush",         PType::Float,  0.0f, 1.0f,   0.0f,  0.0f, Unit::Percent, nullptr, 0 },
        { "crushMode",   "Crush Mode",    PType::Choice, 0.0f, 2.0f,   0.0f,  0.0f, Unit::None,    kCrushChoices, 3 },
    };
    return d;
}

inline int findParam (const char* id) noexcept
{
    const ParamDef* d = paramDefs();
    for (int i = 0; i < P_COUNT; ++i)
        if (std::strcmp (d[i].id, id) == 0)
            return i;
    return -1;
}

inline void defaultValues (float* v) noexcept
{
    const ParamDef* d = paramDefs();
    for (int i = 0; i < P_COUNT; ++i)
        v[i] = d[i].defV;
}

//==============================================================================
// Factory presets: every preset starts from the defaults ("Mavka") and overrides values.
struct PresetValue { const char* id; float value; };
struct Preset      { const char* name; const PresetValue* values; int count; };

#define MAVKA_PRESET(arr) arr, (int) (sizeof (arr) / sizeof (arr[0]))

// signature / init
static const PresetValue kPresetMavka[] = { { "drive", 0.40f } };

// the cleanest end: sheen + handwriting, almost no dirt
static const PresetValue kPresetSilk[] = {
    { "twin", 0.22f }, { "twinType", 0.0f }, { "forest", 0.10f }, 
    { "slant", 0.25f }, { "ligature", 0.15f }, 
    { "drive", 0.06f }, { "grit", 0.0f }, { "melody", 0.25f }, { "motion", 0.35f }, { "ink", 0.35f },
    { "mix", 0.35f }, { "fold", 0.0f }, { "glass", 0.55f }, { "doppler", 0.35f }, { "ghostDown", 0.05f },
    { "ghostUp", 0.10f }, { "smooth", 0.65f }, { "warp", 0.0f }, { "pulse", 0.10f }, { "bright", 0.6f },
    { "decay", 1.4f }, { "harpDrive", 0.15f } };

// dark & warm
static const PresetValue kPresetOakLamp[] = {
    { "forest", 0.18f }, { "forestType", 0.0f }, { "forestTune", 0.30f }, 
    { "slant", 0.30f }, { "ligature", 0.25f }, 
    { "drive", 0.58f }, { "grit", 0.10f }, { "melody", 0.30f }, { "motion", 0.30f }, { "ink", 0.40f },
    { "bias", 0.60f }, { "glass", 0.30f }, { "fold", 0.10f }, { "doppler", 0.25f }, { "ghostDown", 0.40f },
    { "ghostUp", 0.05f }, { "tone", -0.45f }, { "highCut", 9000.0f }, { "smooth", 0.65f },
    { "register", 0.0f }, { "decay", 2.2f }, { "bright", 0.25f }, { "harpDrive", 0.45f }, { "breath", 0.55f } };

static const PresetValue kPresetSmokedVelvet[] = {
    { "forest", 0.30f }, { "forestType", 2.0f }, { "forestTune", 0.50f }, { "echo", 0.12f }, { "echoTime", 5.0f }, 
    { "slant", 0.45f }, { "ligature", 0.40f }, 
    { "drive", 0.45f }, { "grit", 0.05f }, { "melody", 0.45f }, { "motion", 0.40f }, { "ink", 0.65f },
    { "inkTime", 260.0f }, { "bias", 0.45f }, { "glass", 0.45f }, { "doppler", 0.30f }, { "ghostDown", 0.30f },
    { "tone", -0.30f }, { "chord", 7.0f }, { "register", 0.0f }, { "decay", 3.0f }, { "bright", 0.30f },
    { "ratio", 4.0f }, { "rate", 1.0f }, { "ensemble", 0.65f } };

// glassy & singing
static const PresetValue kPresetGlassHarp[] = {
    { "forest", 0.28f }, { "forestTune", 0.75f }, { "twin", 0.20f }, 
    { "slant", 0.50f }, { "ligature", 0.35f }, 
    { "drive", 0.40f }, { "melody", 0.62f }, { "glass", 1.0f }, { "lamp", 0.60f }, { "doppler", 0.72f },
    { "fold", 0.10f }, { "tone", 0.30f }, { "bright", 0.78f }, { "register", 2.0f }, { "decay", 2.6f },
    { "ghostUp", 0.35f }, { "ink", 0.55f }, { "ratio", 2.0f }, { "chord", 4.0f }, { "smooth", 0.5f } };

static const PresetValue kPresetCrystalSpit[] = {
    { "echo", 0.22f }, { "echoTime", 3.0f }, { "echoFb", 0.35f }, { "forest", 0.10f }, { "twin", 0.25f }, { "twinType", 0.0f }, 
    { "slant", -0.30f }, { "ligature", 0.20f }, 
    { "drive", 0.50f }, { "glass", 0.90f }, { "doppler", 0.58f }, { "ink", 0.75f }, { "inkTime", 120.0f },
    { "focus", 0.80f }, { "melody", 0.35f }, { "tone", 0.20f }, { "pulse", 0.35f }, { "rate", 3.0f },
    { "ratio", 6.0f }, { "ghostUp", 0.25f } };

// torn & dirty
static const PresetValue kPresetLikho[] = {
    { "crush", 0.20f }, 
    { "echo", 0.25f }, { "echoTime", 2.0f }, { "echoFb", 0.50f }, { "forest", 0.10f }, { "forestType", 0.0f }, { "twin", 0.0f }, 
    { "slant", -0.50f }, { "ligature", 0.35f }, 
    { "drive", 0.80f }, { "grit", 0.70f }, { "fold", 0.70f }, { "ghostDown", 0.50f }, { "ink", -0.50f },
    { "mix", 0.55f }, { "smooth", 0.35f }, { "tone", -0.10f }, { "motion", 0.35f }, { "melody", 0.25f },
    { "harpDrive", 0.70f }, { "chord", 1.0f }, { "bias", 0.5f } };

static const PresetValue kPresetTornTape[] = {
    { "crush", 0.35f }, { "crushMode", 2.0f }, 
    { "echo", 0.35f }, { "echoTime", 0.0f }, { "echoFb", 0.65f }, { "forest", 0.12f }, { "spin", 0.40f }, { "twin", 0.0f }, 
    { "slant", -0.80f }, { "ligature", 0.50f }, 
    { "drive", 0.92f }, { "grit", 0.92f }, { "fold", 0.85f }, { "bias", 0.75f }, { "doppler", 0.60f },
    { "ink", -0.80f }, { "inkTime", 110.0f }, { "pulse", 0.55f }, { "ratio", 6.0f }, { "rate", 4.0f },
    { "focus", 0.30f }, { "melody", 0.20f }, { "harpDrive", 0.85f }, { "mix", 0.5f }, { "ghostDown", 0.35f } };

static const PresetValue kPresetBurntPhone[] = {
    { "crush", 0.45f }, { "crushMode", 1.0f }, 
    { "echo", 0.30f }, { "echoTime", 0.0f }, { "echoFb", 0.45f }, { "forest", 0.0f }, { "twin", 0.0f }, 
    { "slant", 0.20f }, { "ligature", 0.40f }, 
    { "drive", 0.72f }, { "grit", 0.55f }, { "lowCut", 450.0f }, { "highCut", 3800.0f }, { "fold", 0.45f },
    { "ink", -0.35f }, { "melody", 0.15f }, { "motion", 0.25f }, { "tone", 0.15f }, { "width", 0.2f },
    { "mix", 0.6f }, { "glass", 0.4f } };

// ethereal & wide
static const PresetValue kPresetNebula[] = {
    { "forest", 0.50f }, { "forestType", 3.0f }, { "forestDecay", 6.0f }, { "forestTune", 0.70f }, { "forestBloom", 0.70f }, { "echo", 0.20f }, { "echoTime", 5.0f }, { "twin", 0.20f }, { "twinType", 2.0f }, { "spin", 0.50f }, 
    { "slant", 0.60f }, { "ligature", 0.65f }, 
    { "drive", 0.30f }, { "grit", 0.05f }, { "melody", 0.78f }, { "decay", 5.0f }, { "bright", 0.60f },
    { "register", 2.0f }, { "chord", 8.0f }, { "motion", 0.72f }, { "width", 1.0f }, { "ensemble", 0.85f },
    { "orbit", 0.75f }, { "glass", 0.80f }, { "doppler", 0.45f }, { "ink", 0.60f }, { "mix", 0.50f },
    { "rate", 1.0f }, { "ratio", 7.0f }, { "warp", 0.30f } };

static const PresetValue kPresetTrembita[] = {
    { "forest", 0.38f }, { "forestType", 2.0f }, { "forestTune", 0.85f }, { "forestDecay", 4.0f }, { "echo", 0.10f }, 
    { "slant", 0.50f }, { "ligature", 0.45f }, 
    { "drive", 0.35f }, { "melody", 0.85f }, { "chord", 10.0f }, { "decay", 4.0f }, { "harpDrive", 0.50f },
    { "motion", 0.50f }, { "ratio", 5.0f }, { "ink", 0.50f }, { "width", 0.85f }, { "bright", 0.45f },
    { "ghostDown", 0.25f } };

static const PresetValue kPresetGhostChoir[] = {
    { "twin", 0.55f }, { "twinType", 3.0f }, { "forest", 0.25f }, { "forestType", 2.0f }, { "forestTune", 0.50f }, 
    { "slant", 0.35f }, { "ligature", 0.30f }, 
    { "drive", 0.45f }, { "ghostDown", 0.70f }, { "ghostUp", 0.50f }, { "melody", 0.40f }, { "width", 0.90f },
    { "ensemble", 0.75f }, { "motion", 0.55f }, { "ink", 0.45f }, { "ratio", 1.0f }, { "pulse", 0.4f } };

static const PresetValue kPresetForestChoir[] = {
    { "twin", 0.50f }, { "twinType", 3.0f }, { "forest", 0.55f }, { "forestType", 3.0f }, { "forestTune", 0.90f },
    { "forestDecay", 7.0f }, { "forestBloom", 0.65f }, { "melody", 0.60f }, { "drive", 0.30f }, { "spin", 0.45f },
    { "chord", 8.0f }, { "slant", 0.45f }, { "ligature", 0.20f }, { "mix", 0.50f } };

static const PresetValue kPresetEchoSpirit[] = {
    { "crush", 0.15f }, 
    { "echo", 0.50f }, { "echoTime", 5.0f }, { "echoFb", 0.75f }, { "spin", 0.60f }, { "forest", 0.20f },
    { "drive", 0.50f }, { "grit", 0.30f }, { "ink", -0.30f }, { "slant", -0.35f }, { "ligature", 0.0f }, { "twin", 0.15f } };

static const PresetValue kPresetPixelSpirit[] = {
    { "crush", 0.62f }, { "crushMode", 0.0f }, { "drive", 0.35f }, { "grit", 0.10f }, { "melody", 0.50f },
    { "forest", 0.30f }, { "forestTune", 0.70f }, { "twin", 0.20f }, { "ink", 0.55f }, { "slant", 0.40f },
    { "chord", 7.0f }, { "mix", 0.55f } };

inline const Preset* factoryPresets (int& count) noexcept
{
    static const Preset list[] =
    {
        { "Mavka",          MAVKA_PRESET (kPresetMavka) },
        { "Silk Thread",    MAVKA_PRESET (kPresetSilk) },
        { "Oak Lamp",       MAVKA_PRESET (kPresetOakLamp) },
        { "Smoked Velvet",  MAVKA_PRESET (kPresetSmokedVelvet) },
        { "Glass Harp",     MAVKA_PRESET (kPresetGlassHarp) },
        { "Crystal Spit",   MAVKA_PRESET (kPresetCrystalSpit) },
        { "Likho",          MAVKA_PRESET (kPresetLikho) },
        { "Torn Tape",      MAVKA_PRESET (kPresetTornTape) },
        { "Burnt Phone",    MAVKA_PRESET (kPresetBurntPhone) },
        { "Nebula",         MAVKA_PRESET (kPresetNebula) },
        { "Trembita",       MAVKA_PRESET (kPresetTrembita) },
        { "Ghost Choir",    MAVKA_PRESET (kPresetGhostChoir) },
        { "Forest Choir",   MAVKA_PRESET (kPresetForestChoir) },
        { "Echo Spirit",    MAVKA_PRESET (kPresetEchoSpirit) },
        { "Pixel Spirit",   MAVKA_PRESET (kPresetPixelSpirit) },
    };
    count = (int) (sizeof (list) / sizeof (list[0]));
    return list;
}

inline void presetValues (int index, float* v) noexcept
{
    defaultValues (v);
    int count = 0;
    const Preset* list = factoryPresets (count);
    if (index < 0 || index >= count)
        return;
    for (int i = 0; i < list[index].count; ++i)
    {
        const int p = findParam (list[index].values[i].id);
        if (p >= 0)
            v[p] = list[index].values[i].value;
    }
}

#undef MAVKA_PRESET

} // namespace mavka
