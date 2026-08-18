#pragma once

namespace sift {

// Call every main-loop iteration. When the reader is on USB power and Sift is
// configured, it connects Wi-Fi to the saved network and pulls the read-later
// queue on a background task — no UI, so opening Sift shows the already-synced
// list instead of a "Connecting…" screen. Re-syncs periodically while docked
// and tears Wi-Fi down when unplugged.
void backgroundTick();

// True while a background connect or sync is in progress (so the Sift screen can
// say "Sync in progress" if you open it before the first sync finishes).
bool bgBusy();

}  // namespace sift
