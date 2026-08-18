#pragma once

namespace sift {

// Call once per main-loop iteration. When the reader is docked (USB power) and
// Sift is configured, it kicks a one-shot background sync on its own task
// (network + SD only — no UI), so the catalog is pulled for offline without any
// user action. Throttled to re-sync periodically while it stays docked, and it
// re-arms whenever the device is undocked and docked again.
void backgroundSyncTick();

}  // namespace sift
