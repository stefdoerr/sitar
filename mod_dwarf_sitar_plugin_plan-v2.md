# Development Plan: Sympathetic Resonance LV2 Plugin for MOD Dwarf

**Context for the Agent:** You are tasked with writing the C++ DSP code and HTML/JS Web GUI code for an audio plugin targeting the MOD Dwarf hardware ecosystem. The plugin is a "Sympathetic String Simulator" (similar to a sitar). It uses the live audio input to excite a bank of tuned resonators, creating a lush, microtonal ringing effect. 

The framework to be used is the **DISTRHO Plugin Framework (DPF)** for the C++ DSP and the **MOD SDK** for the Web GUI.

## 1. Core Architecture & DSP (C++)
The DSP engine acts as a parallel filter bank. It does not track pitch or generate oscillators; it passes live audio through 13 tuned feedback comb filters.

### Audio Specifications
* **Sample Rate:** Dynamic (typically 48kHz on MOD Dwarf).
* **Audio Ports:** 1 Mono Input, 2 Stereo Outputs (or Mono-to-Mono depending on user preference).
* **Buffer Management:** The comb filters require fractional delay lines (linear interpolation) to accurately tune to microtonal frequencies. Max delay buffer size per string should comfortably support frequencies down to at least 50 Hz ($D = \frac{f_s}{f}$).

### Plugin Parameters (LV2 Ports)
The C++ `Plugin` class must implement the following parameters:
* **`PARAM_STRING_1` through `PARAM_STRING_13`:** Float. Range: 50.0 Hz to 2000.0 Hz. (Base frequencies controlled by the GUI).
* **`PARAM_OCTAVE`:** Integer. Range: -2 to +2. Default: +1. (Global pitch shift).
* **`PARAM_DECAY`:** Float. Range: 0.0 to 0.99. (Controls the feedback coefficient of the comb filters).
* **`PARAM_MIX`:** Float. Range: 0.0 to 1.0. (Dry/Wet blend).
* **`PARAM_JAWARI`:** Float. Range: 0.0 to 1.0. (Drives a soft-clipper/wavefolder inside or immediately after the feedback loop to simulate sitar bridge buzz).

### DSP Logic (The `run()` block)
1.  **Calculate Octave Multiplier:** Read `PARAM_OCTAVE`. Calculate the frequency multiplier using $Multiplier = 2^{\text{octave}}$.
2.  **Tune the Strings:** For strings 1 to 13, calculate the final target frequency: `targetFreq = baseFreqParam * Multiplier`.
3.  **Calculate Delay Times:** Convert `targetFreq` into samples: $D = \frac{f_s}{\text{targetFreq}}$. Update the delay times for the 13 fractional delay lines.
4.  **Process Audio:** * Split input into `dry` and `wet`.
    * Pass the `wet` sample simultaneously into all 13 comb filters.
    * Sum the outputs of the 13 comb filters. 
    * Apply the `PARAM_JAWARI` saturation to the summed wet signal.
    * Blend `dry` and `wet` using `PARAM_MIX` and send to the output buffers.

## 2. Web GUI Architecture (HTML / CSS / JS)
The Web GUI handles all the microtonal scale mathematics. It intercepts user scale selections, calculates the absolute Hz values, and pushes those values to the 13 string parameters in the DSP.

### Interface Elements
* **13 String Knobs:** Bound to `PARAM_STRING_1` through `PARAM_STRING_13`. Display value should be formatted as `Hz`.
* **Control Knobs:** Octave, Decay, Mix, Jawari.
* **Scale Dropdown (`<select>`):** Contains options like "Just Major", "Raga Bhairavi", etc.
* **Root Note Dropdown (`<select>`):** Contains 12 standard root notes (e.g., C2, C#2, D2) with underlying numeric values representing their exact base frequencies in Hz (e.g., C2 = 65.41 Hz).

### JavaScript Logic (`script.js`)
The agent must implement the following logic in the GUI's JavaScript:

1.  **Data Structure:** A dictionary of scales containing arrays of microtonal ratios (including the fundamental `1.0`). 
    * *Example:* `"Raga Bhairavi": [1.0, 1.0535, 1.1851, 1.3333, 1.5, 1.5802, 1.7777]`
2.  **The Update Function:** When the "Scale" or "Root Note" dropdown changes:
    * Fetch the selected Root Frequency (in Hz).
    * Fetch the selected Scale Ratio array.
    * Loop 13 times (for 13 strings).
    * For each string, calculate the frequency: `Freq = RootHz * Ratio`.
    * *Note on Octaves:* Because scales typically define only 5 to 7 notes, the script must wrap around to the next octave. If the scale has 7 notes, String 8's ratio is `ScaleRatio[0] * 2.0`, String 9 is `ScaleRatio[1] * 2.0`, etc.
    * Update the visual knob positions on the GUI.
    * Send the new Hz values to the hardware using the MOD SDK API (e.g., `xapi.send_parameter()`).

## 3. Implementation Steps for the Agent
1.  **Scaffold the DPF Project:** Set up the `Makefile`, `Plugin.cpp`, and `DistrhoPluginInfo.h` required for an LV2 build.
2.  **Build the Comb Filter Class:** Write a clean, reusable C++ class for a Feedback Comb Filter with linear interpolation for fractional delay times.
3.  **Implement DSP `run()`:** Wire up the 13 comb filters, the Octave multiplier math, and the summing/mix logic.
4.  **Create the Web GUI:** Scaffold the `index.html`, `style.css`, and `script.js` adhering to the MOD SDK structure.
5.  **Write the JS Tuning Logic:** Implement the ratio-multiplication loop and the event listeners for the dropdown menus. 

## 4. Reference Templates & Examples (GitHub)
To accelerate development, the agent should use the following open-source repositories as templates and references:

* **C++ Architecture Template:** Use `DISTRHO/DPF` ([examples/Parameters](https://github.com/DISTRHO/DPF/tree/master/examples/Parameters)). This provides the bare-bones boilerplate for defining audio ports, registering LV2 parameters, and setting up the `run()` block.
* **DSP / Comb Filter Math:** Look at `DISTRHO/DPF-Plugins` ([github.com/DISTRHO/DPF-Plugins](https://github.com/DISTRHO/DPF-Plugins)). Specifically, study the comb filter implementations in reverbs like **MVerb** or **Freeverb** (`comb.cpp`/`comb.hpp`) for handling delay line buffers and feedback loops.
* **Web GUI Template:** Use `moddevices/mod-lv2-examples` ([github.com/moddevices/mod-lv2-examples](https://github.com/moddevices/mod-lv2-examples)). This shows exactly how to structure the HTML, CSS, and JS files, and how to define them in the LV2 `manifest.ttl` file so the MOD Dwarf UI can load them.

**Updated Agent Instruction:** Please begin by providing the C++ code for the `Plugin.cpp` and the fractional delay line class. Ensure the code adheres strictly to the DPF structure. Use the `examples/Parameters` project from DPF as the structural template, and adapt a standard Comb Filter implementation (like those in DPF-Plugins' Freeverb) for the fractional delay line.
