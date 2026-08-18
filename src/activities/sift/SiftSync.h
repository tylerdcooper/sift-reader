#pragma once

namespace sift {
namespace sync {

// Pull the read-later queue to the on-SD cache: the list, each article's blocks,
// and every inline image. Removes local articles that dropped off the queue.
// Silent (no UI). Returns the number of queued articles, or -1 if unreachable.
int syncQueue();

}  // namespace sync
}  // namespace sift
