#pragma once

#include "flock_parser.h"
#include "swiz_flock_hunter.h"

View* flock_view_alloc(SwizApp* app);
void  flock_view_free(View* v);
void  flock_view_apply_msg(View* v, SwizApp* app, const SwizMsg* msg, uint32_t now_tick);
void  flock_view_tick(View* v, uint32_t now_tick);
void  flock_view_set_band(View* v, SwizBand band);
// Wipe per-session state (latest, seen[], counters, baselines) so the next
// scan starts with a clean dashboard. Called when leaving the dashboard back
// to the band picker.
void  flock_view_reset(View* v);
