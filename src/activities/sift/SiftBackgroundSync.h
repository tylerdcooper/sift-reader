#pragma once

namespace sift {

// Background sync of the read-later queue. Two triggers:
//   - syncOnWake(): fire once on boot/wake — connect Wi-Fi, sync, disconnect.
//     So new articles are already there before you open Sift.
//   - requestSync()/endSyncSession(): while Sift is open — sync if due and keep
//     Wi-Fi up (so mark-read works), then tear it down on exit.
// backgroundTick() (from the main loop) drives the connect→sync state machine on
// a task — no UI, no connect screen. (The X4 Pro has no USB-power pin, so we
// can't gate on charging.)
void syncOnWake();
void requestSync();
void endSyncSession();
void backgroundTick();

// True while a connect or sync is in progress (so the Sift list can show
// "Sync in progress").
bool bgBusy();

}  // namespace sift
