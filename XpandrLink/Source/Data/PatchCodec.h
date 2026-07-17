/*
  PatchCodec.h
  Codec for the Oberheim Xpander / Matrix-12 single-patch binary format.

  decode: 196 raw bytes (unpacked from SysEx) → {paramID → value} map
  encode: {paramID → value} map → 196 raw bytes

  Both functions are pure C++ with no JUCE dependency; the byte layout mirrors
  XPanderSinglePatch.FromStream / ToStream in the C# reference implementation.
*/
#pragma once
#include <cstdint>
#include <map>
#include <vector>

namespace PatchCodec
{

// Maps the 196 decoded patch bytes (Oberheim binary layout) to XpanderParam IDs.
// Returns a map of {paramID → value} for every parameter in the patch.
inline std::map<int,int> decode(const std::vector<uint8_t>& r)
{
    std::map<int,int> p;
    if (r.size() < 196) return p;

    auto bit = [&](uint8_t byte, int n) { return (byte >> n) & 1; };
    auto sgn = [](uint8_t v) -> int { return (int)(int8_t)v; };

    // ---- VCOs (2 × 6 bytes, offsets 0 and 6) --------------------------------
    // VCO1 IDs: 0-8  (wave=4, mod: KEYB=5, LAG=6, LEV1=7, VIB=8)
    // VCO2 IDs: 20-29 (wave=24, SYNC=25, mod: KEYB=26, LAG=27, LEV1=28, VIB=29)
    // Flags: TRI=0x01, SAW=0x02, PULSE=0x04, SYNC=0x08, NOISE=0x10
    //        KEYBD=0x01, LAG=0x02, LEV_1=0x04, VIB=0x08
    const int vcoBase[2] = {0, 20};
    for (int i = 0; i < 2; ++i) {
        int off  = i * 6;
        int base = vcoBase[i];
        p[base+0] = r[off+0];           // freq
        p[base+1] = sgn(r[off+1]);      // detune (signed)
        p[base+2] = r[off+2];           // pw
        p[base+3] = r[off+3];           // vol
        uint8_t mod  = r[off+4];
        uint8_t wave = r[off+5];
        if (i == 0) {
            // VCO1: wave bitmask at base+4 (id=4), mod flags at base+5..8 (ids 5-8)
            p[base+4] = wave & 0x07;            // TRI(bit0)/SAW(bit1)/PULSE(bit2)
            p[base+5] = bit(mod,0);             // KEYB  (MODFLAG_KEYBD=0x01)
            p[base+6] = bit(mod,1);             // LAG   (MODFLAG_LAG=0x02)
            p[base+7] = bit(mod,2);             // LEV1  (MODFLAG_LEV_1=0x04)
            p[base+8] = bit(mod,3);             // VIB   (MODFLAG_VIB=0x08)
        } else {
            // VCO2: wave bitmask at base+4 (id=24), SYNC at base+5 (id=25),
            //        mod flags at base+6..9 (ids 26-29)
            p[base+4] = (wave & 0x07) | (bit(wave,4) << 3);  // TRI/SAW/PULSE/NOISE bitmask
            p[base+5] = bit(wave,3);             // SYNC (VCOWAVEFLAG_SYNC=0x08)
            p[base+6] = bit(mod,0);              // KEYB  (MODFLAG_KEYBD=0x01)
            p[base+7] = bit(mod,1);              // LAG   (MODFLAG_LAG=0x02)
            p[base+8] = bit(mod,2);              // LEV1  (MODFLAG_LEV_1=0x04)
            p[base+9] = bit(mod,3);              // VIB   (MODFLAG_VIB=0x08)
        }
    }

    // ---- VCF (6 bytes, offset 12) --- IDs 40-48 ----------------------------
    p[40] = r[12];                       // freq
    p[41] = r[13];                       // res
    p[42] = r[14];                       // fmode (menu)
    p[43] = r[15];                       // vca1
    p[44] = r[16];                       // vca2
    { uint8_t m = r[17];
      p[45]=bit(m,0); p[46]=bit(m,1); p[47]=bit(m,2); p[48]=bit(m,3); }

    // ---- FM/LAG (5 bytes, offset 18) --- IDs 60-66 -------------------------
    p[60] = r[18];                       // f_amp
    p[61] = r[19];                       // f_dest (menu)
    p[62] = r[20];                       // lag_in (menu)
    p[63] = r[21];                       // lag_rate
    { uint8_t m = r[22];                 // lag_mode
      p[64]=bit(m,0);                    // LEGATO (LAGMODE_LEGATO=0x01)
      p[65]=bit(m,1);                    // LIN_EXP (LAGMODE_EXPO=0x02)
      p[66]=bit(m,2); }                  // EQUAL_TIME (LAGMODE_EQUAL_TIME=0x04)

    // ---- LFOs (5 × 7 bytes, offset 23) --- base IDs 70,80,90,100,110 ------
    // Struct order: speed, retrig_mode, lag, wave, retrig, sample, amp
    const int lfoBase[5] = {70,80,90,100,110};
    for (int i = 0; i < 5; ++i) {
        int off  = 23 + i * 7;
        int base = lfoBase[i];
        p[base+0] = r[off+0];   // speed
        p[base+1] = r[off+3];   // waveshape (wave)
        p[base+2] = r[off+5];   // sample_in
        p[base+3] = r[off+4];   // retrig point
        p[base+4] = r[off+6];   // amp
        p[base+5] = r[off+2];   // lag (button)
        p[base+6] = r[off+1];   // retrig_mode (menu)
    }

    // ---- ENVs (5 × 8 bytes, offset 58) --- base IDs 130,150,170,190,210 ---
    // Struct order: flags, lfotrig, delay, attack, decay, sustain, release, amp
    // Flag bits: RESET=0x01, MULTI=0x04, GATED=0x08, EXTRIG=0x10,
    //            LFOTRIG=0x20, DADR=0x40, FREERUN=0x80
    const int envBase[5] = {130,150,170,190,210};
    for (int i = 0; i < 5; ++i) {
        int off  = 58 + i * 8;
        int base = envBase[i];
        uint8_t fl = r[off+0];
        p[base+0]  = r[off+2];   // delay
        p[base+1]  = r[off+3];   // attack
        p[base+2]  = r[off+4];   // decay
        p[base+3]  = r[off+5];   // sustain
        p[base+4]  = r[off+6];   // release
        p[base+5]  = r[off+7];   // amp
        p[base+6]  = bit(fl,0);  // MODE_RESET   (ENVMODE_RESET=0x01)
        p[base+7]  = bit(fl,7);  // MODE_FREERUN (ENVMODE_FREERUN=0x80)
        p[base+8]  = bit(fl,6);  // MODE_DADR    (ENVMODE_DADR=0x40)
        p[base+9]  = bit(fl,2);  // TRIG_MULTI   (ENVMODE_MULTI=0x04)
        p[base+10] = bit(fl,4);  // TRIG_EXTRIG  (ENVMODE_EXTRIG=0x10)
        p[base+11] = bit(fl,5);  // TRIG_LFO     (ENVMODE_LFOTRIG=0x20)
        p[base+12] = (bit(fl,5)) ? (int)r[off+1] : 0;  // TRIG_LFOSRC (only valid when LFOTRIG set)
        p[base+13] = bit(fl,3);  // TRIG_GATED   (ENVMODE_GATED=0x08)
    }

    // ---- TRACKs (3 × 6 bytes, offset 98) --- base IDs 230,240,250 ----------
    const int trkBase[3] = {230,240,250};
    for (int i = 0; i < 3; ++i) {
        int off  = 98 + i * 6;
        int base = trkBase[i];
        for (int j = 0; j < 6; ++j) p[base+j] = r[off+j];  // input, pt1..5
    }

    // ---- RAMPs (4 × 3 bytes, offset 116) --- base IDs 260,270,280,290 ------
    // Struct order: rate, flags, lfotrig
    // Flag bits: GATED=0x01, LFOTRIG=0x02, EXTRIG=0x04, MULTI=0x08
    const int rmpBase[4] = {260,270,280,290};
    for (int i = 0; i < 4; ++i) {
        int off  = 116 + i * 3;
        int base = rmpBase[i];
        p[base+0] = r[off+0];           // rate
        uint8_t fl = r[off+1];
        p[base+1] = bit(fl,3);          // TRIG_MULTI  (RAMPF_MULTI=0x08)
        p[base+2] = bit(fl,2);          // TRIG_EXT    (RAMPF_EXTRIG=0x04)
        p[base+3] = bit(fl,1);          // TRIG_LFO    (RAMPF_LFOTRIG=0x02)
        p[base+4] = r[off+2];           // TRIG_LFOSRC
        p[base+5] = bit(fl,0);          // TRIG_GATED  (RAMPF_GATED=0x01)
    }

    return p;
}

// Inverse of decode — packs a {paramID → value} map back into 196 raw binary bytes.
// Layout mirrors XPanderSinglePatch.ToStream (C# reference).
inline std::vector<uint8_t> encode(const std::map<int,int>& p)
{
    // Safe lookup: returns 0 for unknown/uninitialised params (-999 sentinel)
    auto get = [&](int id) -> uint8_t {
        auto it = p.find(id);
        if (it == p.end() || it->second == -999) return 0;
        return (uint8_t)(it->second & 0xFF);
    };

    std::vector<uint8_t> r(196, 0);

    // ---- VCOs (2 × 6 bytes, offsets 0 and 6) --------------------------------
    const int vcoBase[2] = {0, 20};
    for (int i = 0; i < 2; ++i)
    {
        int off  = i * 6;
        int base = vcoBase[i];

        r[off+0] = get(base+0);                         // freq
        int det  = p.count(base+1) ? p.at(base+1) : 0;
        r[off+1] = (det < 0) ? (uint8_t)(det + 256) : (uint8_t)det;  // detune signed→uint8
        r[off+2] = get(base+2);                         // pw
        r[off+3] = get(base+3);                         // vol

        if (i == 0)
        {
            // VCO1: mod flags at base+5..8; wave bitmask = TRI/SAW/PULSE
            r[off+4] = ((get(base+5) & 1) << 0)        // KEYB
                     | ((get(base+6) & 1) << 1)        // LAG
                     | ((get(base+7) & 1) << 2)        // LEV1
                     | ((get(base+8) & 1) << 3);       // VIB
            r[off+5] = get(base+4) & 0x07;             // TRI/SAW/PULSE → bits 0-2
        }
        else
        {
            // VCO2: KEYB=base+6, LAG=base+7, LEV1=base+8, VIB=base+9
            r[off+4] = ((get(base+6) & 1) << 0)        // KEYB (MODFLAG_KEYBD=bit0)
                     | ((get(base+7) & 1) << 1)        // LAG  (MODFLAG_LAG=bit1)
                     | ((get(base+8) & 1) << 2)        // LEV1 (MODFLAG_LEV_1=bit2)
                     | ((get(base+9) & 1) << 3);       // VIB  (MODFLAG_VIB=bit3)
            // wave byte: TRI/SAW/PULSE from bitmask bits 0-2, SYNC=base+5, NOISE from bitmask bit3→bit4
            r[off+5] = (get(base+4) & 0x07)            // TRI/SAW/PULSE
                     | ((get(base+5) & 1) << 3)        // SYNC
                     | (((get(base+4) >> 3) & 1) << 4); // NOISE (bitmask bit3 → hardware bit4)
        }
    }

    // ---- VCF (6 bytes, offset 12) -------------------------------------------
    r[12] = get(40);  r[13] = get(41);  r[14] = get(42);
    r[15] = get(43);  r[16] = get(44);
    r[17] = ((get(45) & 1) << 0) | ((get(46) & 1) << 1)
           | ((get(47) & 1) << 2) | ((get(48) & 1) << 3);

    // ---- FM/LAG (5 bytes, offset 18) ----------------------------------------
    r[18] = get(60);  r[19] = get(61);  r[20] = get(62);  r[21] = get(63);
    r[22] = ((get(64) & 1) << 0) | ((get(65) & 1) << 1) | ((get(66) & 1) << 2);

    // ---- LFOs (5 × 7 bytes, offset 23) — binary order: speed,retrig_mode,lag,wave,retrig,sample,amp
    const int lfoBase[5] = {70, 80, 90, 100, 110};
    for (int i = 0; i < 5; ++i)
    {
        int off  = 23 + i * 7;
        int base = lfoBase[i];
        r[off+0] = get(base+0);  // speed
        r[off+1] = get(base+6);  // retrig_mode
        r[off+2] = get(base+5);  // lag
        r[off+3] = get(base+1);  // wave
        r[off+4] = get(base+3);  // retrig point
        r[off+5] = get(base+2);  // sample_in
        r[off+6] = get(base+4);  // amp
    }

    // ---- ENVs (5 × 8 bytes, offset 58) — flags: RESET=0x01,MULTI=0x04,GATED=0x08,
    //      EXTRIG=0x10,LFOTRIG=0x20,DADR=0x40,FREERUN=0x80
    const int envBase[5] = {130, 150, 170, 190, 210};
    for (int i = 0; i < 5; ++i)
    {
        int off  = 58 + i * 8;
        int base = envBase[i];
        uint8_t fl = ((get(base+6)  & 1) << 0)   // RESET
                   | ((get(base+9)  & 1) << 2)   // MULTI
                   | ((get(base+13) & 1) << 3)   // GATED
                   | ((get(base+10) & 1) << 4)   // EXTRIG
                   | ((get(base+11) & 1) << 5)   // LFOTRIG
                   | ((get(base+8)  & 1) << 6)   // DADR
                   | ((get(base+7)  & 1) << 7);  // FREERUN
        r[off+0] = fl;
        r[off+1] = (get(base+11) & 1) ? get(base+12) : 0;  // LFOSRC (only when LFOTRIG set)
        r[off+2] = get(base+0);   // delay
        r[off+3] = get(base+1);   // attack
        r[off+4] = get(base+2);   // decay
        r[off+5] = get(base+3);   // sustain
        r[off+6] = get(base+4);   // release
        r[off+7] = get(base+5);   // amp
    }

    // ---- TRACKs (3 × 6 bytes, offset 98) — trivial: direct copy ---------------
    const int trkBase[3] = {230, 240, 250};
    for (int i = 0; i < 3; ++i)
    {
        int off  = 98 + i * 6;
        int base = trkBase[i];
        for (int j = 0; j < 6; ++j) r[off+j] = get(base+j);
    }

    // ---- RAMPs (4 × 3 bytes, offset 116) — flags: GATED=0x01,LFOTRIG=0x02,EXTRIG=0x04,MULTI=0x08
    const int rmpBase[4] = {260, 270, 280, 290};
    for (int i = 0; i < 4; ++i)
    {
        int off  = 116 + i * 3;
        int base = rmpBase[i];
        r[off+0] = get(base+0);   // rate
        r[off+1] = ((get(base+5) & 1) << 0)   // GATED
                 | ((get(base+3) & 1) << 1)   // LFOTRIG
                 | ((get(base+2) & 1) << 2)   // EXTRIG
                 | ((get(base+1) & 1) << 3);  // MULTI
        r[off+2] = get(base+4);   // LFOSRC
    }

    return r;
}

} // namespace PatchCodec
