#pragma once

#include <juce_core/juce_core.h>
#include "Parameters.h"
#include <vector>
#include <utility>

/**
    Factory presets. Each is a starting point that demonstrates a different
    facet of Master Control Panel, so loading through them teaches what the plugin does.
    Values are stored in real units and converted to normalised form when
    applied, so they stay readable and range-safe.
*/
namespace cfm::presets
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<const char*, float>> values;
    };

    inline const std::vector<Preset>& factory()
    {
        using namespace cfm::params::id;
        static const std::vector<Preset> presets =
        {
            { "00 - Init Clean", {
                { tubeOn, 0 }, { tapeOn, 0 }, { compOn, 0 }, { eqOn, 1 },
                { air, 0 }, { tight, 0 }, { width, 100 }, { monoFreq, 0 },
                { mix, 100 }, { oversample, 1 }, { drift, 15 }, { circuitBend, 0 } } },

            { "01 - Gentle Glue", {
                { compOn, 1 }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 30 },
                { compRelease, 300 }, { compMode, 0 }, { compOptBlend, 70 }, { transformer, 0 },
                { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 20 }, { tapeOn, 0 }, { air, 10 } } },

            { "02 - Analog Warmth", {
                { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 45 }, { tubeBias, 15 },
                { tapeOn, 1 }, { tapeSpeed, 1 }, { tapeDrive, 35 }, { air, 15 },
                { compOn, 1 }, { compThresh, -16 }, { compRatio, 1.8f }, { compOptBlend, 80 } } },

            { "03 - Loud & Proud", {
                { compOn, 1 }, { compThresh, -18 }, { compRatio, 4 }, { compAttack, 10 },
                { compRelease, 150 }, { compMode, 0 }, { compOptBlend, 30 },
                { tubeOn, 1 }, { tubeModel, 1 }, { tubeDrive, 55 },
                { tapeOn, 1 }, { tapeSpeed, 0 }, { tapeDrive, 45 },
                { headroom, -3 }, { air, 20 }, { tight, 20 } } },

            { "04 - Airy Master", {
                { air, 45 }, { bandGain[4], 3 }, { bandFreq[4], 12000 }, { bandOn[4], 1 },
                { width, 115 }, { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 25 },
                { compOn, 1 }, { compThresh, -14 }, { compRatio, 1.6f }, { tapeOn, 0 } } },

            { "05 - Punch Bus", {
                { compOn, 1 }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 },
                { compRelease, 120 }, { compOptBlend, 20 }, { compMode, 0 },
                { tubeOn, 1 }, { tubeModel, 1 }, { tubeDrive, 30 }, { transformer, 1 } } },

            { "06 - Tape Cohesion", {
                { tapeOn, 1 }, { tapeSpeed, 1 }, { tapeDrive, 55 }, { tapeLF, 1.5f }, { tapeHF, -1 },
                { tubeOn, 1 }, { tubeModel, 2 }, { tubeDrive, 20 },
                { compOn, 1 }, { compThresh, -15 }, { compRatio, 2 }, { compOptBlend, 85 } } },

            { "07 - Wide & Deep", {
                { width, 140 }, { monoFreq, 120 }, { air, 25 },
                { bandGain[0], 1.5f }, { bandFreq[0], 80 }, { bandOn[0], 1 },
                { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 22 }, { compOn, 0 } } },

            { "08 - Mars Crush", {
                { tubeOn, 1 }, { tubeModel, 2 }, { tubeDrive, 70 }, { tubeBias, 30 },
                { circuitBend, 30 }, { tapeOn, 1 }, { tapeSpeed, 0 }, { tapeDrive, 50 },
                { compOn, 1 }, { compThresh, -20 }, { compRatio, 6 }, { transformer, 2 },
                { width, 110 }, { drift, 60 } } },

            { "09 - Main Stage Voltage", {
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 30 }, { tight, 32 },
                { air, 45 }, { bandOn[3], 1 }, { bandFreq[3], 5000 }, { bandGain[3], 2.5f },
                { bandQ[3], 1 }, { compOn, 1 }, { compThresh, -18 }, { compRatio, 3 },
                { compAttack, 12 }, { compRelease, 120 }, { compKnee, 6 }, { compOptBlend, 65 },
                { compAutoMk, 1 }, { scHpf, 95 }, { transformer, 1 }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 26 }, { tubeBias, 12 }, { width, 122 },
                { monoFreq, 120 }, { autoGain, 1 }, { oversample, 1 } } },

            { "10 - Velvet Basement Dub", {
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 30 }, { tight, 25 },
                { bandOn[1], 1 }, { bandFreq[1], 300 }, { bandGain[1], -2 }, { bandQ[1], 1.2f },
                { compOn, 1 }, { compThresh, -20 }, { compRatio, 2.5f }, { compAttack, 30 },
                { compRelease, 250 }, { compKnee, 9 }, { compOptBlend, 100 }, { compAutoMk, 1 },
                { scHpf, 100 }, { transformer, 0 }, { tubeOn, 0 }, { tapeOn, 1 },
                { tapeDrive, 30 }, { tapeSpeed, 0 }, { tapeLF, 2 }, { tapeHF, -1 },
                { width, 110 }, { monoFreq, 100 }, { autoGain, 1 }, { oversample, 1 } } },

            { "11 - Concrete Warehouse", {
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 34 }, { tight, 40 },
                { lpOn, 1 }, { lpFreq, 16500 }, { bandOn[0], 1 }, { bandFreq[0], 80 },
                { bandGain[0], 2 }, { bandQ[0], 0.8f }, { compOn, 1 }, { compThresh, -16 },
                { compRatio, 4 }, { compAttack, 8 }, { compRelease, 90 }, { compKnee, 2 },
                { compOptBlend, 20 }, { compAutoMk, 1 }, { scHpf, 110 }, { transformer, 2 },
                { compMode, 2 }, { tubeOn, 1 }, { tubeModel, 1 }, { tubeDrive, 42 },
                { tubeTone, -12 }, { circuitBend, 18 }, { drift, 25 }, { width, 124 },
                { monoFreq, 130 }, { autoGain, 1 }, { oversample, 2 } } },

            { "12 - Neon Supersaw Rush", {
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 30 }, { tight, 20 },
                { air, 40 }, { bandOn[4], 1 }, { bandFreq[4], 12000 }, { bandGain[4], 3 },
                { bandQ[4], 0.7f }, { compOn, 1 }, { compThresh, -18 }, { compRatio, 3 },
                { compAttack, 20 }, { compRelease, 140 }, { compKnee, 5 }, { compOptBlend, 45 },
                { compAutoMk, 1 }, { scHpf, 95 }, { transformer, 0 }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 18 }, { tubeTone, 35 }, { tapeOn, 1 },
                { tapeDrive, 18 }, { tapeSpeed, 1 }, { tapeHF, 2 }, { width, 140 },
                { monoFreq, 110 }, { autoGain, 1 }, { oversample, 1 } } },

            { "13 - Midnight 808 Glue", {
                { inputTrim, 0 }, { headroom, 3 }, { oversample, 1 }, { autoGain, 1 },
                { mix, 100 }, { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 24 },
                { tight, 25 }, { air, 22 }, { bandOn[0], 1 }, { bandFreq[0], 55 },
                { bandGain[0], 3.5f }, { bandQ[0], 0.7f }, { compOn, 1 }, { compThresh, -18 },
                { compRatio, 2.5f }, { compAttack, 30 }, { compRelease, 180 }, { compKnee, 8 },
                { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 70 }, { scHpf, 90 },
                { transformer, 0 }, { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 18 },
                { tubeBias, 12 }, { tubeTone, 8 }, { monoFreq, 110 }, { width, 105 } } },

            { "14 - Concrete Trunk Rattle", {
                { inputTrim, 0 }, { headroom, 8 }, { oversample, 2 }, { autoGain, 1 },
                { mix, 100 }, { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 28 },
                { tight, 40 }, { bandOn[0], 1 }, { bandFreq[0], 60 }, { bandGain[0], 4 },
                { bandQ[0], 0.8f }, { bandOn[3], 1 }, { bandFreq[3], 4000 }, { bandGain[3], 3 },
                { bandQ[3], 1.2f }, { compOn, 1 }, { compThresh, -22 }, { compRatio, 4 },
                { compAttack, 12 }, { compRelease, 120 }, { compKnee, 3 }, { compAutoMk, 1 },
                { compMode, 0 }, { compOptBlend, 25 }, { scHpf, 110 }, { transformer, 2 },
                { tubeOn, 1 }, { tubeModel, 2 }, { tubeDrive, 55 }, { tubeBias, -25 },
                { tubeTone, 18 }, { circuitBend, 15 }, { monoFreq, 120 }, { width, 115 } } },

            { "15 - Dusty Boom Bap", {
                { inputTrim, 0 }, { headroom, 5 }, { oversample, 1 }, { autoGain, 1 },
                { mix, 100 }, { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 30 },
                { lpOn, 1 }, { lpFreq, 16000 }, { tight, 30 }, { bandOn[1], 1 },
                { bandFreq[1], 280 }, { bandGain[1], 2.5f }, { bandQ[1], 0.9f }, { compOn, 1 },
                { compThresh, -20 }, { compRatio, 3.5f }, { compAttack, 8 }, { compRelease, 140 },
                { compKnee, 4 }, { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 40 },
                { scHpf, 100 }, { transformer, 1 }, { tubeOn, 1 }, { tubeModel, 0 },
                { tubeDrive, 32 }, { tubeBias, 20 }, { tubeTone, -10 }, { tapeOn, 1 },
                { tapeDrive, 38 }, { tapeSpeed, 0 }, { tapeLF, 2 }, { tapeHF, -1.5f },
                { monoFreq, 100 }, { width, 108 } } },

            { "16 - Cassette Lullaby Haze", {
                { inputTrim, 0 }, { headroom, 6 }, { oversample, 1 }, { autoGain, 1 },
                { mix, 100 }, { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 40 },
                { lpOn, 1 }, { lpFreq, 11000 }, { bandOn[0], 1 }, { bandFreq[0], 80 },
                { bandGain[0], 3 }, { bandQ[0], 0.7f }, { compOn, 1 }, { compThresh, -19 },
                { compRatio, 3 }, { compAttack, 25 }, { compRelease, 220 }, { compKnee, 6 },
                { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 85 }, { scHpf, 90 },
                { transformer, 1 }, { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 28 },
                { tubeBias, 15 }, { tubeTone, -22 }, { tapeOn, 1 }, { tapeDrive, 55 },
                { tapeSpeed, 0 }, { tapeLF, 3 }, { tapeHF, -4 }, { circuitBend, 32 },
                { drift, 45 }, { monoFreq, 130 }, { width, 92 } } },

            { "17 - Velvet Overdrive", {
                { inputTrim, 0 }, { headroom, 3 }, { oversample, 1 }, { eqOn, 1 },
                { hpOn, 1 }, { hpFreq, 32 }, { tight, 30 }, { bandOn[0], 1 },
                { bandFreq[0], 90 }, { bandGain[0], 1.5f }, { bandQ[0], 0.7f }, { bandOn[2], 1 },
                { bandFreq[2], 1200 }, { bandGain[2], 2 }, { bandQ[2], 0.9f }, { compOn, 1 },
                { compThresh, -18 }, { compRatio, 2.5f }, { compAttack, 35 }, { compRelease, 250 },
                { compKnee, 8 }, { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 80 },
                { scHpf, 90 }, { transformer, 1 }, { tubeOn, 1 }, { tubeModel, 0 },
                { tubeDrive, 28 }, { tubeBias, 20 }, { tubeTone, -8 }, { tapeOn, 1 },
                { tapeDrive, 20 }, { tapeSpeed, 1 }, { tapeLF, 1 }, { autoGain, 1 } } },

            { "18 - Titanium Wall", {
                { inputTrim, 0 }, { headroom, 6 }, { oversample, 2 }, { eqOn, 1 },
                { hpOn, 1 }, { hpFreq, 38 }, { tight, 55 }, { air, 30 },
                { bandOn[3], 1 }, { bandFreq[3], 4200 }, { bandGain[3], 3 }, { bandQ[3], 1.2f },
                { bandOn[1], 1 }, { bandFreq[1], 320 }, { bandGain[1], -2.5f }, { bandQ[1], 1.4f },
                { compOn, 1 }, { compThresh, -24 }, { compRatio, 4 }, { compAttack, 12 },
                { compRelease, 120 }, { compKnee, 4 }, { compAutoMk, 1 }, { compMode, 2 },
                { compOptBlend, 30 }, { scHpf, 120 }, { transformer, 1 }, { tubeOn, 1 },
                { tubeModel, 1 }, { tubeDrive, 45 }, { tubeBias, -10 }, { tubeTone, 12 },
                { tapeOn, 1 }, { tapeDrive, 30 }, { tapeSpeed, 1 }, { tapeHF, -1 },
                { monoFreq, 110 }, { width, 120 }, { autoGain, 1 } } },

            { "19 - Garage Filament", {
                { inputTrim, 0 }, { headroom, 4 }, { oversample, 2 }, { drift, 30 },
                { circuitBend, 22 }, { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 45 },
                { lpOn, 1 }, { lpFreq, 14000 }, { bandOn[2], 1 }, { bandFreq[2], 1400 },
                { bandGain[2], 3.5f }, { bandQ[2], 1.1f }, { compOn, 1 }, { compThresh, -16 },
                { compRatio, 3 }, { compAttack, 20 }, { compRelease, 180 }, { compKnee, 6 },
                { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 45 }, { scHpf, 85 },
                { transformer, 2 }, { tubeOn, 1 }, { tubeModel, 2 }, { tubeDrive, 52 },
                { tubeBias, 35 }, { tubeTone, -14 }, { tapeOn, 1 }, { tapeDrive, 45 },
                { tapeSpeed, 0 }, { tapeLF, 2 }, { tapeHF, -2 }, { autoGain, 1 } } },

            { "20 - Snap And Spit", {
                { inputTrim, 0 }, { headroom, 2 }, { oversample, 1 }, { eqOn, 1 },
                { hpOn, 1 }, { hpFreq, 40 }, { tight, 45 }, { air, 22 },
                { bandOn[3], 1 }, { bandFreq[3], 5000 }, { bandGain[3], 2.5f }, { bandQ[3], 1 },
                { bandOn[0], 1 }, { bandFreq[0], 80 }, { bandGain[0], -1.5f }, { bandQ[0], 0.8f },
                { compOn, 1 }, { compThresh, -20 }, { compRatio, 6 }, { compAttack, 3 },
                { compRelease, 90 }, { compKnee, 2 }, { compAutoMk, 1 }, { compMode, 1 },
                { compOptBlend, 10 }, { scHpf, 100 }, { transformer, 1 }, { tubeOn, 1 },
                { tubeModel, 1 }, { tubeDrive, 22 }, { tubeBias, 0 }, { tubeTone, 15 },
                { tapeOn, 0 }, { width, 115 }, { autoGain, 1 } } },

            { "21 - Radio Ready Sheen", {
                { inputTrim, 0 }, { headroom, 3 }, { autoGain, 1 }, { oversample, 1 },
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 30 }, { tight, 22 },
                { air, 40 }, { bandOn[3], 1 }, { bandFreq[3], 5000 }, { bandGain[3], 2 },
                { bandQ[3], 0.9f }, { compOn, 1 }, { compThresh, -18 }, { compRatio, 2 },
                { compAttack, 30 }, { compRelease, 180 }, { compKnee, 8 }, { compAutoMk, 1 },
                { compMode, 0 }, { compOptBlend, 70 }, { scHpf, 90 }, { transformer, 0 },
                { width, 108 }, { outputTrim, 0 } } },

            { "22 - Modern Pop Gloss", {
                { inputTrim, 0 }, { headroom, 5 }, { autoGain, 1 }, { oversample, 1 },
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 28 }, { air, 48 },
                { bandOn[0], 1 }, { bandFreq[0], 80 }, { bandGain[0], 2 }, { bandQ[0], 0.7f },
                { compOn, 1 }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 15 },
                { compRelease, 150 }, { compKnee, 6 }, { compAutoMk, 1 }, { compMode, 2 },
                { compOptBlend, 60 }, { scHpf, 100 }, { transformer, 0 }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 22 }, { tubeBias, 15 }, { tubeTone, 20 },
                { width, 115 }, { outputTrim, 0 } } },

            { "23 - Acoustic Pop Warmth", {
                { inputTrim, 0 }, { headroom, 4 }, { autoGain, 1 }, { oversample, 1 },
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 34 }, { tight, 18 },
                { air, 30 }, { bandOn[3], 1 }, { bandFreq[3], 4500 }, { bandGain[3], 1.5f },
                { bandQ[3], 1 }, { compOn, 1 }, { compThresh, -16 }, { compRatio, 2 },
                { compAttack, 40 }, { compRelease, 250 }, { compKnee, 10 }, { compAutoMk, 1 },
                { compMode, 0 }, { compOptBlend, 90 }, { scHpf, 85 }, { transformer, 1 },
                { tapeOn, 1 }, { tapeDrive, 20 }, { tapeSpeed, 0 }, { tapeLF, 1 },
                { tapeHF, -1 }, { width, 105 }, { outputTrim, 0 } } },

            { "24 - Vocal Up Front", {
                { inputTrim, 0 }, { headroom, 3 }, { autoGain, 1 }, { oversample, 1 },
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 32 }, { tight, 30 },
                { air, 45 }, { bandOn[2], 1 }, { bandFreq[2], 2500 }, { bandGain[2], 2 },
                { bandQ[2], 1.2f }, { bandOn[1], 1 }, { bandFreq[1], 350 }, { bandGain[1], -2 },
                { bandQ[1], 1.4f }, { compOn, 1 }, { compThresh, -22 }, { compRatio, 3 },
                { compAttack, 20 }, { compRelease, 120 }, { compKnee, 6 }, { compAutoMk, 1 },
                { compMode, 1 }, { compOptBlend, 50 }, { scHpf, 110 }, { transformer, 0 },
                { width, 100 }, { monoFreq, 100 }, { outputTrim, 0 } } },

            { "25 - Hearthside Folk", {
                { inputTrim, 0 }, { outputTrim, 0 }, { autoGain, 1 }, { oversample, 1 },
                { eqOn, 1 }, { hpOn, 1 }, { hpFreq, 32 }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 12 }, { tubeBias, 15 }, { tubeTone, -5 },
                { tapeOn, 1 }, { tapeDrive, 10 }, { tapeSpeed, 0 }, { tapeLF, 1 },
                { tapeHF, -1 }, { width, 110 }, { compOn, 0 } } },

            { "26 - Blue Note Trio", {
                { autoGain, 1 }, { oversample, 1 }, { eqOn, 1 }, { hpOn, 1 },
                { hpFreq, 30 }, { air, 15 }, { compOn, 1 }, { compThresh, -18 },
                { compRatio, 1.8f }, { compAttack, 30 }, { compRelease, 250 }, { compKnee, 8 },
                { compAutoMk, 1 }, { compMode, 0 }, { compOptBlend, 85 }, { scHpf, 90 },
                { transformer, 1 }, { width, 105 } } },

            { "27 - Concert Hall Air", {
                { autoGain, 1 }, { oversample, 1 }, { eqOn, 1 }, { hpOn, 1 },
                { hpFreq, 24 }, { propQ, 1 }, { air, 20 }, { tight, 10 },
                { width, 130 }, { drift, 15 }, { monoFreq, 100 }, { compOn, 0 },
                { tubeOn, 0 } } },

            { "28 - Whispered Verses", {
                { autoGain, 1 }, { oversample, 1 }, { eqOn, 1 }, { hpOn, 1 },
                { hpFreq, 28 }, { tight, 18 }, { air, 18 }, { bandOn[2], 1 },
                { bandFreq[2], 2500 }, { bandGain[2], 1.5f }, { bandQ[2], 0.8f }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 8 }, { tubeBias, 20 }, { compOn, 1 },
                { compThresh, -16 }, { compRatio, 2 }, { compAttack, 25 }, { compRelease, 200 },
                { compOptBlend, 70 }, { compAutoMk, 1 }, { transformer, 0 }, { scHpf, 80 },
                { width, 105 } } },

            { "29 - Triode Velvet", {
                { tubeOn, 1 }, { tubeModel, 0 }, { tubeDrive, 34 }, { tubeBias, 28 },
                { tubeTone, 12 }, { headroom, 4 }, { eqOn, 1 }, { air, 22 },
                { tight, 18 }, { bandOn[0], 1 }, { bandFreq[0], 90 }, { bandGain[0], 1.5f },
                { bandQ[0], 0.7f }, { drift, 12 }, { oversample, 1 }, { autoGain, 1 },
                { mix, 100 } } },

            { "30 - Starved Filament Grit", {
                { tubeOn, 1 }, { tubeModel, 2 }, { tubeDrive, 62 }, { tubeBias, -34 },
                { tubeTone, -14 }, { headroom, 8 }, { circuitBend, 22 }, { eqOn, 1 },
                { hpOn, 1 }, { hpFreq, 32 }, { tight, 26 }, { compOn, 1 },
                { compThresh, -18 }, { compRatio, 3 }, { compAttack, 22 }, { compRelease, 180 },
                { compOptBlend, 20 }, { compAutoMk, 1 }, { scHpf, 90 }, { transformer, 2 },
                { drift, 30 }, { oversample, 2 }, { autoGain, 1 }, { mix, 100 } } },

            { "31 - Thirty IPS Glue", {
                { tapeOn, 1 }, { tapeDrive, 40 }, { tapeSpeed, 1 }, { tapeLF, 1.5f },
                { tapeHF, -1 }, { compOn, 1 }, { compThresh, -20 }, { compRatio, 2 },
                { compAttack, 35 }, { compRelease, 250 }, { compKnee, 8 }, { compOptBlend, 85 },
                { compMode, 0 }, { scHpf, 90 }, { transformer, 0 }, { compAutoMk, 1 },
                { eqOn, 1 }, { air, 18 }, { oversample, 1 }, { autoGain, 1 },
                { mix, 100 } } },

            { "32 - Iron Console Warmth", {
                { tubeOn, 1 }, { tubeModel, 1 }, { tubeDrive, 30 }, { tubeBias, 10 },
                { tubeTone, -6 }, { tapeOn, 1 }, { tapeDrive, 32 }, { tapeSpeed, 0 },
                { tapeLF, 2 }, { tapeHF, -2 }, { compOn, 1 }, { compThresh, -16 },
                { compRatio, 2.5f }, { compAttack, 28 }, { compRelease, 220 }, { compOptBlend, 60 },
                { scHpf, 85 }, { transformer, 1 }, { compAutoMk, 1 }, { eqOn, 1 },
                { bandOn[1], 1 }, { bandFreq[1], 300 }, { bandGain[1], 2 }, { bandQ[1], 0.9f },
                { tight, 20 }, { drift, 22 }, { headroom, 5 }, { oversample, 2 },
                { autoGain, 1 }, { mix, 100 } } },

            { "33 - Transparent Bus Glue", {
                { oversample, 1 }, { autoGain, 1 }, { mix, 100 }, { compOn, 1 },
                { compThresh, -20 }, { compRatio, 2 }, { compAttack, 30 }, { compRelease, 250 },
                { compKnee, 8 }, { compAutoMk, 1 }, { compOptBlend, 85 }, { compMode, 0 },
                { scHpf, 90 }, { transformer, 0 } } },

            { "34 - Low End Anchor", {
                { oversample, 1 }, { autoGain, 1 }, { eqOn, 1 }, { hpOn, 1 },
                { hpFreq, 28 }, { tight, 45 }, { bandOn[0], 1 }, { bandFreq[0], 80 },
                { bandGain[0], -2 }, { bandQ[0], 0.7f }, { monoFreq, 120 }, { compOn, 1 },
                { compThresh, -18 }, { compRatio, 2 }, { compAttack, 40 }, { compRelease, 200 },
                { compKnee, 6 }, { compMode, 2 }, { compAutoMk, 1 }, { scHpf, 120 } } },

            { "35 - Wide Open Master", {
                { oversample, 1 }, { autoGain, 1 }, { width, 135 }, { drift, 15 },
                { monoFreq, 100 }, { eqOn, 1 }, { air, 25 }, { bandOn[4], 1 },
                { bandFreq[4], 12000 }, { bandGain[4], 1.5f }, { bandQ[4], 0.7f } } },

            { "36 - Finish Line Polish", {
                { oversample, 2 }, { autoGain, 1 }, { headroom, 2 }, { compOn, 1 },
                { compThresh, -7 }, { compRatio, 4 }, { compAttack, 10 }, { compRelease, 120 },
                { compAutoMk, 1 }, { compOptBlend, 25 }, { scHpf, 100 }, { tubeOn, 1 },
                { tubeModel, 0 }, { tubeDrive, 15 }, { tapeOn, 1 }, { tapeDrive, 18 },
                { tapeSpeed, 1 }, { outputTrim, -0.3f } } },
        };
        return presets;
    }
}
