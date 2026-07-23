/*
 * Regression test: the user-scale library parser (UserScale.hpp).
 *   - parseInterval: ratios (p/q), cents (has '.'), bare int (n/1), garbage.
 *   - parseUserScales: positional slot fill, name trim, implicit/explicit
 *     tonic, cents->ratio, note-count and slot clamping, blank/malformed lines.
 *
 * Build (host-less, no DPF needed):
 *   g++ -std=gnu++14 -g -O0 -fsanitize=address -I../plugins/Sitar \
 *       test_userscale_parse.cpp -o test_userscale_parse && ./test_userscale_parse
 */
#include "UserScale.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace sitar;

static int gFail = 0;
static void near(const char* what, float a, float b)
{
    if (std::fabs(a - b) > 1e-3f) { std::printf("FAIL: %s: got %.5f want %.5f\n", what, a, b); ++gFail; }
}
static void eqi(const char* what, long a, long b)
{
    if (a != b) { std::printf("FAIL: %s: got %ld want %ld\n", what, a, b); ++gFail; }
}
static void istrue(const char* what, bool c)
{
    if (!c) { std::printf("FAIL: %s\n", what); ++gFail; }
}

int main()
{
    // ---- parseInterval ----
    float r = 0.0f;
    istrue("ratio parses",       parseInterval("3/2", r));    near("3/2", r, 1.5f);
    istrue("cents parses",       parseInterval("1200.0", r)); near("1200c=2.0", r, 2.0f);
    istrue("cents neutral 3rd",  parseInterval("347.4", r));  near("347.4c", r, std::pow(2.0f, 347.4f/1200.0f));
    istrue("bare int = n/1",     parseInterval("2", r));      near("2 -> 2/1", r, 2.0f);
    istrue("leading space ok",   parseInterval("  5/4", r));  near("sp 5/4", r, 1.25f);
    istrue("empty rejected",    !parseInterval("", r));
    istrue("garbage rejected",  !parseInterval("abc", r));
    istrue("zero denom rejected",!parseInterval("1/0", r));
    istrue("zero int rejected",  !parseInterval("0", r));

    // ---- parseUserScales ----
    UserScale s[8];

    // implicit tonic; ratios + cents mixed; name trimmed
    parseUserScales("  My Rast | 9/8, 347.4, 4/3, 3/2, 27/16, 1049.0  \n", s, 8);
    istrue("slot0 valid", s[0].valid);
    istrue("name trimmed", std::strcmp(s[0].name, "My Rast") == 0);
    eqi("rast notes = 7 (tonic+6)", s[0].notesPerOctave, 7);
    near("rast[0]=1", s[0].ratios[0], 1.0f);
    near("rast[1]=9/8", s[0].ratios[1], 1.125f);
    near("rast[4]=3/2", s[0].ratios[4], 1.5f);
    istrue("slot1 empty", !s[1].valid);

    // explicit tonic (1/1 first) must not double-count
    parseUserScales("Ex | 1/1, 5/4, 3/2\n", s, 8);
    eqi("explicit tonic -> 3 notes", s[0].notesPerOctave, 3);
    near("ex[0]=1", s[0].ratios[0], 1.0f);
    near("ex[1]=5/4", s[0].ratios[1], 1.25f);

    // positional slots: blank + malformed lines leave slots invalid
    parseUserScales("A | 9/8\n\nnobar line\nD | 6/5\n", s, 8);
    istrue("slot0 A valid", s[0].valid && std::strcmp(s[0].name, "A") == 0);
    istrue("slot1 blank invalid", !s[1].valid);
    istrue("slot2 nobar invalid", !s[2].valid);
    istrue("slot3 D valid", s[3].valid && std::strcmp(s[3].name, "D") == 0);

    // clamps: >12 notes and >8 slots
    parseUserScales("Big | 2,3,4,5,6,7,8,9,10,11,12,13,14,15\n", s, 8);
    istrue("notes clamped <= 12", s[0].notesPerOctave <= 12);
    parseUserScales("1|2\n2|2\n3|2\n4|2\n5|2\n6|2\n7|2\n8|2\n9|2\n", s, 8);
    istrue("slot7 valid", s[7].valid);  // 9th line ignored (maxOut=8)

    if (gFail) { std::printf("%d failure(s)\n", gFail); return 1; }
    std::printf("ok: user-scale parser (intervals, slots, tonic, cents, clamps)\n");
    return 0;
}
