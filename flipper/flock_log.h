// Persistent CSV logger for SwizFlockHunter HITs.
// Opens /ext/apps_data/swiz_flock_hunter/hits.csv on first call, appends a row
// per HIT received. Survives app exit so geotagged detections accumulate.

#pragma once

#include "swiz_flock_hunter.h"

void flock_log_init(void);
void flock_log_hit(const SwizHit* h);
void flock_log_deinit(void);
