#include "content/palette.h"

#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf {

namespace {
const char kPalettesJson[] =
#include "palettes.json.inc"
    ;

bool colour(const JsonDoc& doc, const JsonValue* entry, Vec3& out, const char* what) {
    float c[3];
    if (doc.numbers("linear", c, 3, entry) != 3) {
        RF_ERR("palettes: no linear colour for %s", what);
        return false;
    }
    out = {c[0], c[1], c[2]};
    return true;
}
}  // namespace

const Palette* Palettes::find(const char* id) const {
    for (u32 i = 0; i < count; ++i)
        if (std::strcmp(list[i].id, id) == 0) return &list[i];
    return nullptr;
}

bool loadPalettes(Palettes& out) {
    JsonDoc doc;
    if (!doc.parse(kPalettesJson, sizeof kPalettesJson - 1, "design/palettes.json")) return false;
    bool ok = colour(doc, doc.find("constants.danger"), out.danger, "danger");
    ok = colour(doc, doc.find("constants.pickup"), out.pickup, "pickup") && ok;
    ok = colour(doc, doc.find("constants.white"), out.white, "white") && ok;
    const JsonValue* players = doc.find("constants.players");
    u32 n = 0;
    for (const JsonValue* p = doc.firstChild(players); p && n < 4; p = doc.nextSibling(p), ++n) {
        ok = colour(doc, p, out.player[n], "a player") && ok;
        ok = colour(doc, doc.member(p, "deep"), out.playerDeep[n], "a player's deep colour") && ok;
        ok = colour(doc, doc.member(p, "light"), out.playerLight[n], "a player's light colour") && ok;
    }
    if (n != 4) {
        RF_ERR("palettes: expected four players, found %u", n);
        ok = false;
    }

    out.count = 0;
    const JsonValue* list = doc.find("palettes");
    for (const JsonValue* e = doc.firstChild(list); e && out.count < Palettes::kMax; e = doc.nextSibling(e)) {
        Palette& p = out.list[out.count++];
        doc.text("id", p.id, sizeof p.id, e);
        doc.text("name", p.name, sizeof p.name, e);
        p.track = static_cast<u32>(doc.number("track", 0.0, nullptr, e));
        ok = colour(doc, doc.member(e, "bg0"), p.bg0, "bg0") && ok;
        ok = colour(doc, doc.member(e, "bg1"), p.bg1, "bg1") && ok;
        ok = colour(doc, doc.member(e, "grid"), p.grid, "grid") && ok;
        ok = colour(doc, doc.member(e, "gridHot"), p.gridHot, "gridHot") && ok;
        ok = colour(doc, doc.member(e, "accent"), p.accent, "accent") && ok;
        float solo[3];
        if (doc.numbers("soloEnemies", solo, 3, e) != 3) {
            RF_ERR("palettes: %s has no soloEnemies", p.id);
            ok = false;
            continue;
        }
        const JsonValue* candidates = doc.member(e, "enemyCandidates");
        for (u32 k = 0; k < 3; ++k) {
            const JsonValue* c = doc.at(candidates, static_cast<u32>(solo[k]));
            if (!c) {
                RF_ERR("palettes: %s soloEnemies[%u] out of range", p.id, k);
                ok = false;
                continue;
            }
            ok = colour(doc, c, p.hue[k], "an enemy hue") && ok;
            const u32 which = static_cast<u32>(doc.number("soloPartner", 0.0, nullptr, c));
            ok = colour(doc, doc.at(doc.member(c, "partners"), which), p.partner[k], "a partner") && ok;
        }
    }
    if (out.count == 0) {
        RF_ERR("palettes: none found");
        ok = false;
    }
    if (ok) RF_LOG("palettes: %u (%s ... %s)", out.count, out.list[0].id, out.list[out.count - 1].id);
    return ok;
}

}  // namespace rf
