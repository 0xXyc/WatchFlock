#include "flock_oui.h"

#include <string.h>
#include <strings.h>

typedef struct {
    const char* prefix;
    const char* vendor;
} OuiEntry;

// High-confidence and contract-mfr Flock OUIs. Same ordering as the
// firmware lists. e4:aa:ea is the Liteon prefix that field-confirmed a
// Falcon V2 in St. Pete, so it gets a more specific label.
static const OuiEntry kTable[] = {
    {"b4:1e:52", "Flock Safety"},
    {"58:8e:81", "Flock"},
    {"cc:cc:cc", "Flock"},
    {"ec:1b:bd", "Flock"},
    {"90:35:ea", "Flock"},
    {"04:0d:84", "Flock"},
    {"f0:82:c0", "Flock"},
    {"1c:34:f1", "Flock"},
    {"38:5b:44", "Flock"},
    {"94:34:69", "Flock"},
    {"b4:e3:f9", "Flock"},
    {"70:c9:4e", "Flock"},
    {"3c:91:80", "Flock"},
    {"d8:f3:bc", "Flock"},
    {"80:30:49", "Flock"},
    {"14:5a:fc", "Flock"},
    {"74:4c:a1", "Flock"},
    {"08:3a:88", "Flock"},
    {"9c:2f:9d", "Flock"},
    {"94:08:53", "Flock"},
    {"e4:aa:ea", "Liteon (Flock)"},
    {"f4:6a:dd", "Liteon"},
    {"f8:a2:d6", "Liteon"},
    {"e0:0a:f6", "Liteon"},
    {"00:f4:8d", "Liteon"},
    {"d0:39:57", "USI"},
    {"e8:d0:fc", "USI"},
    {"d4:11:d6", "SoundThinking"},
    // Community-observed prefixes (DeFlock). The firmware reports these at
    // MEDIUM confidence (rule=oui_flock_likely), so the label says "likely" --
    // a HIGH and a MEDIUM hit must not read identically on the badge.
    {"b8:35:32", "Flock (likely)"},
    {"c0:35:32", "Flock (likely)"},
    {"24:b2:b9", "Flock (likely)"},
    {"e0:4f:43", "Flock (likely)"},
    {"b8:1e:a4", "Flock (likely)"},
    {"70:08:94", "Flock (likely)"},
    {"3c:71:bf", "Flock (likely)"},
    {"58:00:e3", "Flock (likely)"},
    {"5c:93:a2", "Flock (likely)"},
    {"64:6e:69", "Flock (likely)"},
    {"48:27:ea", "Flock (likely)"},
    {"a4:cf:12", "Flock (likely)"},
    // Contributed by Michael / DeFlockJoplin, attributed to a Raven acoustic
    // sensor. Locally administered (bit 1 of the first byte is set), so it is
    // not an IEEE-registered vendor prefix and will never appear in an OUI
    // registry — the lookup here is the only thing that can name it.
    {"82:6b:f2", "Flock Raven? (likely)"},
};

void flock_oui_lookup(const char* oui, char* out, size_t out_sz) {
    if (out_sz == 0) return;
    out[0] = '\0';
    if (!oui) {
        strncpy(out, "unknown", out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }
    for (size_t i = 0; i < sizeof(kTable) / sizeof(kTable[0]); i++) {
        if (strncasecmp(oui, kTable[i].prefix, 8) == 0) {
            strncpy(out, kTable[i].vendor, out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
    }
    strncpy(out, "unknown", out_sz - 1);
    out[out_sz - 1] = '\0';
}
