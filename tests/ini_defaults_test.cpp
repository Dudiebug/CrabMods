// Bringing a player's CrabMods.ini up to new defaults (crabmods/ini_defaults.hpp): settings still at their old
// default move to the new one, settings the player changed stay, and untouched sections get the new comments.
#include <cstdio>

#include "../crabmods/defaults_0_14_1.hpp"
#include "../crabmods/ini_defaults.hpp"

using namespace crabmods;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

static const char kWas[] = "[underdog]\nenabled=true\nrarity=epic\ndamagePercent=0.5\n"
                           "[untouchable]\nenabled=true\ndamagePercent=1\nmaxPercent=20\n"
                           "[grappler]\nenabled=true\nrarity=legendary\n";
static const char kNow[] = "[underdog]\nenabled=true\nrarity=epic\ndamagePercent=1\n"
                           "[untouchable]\nenabled=true\ndamagePercent=2\nmaxPercent=40\n"
                           "[grappler]\nenabled=true\nrarity=epic\n";

static std::string Section(const std::string& s) {
    if (s == "underdog") return "# Underdog: 1 gives +50% at half health.\nenabled = true\nrarity = epic\ndamagePercent = 1\n";
    return "";
}

static DefaultsUpdate Run(const std::string& ini) { return UpdateDefaults(ini, ParseValues(kWas), ParseValues(kNow), Section); }

int main() {
    // Nothing the player changed: the whole section is rewritten, comments included, and the blank line kept.
    {
        DefaultsUpdate u = Run("# header\n\n[underdog]\n# Underdog: 0.5 gives +25% at half health.\nenabled = true\n"
                               "rarity = epic\ndamagePercent = 0.5\n\n[grappler]\nenabled = true\nrarity = Legendary\n");
        CHECK(u.text == "# header\n\n[underdog]\n# Underdog: 1 gives +50% at half health.\nenabled = true\nrarity = epic\n"
                        "damagePercent = 1\n\n[grappler]\nenabled = true\nrarity = epic\n");
        CHECK(u.changed.size() == 2);
        CHECK(u.changed[0] == "underdog.damagePercent 0.5 -> 1");
        CHECK(u.changed[1] == "grappler.rarity Legendary -> epic");  // no section text: just the line
    }
    // A changed setting stays; the others in its section still move, line by line, keeping comments after values.
    {
        DefaultsUpdate u = Run("[untouchable]\r\nenabled = true\r\ndamagePercent = 1.0   # per second\r\nmaxPercent = 25\r\n");
        CHECK(u.text == "[untouchable]\r\nenabled = true\r\ndamagePercent = 2 # per second\r\nmaxPercent = 25\r\n");
        CHECK(u.changed.size() == 1);
    }
    // A player who disabled the item keeps it disabled, and the section isn't rewritten.
    {
        DefaultsUpdate u = Run("[underdog]\n# old comment\nenabled = false\ndamagePercent = 0.5\n");
        CHECK(u.text == "[underdog]\n# old comment\nenabled = false\ndamagePercent = 1\n");
    }
    // Already at the new defaults, or a key the old defaults don't know: nothing changes.
    {
        std::string ini = "[underdog]\ndamagePercent = 1\n[untouchable]\nnewKey = 3\nmaxPercent = 20.0001\n";
        DefaultsUpdate u = Run(ini);
        CHECK(u.text == ini);
        CHECK(u.changed.empty());
    }
    // Keys before any section are [arsenal]'s (the old Arsenal.ini).
    {
        IniValues v = ParseValues("a = 1 # c\n[x]\nb=2\n");
        CHECK(v.at({"arsenal", "a"}) == "1");
        CHECK(v.at({"x", "b"}) == "2");
    }
    CHECK(SameValue("10", "10.0"));
    CHECK(SameValue("Legendary", "legendary"));
    CHECK(!SameValue("0.25", "1"));
    CHECK(!SameValue("", "0"));
    // The 0.14.1 snapshot has the defaults this update changes.
    {
        IniValues old = ParseValues(kDefaults_0_14_1);
        CHECK(old.at({"serratedLinks", "damagePercent"}) == "50");
        CHECK(old.at({"grappler", "rarity"}) == "legendary");
        CHECK(old.at({"soulSiphon", "souls"}) == "1");
    }
    std::printf(g_failures ? "ini_defaults_test: %d FAILED\n" : "ini_defaults_test: ok\n", g_failures);
    return g_failures ? 1 : 0;
}
