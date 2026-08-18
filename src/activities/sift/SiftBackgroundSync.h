#pragma once

namespace sift {

// Sync session driven by opening Sift (the X4 Pro has no USB-power-detect pin,
// so we can't gate on charging). SiftFeeds calls requestSync() on enter and
// endSyncSession() on exit; backgroundTick() (from the main loop) connects the
// saved Wi-Fi and pulls the queue on a task — no UI, no connect screen. Wi-Fi is
// only up while you're in Sift, then torn down.
void requestSync();
void endSyncSession();
void backgroundTick();

// True while a connect or sync is in progress (so the Sift list can show
// "Sync in progress").
bool bgBusy();

}  // namespace sift
