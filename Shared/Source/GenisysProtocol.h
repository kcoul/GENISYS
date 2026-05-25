#pragma once
#include <juce_osc/juce_osc.h>

// OSC address space for Frontend <-> Backend communication.
// All addresses are under /genisys/ to avoid collisions with SurgeXT's /param/ space.

namespace GenisysProtocol
{
    // Frontend -> Backend
    static constexpr auto studioLoad     = "/genisys/studio/load";     // string: profile JSON
    static constexpr auto studioSave     = "/genisys/studio/save";     // string: profile JSON
    static constexpr auto commandSend    = "/genisys/command";         // string: destination, string: intent

    // Backend -> Frontend
    static constexpr auto studioState    = "/genisys/studio/state";    // string: profile JSON (current state)
    static constexpr auto deviceList     = "/genisys/devices";         // string: JSON array of discovered devices
    static constexpr auto ack            = "/genisys/ack";             // string: address being acknowledged
    static constexpr auto error          = "/genisys/error";           // string: human-readable error

    // Backend -> Diagnostic client (continuous stream, always on)
    static constexpr auto diagTranscript = "/genisys/diag/transcript"; // string: text, int: latency_ms
    static constexpr auto diagVadStart   = "/genisys/diag/vad_start";  // float: speech_probability
    static constexpr auto diagVadEnd     = "/genisys/diag/vad_end";    // int: utterance_samples
    static constexpr auto diagMatch      = "/genisys/diag/match";      // string: transcript, string: command_id (empty = no match)
    static constexpr auto diagStats      = "/genisys/diag/stats";      // int: total, int: matched, float: avg_latency_ms

    // Ports
    static constexpr int backendPort  = 54280;  // Backend listens here
    static constexpr int frontendPort = 54281;  // Frontend listens here (backend replies)
    static constexpr int diagPort     = 54282;  // DebugConsole listens here
}
