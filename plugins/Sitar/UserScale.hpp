/*
 * Sitar — user-defined scale library parser.
 *
 * Turns the serialized "userscales" state string into up to 8 scale slots.
 * Format: one scale per line, "Name | i1, i2, ...". Line i fills slot i
 * (positional, so slot indices are stable across edits). Intervals are:
 *   p/q            -> ratio p/q
 *   number with .  -> cents, ratio = 2^(cents/1200)   (Scala convention)
 *   bare integer   -> ratio n/1                        (Scala convention)
 * The tonic 1/1 is implicit (ratios[0]); a listed value ~1.0 is not doubled.
 * Header-only so both the plugin and the host-less tests link it directly.
 */
#ifndef SITAR_USER_SCALE_HPP_INCLUDED
#define SITAR_USER_SCALE_HPP_INCLUDED

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace sitar {

static constexpr uint32_t kMaxUserScales    = 8;
static constexpr uint32_t kMaxScaleNotes    = 12;   // ratios[] capacity
static constexpr uint32_t kUserScaleNameCap = 32;

struct UserScale {
    bool     valid;
    char     name[kUserScaleNameCap];
    uint32_t notesPerOctave;                 // >= 1; ratios[0..notesPerOctave-1]
    float    ratios[kMaxScaleNotes];         // ratios[0] == 1.0 (tonic)
};

// Parse one interval token to a frequency ratio (> 0). Returns false and
// leaves ratioOut untouched for empty / unparseable / non-positive tokens.
inline bool parseInterval(const char* tok, float& ratioOut)
{
    while (*tok == ' ' || *tok == '\t') ++tok;
    if (*tok == '\0') return false;

    if (const char* slash = std::strchr(tok, '/'))
    {
        const double num = std::atof(tok);
        const double den = std::atof(slash + 1);
        if (num <= 0.0 || den <= 0.0) return false;
        ratioOut = static_cast<float>(num / den);
        return true;
    }
    if (std::strchr(tok, '.') != nullptr)              // cents
    {
        const double cents = std::atof(tok);
        const float  ratio = static_cast<float>(std::pow(2.0, cents / 1200.0));
        if (ratio <= 0.0f) return false;
        ratioOut = ratio;
        return true;
    }
    const double n = std::atof(tok);                   // bare int -> n/1
    if (n <= 0.0) return false;
    ratioOut = static_cast<float>(n);
    return true;
}

// Parse the library string into out[0..maxOut-1] positionally (line i -> slot
// i). Slots whose line has no name or no '|' are marked invalid.
inline void parseUserScales(const char* text, UserScale* out, uint32_t maxOut)
{
    for (uint32_t i = 0; i < maxOut; ++i) { out[i].valid = false; out[i].name[0] = '\0'; out[i].notesPerOctave = 0; }
    if (text == nullptr) return;

    const char* line = text;
    uint32_t slot = 0;
    while (*line != '\0' && slot < maxOut)
    {
        const char* eol     = std::strchr(line, '\n');
        const char* lineEnd = (eol != nullptr) ? eol : (line + std::strlen(line));

        const char* bar = nullptr;
        for (const char* c = line; c < lineEnd; ++c) { if (*c == '|') { bar = c; break; } }

        if (bar != nullptr)
        {
            UserScale& s = out[slot];

            const char* nb = line;
            const char* ne = bar;
            while (nb < ne && (*nb == ' ' || *nb == '\t')) ++nb;
            while (ne > nb && (*(ne - 1) == ' ' || *(ne - 1) == '\t')) --ne;
            uint32_t ni = 0;
            for (const char* c = nb; c < ne && ni < kUserScaleNameCap - 1; ++c) s.name[ni++] = *c;
            s.name[ni] = '\0';

            s.ratios[0]      = 1.0f;
            s.notesPerOctave = 1;

            char buf[64];
            const char* p = bar + 1;
            while (p < lineEnd && s.notesPerOctave < kMaxScaleNotes)
            {
                const char* comma = p;
                while (comma < lineEnd && *comma != ',') ++comma;
                uint32_t bi = 0;
                for (const char* c = p; c < comma && bi < sizeof(buf) - 1; ++c) buf[bi++] = *c;
                buf[bi] = '\0';

                float ratio;
                if (parseInterval(buf, ratio) && std::fabs(ratio - 1.0f) > 1e-4f)
                    s.ratios[s.notesPerOctave++] = ratio;

                p = (comma < lineEnd) ? comma + 1 : lineEnd;
            }

            s.valid = (s.name[0] != '\0');
        }

        ++slot;
        if (eol == nullptr) break;
        line = eol + 1;
    }
}

} // namespace sitar

#endif // SITAR_USER_SCALE_HPP_INCLUDED
