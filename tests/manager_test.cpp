// Manager tests: versions, the releases list, share codes and ini merging (manager/core.hpp).
#include <cstdio>

#include "../manager/core.hpp"

using namespace mgr;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

static void Versions() {
    CHECK(CompareVersions("0.3.0", "0.10.0") < 0);
    CHECK(CompareVersions("1.0", "1.0.0") == 0);
    CHECK(CompareVersions("1.0.1", "1.0.0") > 0);
    CHECK(CompareVersions("2.0.0-beta", "2.0.0") == 0);
}

static void Releases() {
    const char* json = R"([
      {"tag_name":"crabmods-v0.3.0","draft":false,"prerelease":false,"assets":[
        {"name":"CrabMods.dll","browser_download_url":"https:\/\/x\/m030.dll"},
        {"name":"xinput1_3.dll","browser_download_url":"https://x/l.dll"}]},
      {"tag_name":"crabmods-v0.4.0","draft":true,"prerelease":false,"assets":[
        {"name":"CrabMods.dll","browser_download_url":"https://x/draft.dll"}]},
      {"tag_name":"crabmods-v0.10.0","draft":false,"prerelease":true,"assets":[
        {"name":"CrabMods.dll","browser_download_url":"https://x/pre.dll"}]},
      {"tag_name":"crabmods-v0.2.0","assets":[{"name":"CrabMods-0.2.0.zip","browser_download_url":"https://x/z"}]},
      {"tag_name":"crabsync-v1.0.1","assets":[{"name":"CrabSync.dll","browser_download_url":"https://x/s.dll"}],
       "body":"Fixes \"quotes\" and \u00e9 \ud83e\udd80"},
      {"tag_name":"manager-v0.1.0","assets":[]}
    ])";
    auto latest = LatestReleases(json);
    CHECK(latest.size() == 2);
    CHECK(latest["CrabMods"].version == "0.3.0");
    CHECK(latest["CrabMods"].assets["CrabMods.dll"] == "https://x/m030.dll");
    CHECK(latest["CrabMods"].assets["xinput1_3.dll"] == "https://x/l.dll");
    CHECK(latest["CrabSync"].version == "1.0.1");
    CHECK(LatestReleases("not json").empty());
    CHECK(LatestReleases("{\"message\":\"API rate limit exceeded\"}").empty());
    auto j = JsonParser(R"({"s":"\u00e9\ud83e\udd80"})").Parse();
    CHECK(j && j->Str("s") == "\xc3\xa9\xf0\x9f\xa6\x80");
}

static const char* kIni = R"(# CrabMods: new items.

[arsenal]
# damage
enabled = true
damagePerModPercent = 3  # per mod
rarity = epic

[hoarder]
enabled = true
damagePercent = 3

[minimalist]
enabled = false
)";

static void ShareCodes() {
    std::string code = MakeShareCode("CrabMods.ini", kIni);
    CHECK(code.rfind("CRAB1-", 0) == 0);
    auto s = ReadShareCode(code);
    CHECK(s && s->iniName == "CrabMods.ini");
    CHECK(s && s->settings == CompactIni(kIni));
    CHECK(s && s->settings.find('#') == std::string::npos);
    // Pasted with line breaks and spaces.
    std::string wrapped = "  " + code.substr(0, 20) + "\r\n" + code.substr(20) + "\n";
    CHECK(ReadShareCode(wrapped).has_value());
    // Damaged: one character changed, cut short, wrong prefix.
    std::string bad = code;
    bad[10] = bad[10] == 'A' ? 'B' : 'A';
    CHECK(!ReadShareCode(bad));
    CHECK(!ReadShareCode(code.substr(0, code.size() - 3)));
    CHECK(!ReadShareCode("CRAB2-" + code.substr(6)));
    CHECK(!ReadShareCode("CRAB1-***"));
    for (std::string t : {"", "a", "ab", "abc", "abcd"}) CHECK(FromBase64Url(Base64Url(t)) == t);
    CHECK(Crc32("123456789") == 0xCBF43926);
}

static void Merge() {
    std::string incoming = "[arsenal]\ndamagePerModPercent = 5\nnewKey = 1\n[minimalist]\nenabled = true\n"
                           "[jackOfAllTrades]\nenabled = false\n";
    std::string m = MergeIni(kIni, incoming);
    IniValues v = ReadIni(m);
    CHECK(v["arsenal"]["damagePerModPercent"] == "5");
    CHECK(v["arsenal"]["newKey"] == "1");
    CHECK(v["arsenal"]["rarity"] == "epic");
    CHECK(v["hoarder"]["damagePercent"] == "3");
    CHECK(v["minimalist"]["enabled"] == "true");
    CHECK(v["jackOfAllTrades"]["enabled"] == "false");
    CHECK(m.find("# damage\n") != std::string::npos);                       // comments kept
    CHECK(m.find("damagePerModPercent = 5  # per mod") != std::string::npos);  // inline comment kept
    // The new key goes inside [arsenal], before the blank line ending it.
    CHECK(m.find("newKey = 1") < m.find("[hoarder]"));
    CHECK(MergeIni(kIni, CompactIni(kIni)) == std::string(kIni));
}

static void Libraries() {
    const char* vdf = R"("libraryfolders"
{
	"0"
	{
		"path"		"/home/me/.local/share/Steam"
		"apps" { "774801" "1" }
	}
	"1"
	{
		"path"		"D:\\SteamLibrary"
	}
})";
    auto libs = SteamLibraries(vdf);
    CHECK(libs.size() == 2);
    CHECK(libs.size() == 2 && libs[0] == "/home/me/.local/share/Steam");
    CHECK(libs.size() == 2 && libs[1] == "D:\\SteamLibrary");
}

int main() {
    Versions();
    Releases();
    ShareCodes();
    Merge();
    Libraries();
    std::printf(g_failures ? "manager tests: %d FAILED\n" : "manager tests: all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
