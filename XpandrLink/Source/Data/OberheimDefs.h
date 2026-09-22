/*
  OberheimDefs.h
  Timing and slot constants for the Oberheim Xpander/Matrix-12 SysEx protocol.

  Message formats and command bytes are documented where they are built/parsed
  (MidiEngine.cpp) and in SPEC.md §4 — the wire layout reads top-to-bottom at
  the send/receive site, so no named byte constants are kept here.
*/
#pragma once

namespace Oberheim
{
    // Minimum settle times (ms) between outgoing SysEx messages.
    // The vintage CPU needs time to update state after a page-select before it can accept a param.
    constexpr int kButtonPageSettleMs = 150;  // page-select → button-subpage parameter
    constexpr int kSliderPageSettleMs = 100;  // page-select → slider/knob parameter
    constexpr int kModCmdGapMs        =  50;  // between mod-matrix sub-commands
    constexpr int kCcSendThrottleMs   = 150;  // shared floor between any two CC-driven
                                               // parameter sends -- must be >= the largest
                                               // page-select settle above (kButtonPageSettleMs),
                                               // since two CCs mapped to different pages each
                                               // require a full page-select + settle on every
                                               // alternation between them (kModCmdGapMs is too
                                               // short: it paces sub-commands within a single
                                               // mod-matrix edit, which need no page-select at all)
    constexpr int kPatchSendSettleMs  = 100;  // patch-dump SysEx → program-change
    constexpr int kDisplaySettleMs    = 100;  // display OFF -> ON -> text message
    constexpr int kDisplayScrollMs    =  50;  // typewriter: delay between growing substrings

    // Dedicated "scratchpad" program slot used when pushing a library/editor-loaded
    // patch to hardware. Loads target this single slot instead of the patch's own
    // stored program number, so the user's other patches in synth memory are never
    // overwritten by an audition/load. Matrix-12 banks are 0..99; 99 is the last slot.
    constexpr int kScratchpadProgram = 99;
}
