// Manager tests, on Windows and Linux: versions, releases, share codes, ini merging, crash reports, hashes, the
// version reader, update decisions, sorting, and installing into a scratch game folder (manager/core.hpp,
// manager/files.hpp).
// Usage: manager_core_test [build folder]  (with the built CrabMods.dll and the catalogs for the checks on real
// files; they are skipped without them)
#include <cstdio>

#include "../manager/files.hpp"

using namespace mgr;

static int g_failures = 0;
static fs::path g_build;
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
      {"tag_name":"manager-v0.1.0","assets":[]},
      {"tag_name":"manager-v0.3.0","assets":[{"name":"CrabModsManager.exe","browser_download_url":"https://x/m.exe"}]}
    ])";
    auto latest = LatestReleases(json);
    CHECK(latest.size() == 3 && latest["Manager"].version == "0.3.0");
    CHECK(latest["CrabMods"].version == "0.3.0");
    CHECK(latest["CrabMods"].assets["CrabMods.dll"].url == "https://x/m030.dll");
    CHECK(latest["CrabMods"].assets["xinput1_3.dll"].url == "https://x/l.dll");
    CHECK(latest["CrabSync"].version == "1.0.1");
    CHECK(LatestReleases("not json").empty());
    CHECK(LatestReleases("{\"message\":\"API rate limit exceeded\"}").empty());
    CHECK(!ParseReleases("{\"message\":\"x\"}").ok && ParseReleases("[]").ok);
    // No AppImage release yet: nothing for Linux.
    CHECK(!LatestReleases(json, kManagerAppImage).count("Manager"));
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

static void Crashes() {
    CHECK(ScrubHomes("C:\\Users\\Dudie Bug\\AppData\\x.log") == "C:\\Users\\<user>\\AppData\\x.log");
    CHECK(ScrubHomes("Z:\\home\\dudie\\.local/share") == "Z:\\home\\<user>\\.local/share");
    CHECK(ScrubHomes("/home/dudie") == "/home/<user>");
    CHECK(ScrubHomes("C:/users/steamuser/AppData") == "C:/users/steamuser/AppData");
    CHECK(ScrubHomes("my home is nice, /homes/x") == "my home is nice, /homes/x");
    CHECK(ReplaceNoCase("DESKTOP-ABC and desktop-abc", "Desktop-Abc", "<user>") == "<user> and <user>");
    CHECK(ReplaceNoCase("a b", "a", "x") == "a b");  // too short to be worth hiding

    const char* sync = "[23:09:20.804] hook UGameEngine::Tick @ 00000001422EEFB0: MH_OK\n"
                       "[23:10:23.483] Bob: gained ability mod x1 DA_AbilityMod_ToolBelt\n"
                       "[23:10:24.000]   Bobby Tables: health=1/2 (x) base max=3\n"
                       "[23:10:25.000] back in the lobby: team inventory cleared\n"
                       "[23:10:26.000] the host runs CrabSync 1.0.4: ok\n"
                       "[23:10:27.000] Bob: slots 1 +5\n";
    auto names = PlayerNames(sync);
    CHECK(names.size() == 2 && names[0] == "Bob" && names[1] == "Bobby Tables");
    std::string s = Scrub("Bobby Tables met Bob at C:\\Users\\Dudie\\x on PC-7", {"PC-7"}, names);
    CHECK(s == "Player 2 met Player 1 at C:\\Users\\<user>\\x on <user>");

    const char* xml =
        "<ErrorMessage>Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0xff</ErrorMessage>\n"
        "<SecondsSinceStart>1397</SecondsSinceStart>\n<MachineId>AFADD9FF</MachineId>\n"
        "<LoginId>afadd9ff</LoginId>\n<Misc.OSVersionMajor>Windows 10</Misc.OSVersionMajor>\n"
        "<PCallStack>\nCrabSync                      0x00006ffffb540000 + 1b10            \n"
        "CrabChampions-Win64-Shipping  0x0000000140000000 + 78a4e0          \n</PCallStack>";
    std::string clean = StripCrashIds(xml);
    CHECK(clean.find("AFADD9FF") == std::string::npos && clean.find("afadd9ff") == std::string::npos);
    CHECK(clean.find("<MachineId></MachineId>") != std::string::npos);
    CrashReport r;
    ReadCrashContext(r, clean);
    CHECK(r.crashed && r.secondsIn == 1397 && r.os == "Windows 10");
    CHECK(r.error == "EXCEPTION_ACCESS_VIOLATION reading address 0xff");
    CHECK(r.callStack == "CrabSync+0x1b10\nCrabChampions-Win64-Shipping+0x78a4e0\n");
    CHECK(IssueTitle(r) == "Crash: EXCEPTION_ACCESS_VIOLATION reading address 0xff at CrabSync+0x1b10");

    CHECK(LastLines("a\nb\nc\n", 2) == "b\nc");
    CHECK(LastLines("a\nb\nc", 5) == "a\nb\nc");
    CHECK(LastLines("a\nb", 0).empty());
    CHECK(LastLines("only", 1) == "only");
    CHECK(UrlEncode("a b&c/\xc3\xa9~") == "a%20b%26c%2F%C3%A9~");

    r.versions = "CrabMods 0.8.0";
    r.platform = "Linux (Proton)";
    r.zipName = "CrabCrash.zip";
    for (int i = 0; i < 2000; i++) r.crabModsLog += "[00:00:00.000] a fairly long log line number " + std::to_string(i) + "\r\n";
    r.crabSyncLog = "x ``` y\n";
    std::string body = IssueBody(r, 3);
    CHECK(body.find("number 1999\n") != std::string::npos && body.find("number 1997\n") != std::string::npos);
    CHECK(body.find("number 1996") == std::string::npos && body.find('\r') == std::string::npos);
    CHECK(body.find("x ''' y") != std::string::npos && body.find("CrabCrash.zip") != std::string::npos);
    std::string url = IssueUrl(r);
    CHECK(url.rfind("https://github.com/Dudiebug/CrabMods/issues/new?title=Crash%3A%20", 0) == 0);
    CHECK(url.size() <= 7500 && url.find("number%201999") != std::string::npos);

    std::string zip = StoreZip({{"a.txt", "hello"}, {"b/c.log", ""}});
    CHECK(zip.compare(0, 4, "PK\x03\x04") == 0 && zip.find("hello") != std::string::npos);
    CHECK(zip.size() == 30 + 5 + 5 + 30 + 7 + 46 + 5 + 46 + 7 + 22);
    CHECK(zip.compare(zip.size() - 22, 4, "PK\x05\x06") == 0);
}

static const char* kSyncIni = R"(# CrabSync: shared team inventory. Only the host's settings matter; clients need nothing.
enabled = true

# Equipment: the most recent change by anyone is applied to everyone.
syncWeapon = true
syncAbility = true

# Max health added to the pool for each player after the first (2 players = your max + 250).
healthPerPlayer = 250
# Chests that cost health or max health (spiked and greed chests):
#   split = each player's chest costs the team a share
#   full  = the team pays every chest's whole cost
#   free  = they cost the team nothing
chestHealthCost = split
pollSeconds = 0.1
)";

static const char* kCatalogJson = R"({"header":"# CrabMods\n","items":[
  {"section":"hoarder","name":"Hoarder","kind":"Perk","rarity":"legendary","ini":"# Hoarder, a perk: more damage for each weapon mod.\nenabled = true\n\n# Damage bonus, in percent.\ndamagePercent = 3\n\n# Which chests it drops from: common, epic, legendary or greed.\nrarity = legendary\n"},
  {"section":"keyTotem","name":"Key Totem Menu","kind":"Tweak","rarity":"","ini":"# Key totem: choose any item. A locked item\n# you choose is unlocked.\nenabled = true\n\n# Without the menu: how many items are in the row (1 to 8).\nchoices = 3\nmenu = true\n"},
  {"section":"martyr","name":"Martyr","kind":"Relic","rarity":"epic","ini":"# Martyr, a relic: hurts.\nenabled = true\nname = big bang\n"}
]})";

static void CatalogParsing() {
    auto cat = ParseCatalog(kCatalogJson);
    CHECK(cat && cat->items.size() == 3 && cat->header == "# CrabMods\n");
    if (!cat || cat->items.size() != 3) return;
    const CatalogItem& h = cat->items[0];
    CHECK(h.name == "Hoarder" && h.kind == "Perk" && h.rarity == "legendary" && h.toggle);
    CHECK(h.about == "More damage for each weapon mod.");
    CHECK(h.settings.size() == 2);
    CHECK(h.settings[0].key == "damagePercent" && h.settings[0].type == Setting::Int && h.settings[0].def == "3");
    CHECK(h.settings[0].help == "Damage bonus, in percent.");
    CHECK(h.settings[1].type == Setting::Choice && h.settings[1].choices.size() == 4 && h.settings[1].choices[3] == "greed");
    const CatalogItem& k = cat->items[1];
    CHECK(k.about == "Choose any item. A locked item you choose is unlocked.");
    CHECK(k.settings[0].range && k.settings[0].lo == 1 && k.settings[0].hi == 8);
    CHECK(k.settings[1].type == Setting::Bool && k.settings[1].help.empty());
    CHECK(cat->items[2].settings[0].type == Setting::Text);
    std::string ini = DefaultCrabModsIni(*cat);
    CHECK(ini.rfind("# CrabMods\n\n[hoarder]\n# Hoarder", 0) == 0 && ini.find("\n[martyr]\n") != std::string::npos);

    CatalogItem s = CrabSyncItem(kSyncIni);
    CHECK(s.about.rfind("Shared team inventory.", 0) == 0 && !s.toggle);
    CHECK(s.settings.size() == 5);
    CHECK(s.settings[3].key == "chestHealthCost" && s.settings[3].type == Setting::Choice && s.settings[3].choices.size() == 3);
    CHECK(s.settings[4].type == Setting::Float && s.settings[2].type == Setting::Int);

    IniValues v = ReadIni("[hoarder]\nenabled = false\ndamagePercent = 7\n");
    CHECK(!ItemOn(v, h) && ItemOn(v, k));
    CHECK(ValueOf(v, "hoarder", h.settings[0]) == "7" && ValueOf(v, "hoarder", h.settings[1]) == "legendary");

    CHECK(ListAround("Pick from a, b or c. Or not.", "b").size() == 3);
    CHECK(ListAround("nothing here", "b").empty());
}

static void SetupCodes() {
    auto cat = *ParseCatalog(kCatalogJson);
    CatalogItem sync = CrabSyncItem(kSyncIni);
    Setup all;
    all.crabSync = all.crabMods = true;
    std::string code = MakeSetupCode(cat, &sync, all);
    CHECK(code.rfind("CRAB2-S;M-", 0) == 0 && code.size() == 14);
    auto back = ReadSetupCode(cat, &sync, code);
    CHECK(back && back->crabSync && back->crabMods && back->on["hoarder"] && back->on["martyr"] && back->unknown == 0);

    Setup s;
    s.crabMods = true;
    s.on["martyr"] = false;
    s.values["hoarder"]["damagePercent"] = "5";
    s.values["hoarder"]["rarity"] = "legendary";  // the default: not in the code
    s.values["keyTotem"]["menu"] = "false";
    s.values["martyr"]["name"] = "a;b=c d%";
    code = MakeSetupCode(cat, &sync, s);
    CHECK(code.find("legendary") == std::string::npos && code.find("keyTotem.menu=0") != std::string::npos);
    CHECK(code.find(' ') == std::string::npos);
    back = ReadSetupCode(cat, &sync, "  " + code.substr(0, 9) + "\r\n" + code.substr(9));
    CHECK(back && !back->crabSync && back->crabMods && !back->on["martyr"] && back->on["hoarder"]);
    CHECK(back && back->values["hoarder"]["damagePercent"] == "5" && back->values["keyTotem"]["menu"] == "false");
    CHECK(back && back->values["martyr"]["name"] == "a;b=c d%");

    // Mostly off: listed as the ones that are on.
    Setup few;
    few.crabMods = true;
    few.on = {{"hoarder", false}, {"keyTotem", false}, {"martyr", true}};
    code = MakeSetupCode(cat, nullptr, few);
    CHECK(code.find(";0;+martyr") != std::string::npos || code.find("0;+martyr") == 6);
    back = ReadSetupCode(cat, nullptr, code);
    CHECK(back && !back->on["hoarder"] && !back->on["keyTotem"] && back->on["martyr"]);

    // Damaged, cut short, other things.
    std::string bad = code;
    bad[7] = bad[7] == 'x' ? 'y' : 'x';
    CHECK(!ReadSetupCode(cat, nullptr, bad));
    CHECK(!ReadSetupCode(cat, nullptr, code.substr(0, code.size() - 1)));
    CHECK(!ReadSetupCode(cat, nullptr, "CRAB1-abc"));
    // A newer version's item or setting is counted, not used.
    std::string payload = "M;-newThing;hoarder.newKey=1";
    back = ReadSetupCode(cat, nullptr, kSetupPrefix + payload + "-" + CodeCheck(payload));
    CHECK(back && back->unknown == 2 && back->on["hoarder"]);

    std::string ini = SetupIni(s, {&cat.items[0], &cat.items[2]});
    IniValues v = ReadIni(ini);
    CHECK(v["hoarder"]["enabled"] == "true" && v["hoarder"]["damagePercent"] == "5" && v["martyr"]["enabled"] == "false");
    CHECK(ReadIni(SetupIni(s, {&sync}))[""]["chestHealthCost"] == "split");
}

// --- Hashes and versions ----------------------------------------------------------------------------------------

static void Hashes() {
    CHECK(Sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(Sha256Hex(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // Fed in pieces of every size across the 64-byte blocks.
    std::string text;
    for (int i = 0; i < 300; i++) text += char('a' + i % 26);
    for (size_t step : {1, 7, 55, 56, 63, 64, 65, 200}) {
        Sha256 s;
        for (size_t i = 0; i < text.size(); i += step) s.Update(text.substr(i, step));
        CHECK(s.Hex() == Sha256Hex(text));
    }
}

// A minimal 64-bit PE with a version resource of the given version (or none).
static std::string FakePe(int major, int minor, int patch, bool withVersion) {
    std::string b(0x400, '\0');
    auto put16 = [&](size_t at, uint32_t v) { b[at] = char(v), b[at + 1] = char(v >> 8); };
    auto put32 = [&](size_t at, uint32_t v) { put16(at, v & 0xFFFF), put16(at + 2, v >> 16); };
    b[0] = 'M', b[1] = 'Z';
    put32(0x3C, 0x40);
    put32(0x40, 0x00004550);  // "PE\0\0"
    size_t coff = 0x44, opt = coff + 20;
    put16(coff + 2, 1);       // one section
    put16(coff + 16, 0xF0);   // optional header size
    put16(opt, 0x20b);        // PE32+
    if (withVersion) put32(opt + 112 + 16, 0x1000), put32(opt + 112 + 20, 0x100);
    size_t sec = opt + 0xF0;
    put32(sec + 8, 0x200), put32(sec + 12, 0x1000), put32(sec + 16, 0x200), put32(sec + 20, 0x200);
    if (!withVersion) return b;
    size_t r = 0x200;  // the resource tree: type 16 -> name 1 -> language 0x409 -> data
    put16(r + 14, 1), put32(r + 16, 16), put32(r + 20, 0x80000018);
    put16(r + 0x18 + 14, 1), put32(r + 0x18 + 16, 1), put32(r + 0x18 + 20, 0x80000030);
    put16(r + 0x30 + 14, 1), put32(r + 0x30 + 16, 0x409), put32(r + 0x30 + 20, 0x48);
    put32(r + 0x48, 0x1058), put32(r + 0x48 + 4, 0x80);
    size_t vi = r + 0x58;  // VS_VERSIONINFO: a header, then VS_FIXEDFILEINFO
    put16(vi, 0x80);
    put32(vi + 40, 0xFEEF04BD);
    put32(vi + 48, uint32_t(major) << 16 | uint32_t(minor));
    put32(vi + 52, uint32_t(patch) << 16);
    return b;
}

static void PeVersions() {
    CHECK(PeFileVersion(FakePe(0, 14, 2, true)) == "0.14.2");
    CHECK(PeFileVersion(FakePe(1, 0, 10, true)) == "1.0.10");
    CHECK(PeFileVersion(FakePe(1, 0, 0, false)) == "?");
    CHECK(PeFileVersion("{\"json\": true}").empty());
    CHECK(PeFileVersion("MZ").empty());
    CHECK(PeFileVersion(FakePe(0, 1, 0, true).substr(0, 0x220)) == "?");  // cut off in the resource: no crash
    // Any truncation of a real-looking file is safe.
    std::string full = FakePe(0, 1, 0, true);
    for (size_t n = 0; n < full.size(); n += 8) PeFileVersion(full.substr(0, n));
    if (Exists(g_build / "CrabMods.dll")) {
        std::string v = FileVersion(g_build / "CrabMods.dll");
        CHECK(ParseVersion(v).size() == 3);
        CHECK(!FileVersion(g_build / "xinput1_3.dll").empty());
        std::printf("  CrabMods.dll %s, CrabSync.dll %s, xinput1_3.dll %s\n", v.c_str(),
                    FileVersion(g_build / "CrabSync.dll").c_str(), FileVersion(g_build / "xinput1_3.dll").c_str());
    } else {
        std::printf("  (no built DLLs in %s: real version checks skipped)\n", U8(g_build).c_str());
    }
    CHECK(FileVersion(g_build / "does-not-exist.dll").empty());
}

static void ReleaseDetails() {
    const char* json = R"([
      {"tag_name":"crabmods-v0.14.1","html_url":"https://github.com/x/releases/tag/crabmods-v0.14.1","body":"Fixes","assets":[
        {"name":"CrabMods.dll","browser_download_url":"https://x/141.dll","size":3049984,
         "digest":"sha256:5c3145b2f6aba923d4bc9533a063bf5275d9341323b7f27810326bc6402cb5d2"},
        {"name":"CrabMods.catalog.json","browser_download_url":"https://x/c.json","size":10,"digest":null}]},
      {"tag_name":"crabmods-v0.9.0","assets":[{"name":"CrabMods.dll","browser_download_url":"https://x/09.dll"}]},
      {"tag_name":"crabmods-v0.13.4","assets":[{"name":"CrabMods.dll","browser_download_url":"https://x/134.dll"}]},
      {"tag_name":"manager-v0.4.0","assets":[{"name":"CrabModsManager.exe","browser_download_url":"https://x/m.exe"},
        {"name":"CrabModsManager-x86_64.AppImage","browser_download_url":"https://x/m.AppImage"}]}
    ])";
    ReleaseList list = ParseReleases(json);
    CHECK(list.ok && list.all["CrabMods"].size() == 3);
    const Release* r = list.Latest("CrabMods", "CrabMods.dll");
    CHECK(r && r->version == "0.14.1" && r->page.ends_with("crabmods-v0.14.1") && r->notes == "Fixes");
    CHECK(r && r->Get("CrabMods.dll")->sha256 == "5c3145b2f6aba923d4bc9533a063bf5275d9341323b7f27810326bc6402cb5d2");
    CHECK(r && r->Get("CrabMods.dll")->size == 3049984 && r->Get("CrabMods.catalog.json")->sha256.empty());
    CHECK(list.all["CrabMods"].size() == 3 && list.all["CrabMods"][1].version == "0.13.4" &&
          list.all["CrabMods"][2].version == "0.9.0");  // newest first
    const Release* before = list.Before("CrabMods", "0.14.1", "CrabMods.dll");
    CHECK(before && before->version == "0.13.4");
    CHECK(list.Version("CrabMods", "0.9.0") && !list.Version("CrabMods", "0.10.0"));
    CHECK(!list.Before("CrabMods", "0.9.0", "CrabMods.dll"));
    auto latest = LatestReleases(json, kManagerAppImage);
    CHECK(latest.count("Manager") && latest["Manager"].Has(kManagerAppImage));
    CHECK(!ParseReleases("{\"message\":\"Not Found\"}").ok);
    CHECK(CatalogAsset(Mods()[0]) == "CrabMods.catalog.json" && CatalogAsset(Mods()[1]) == "CrabSync.catalog.json");
    auto ini = ParseSyncCatalog(std::string("{\"ini\":") + JsonString(kSyncIni) + "}");
    CHECK(ini && *ini == kSyncIni);
    CHECK(!ParseSyncCatalog("[]"));
}

// --- Settings, updates and the list -------------------------------------------------------------------------

static void Configs() {
    // Written by manager 0.3: both auto-updates "true", but the player never chose: asked once.
    Config old = ParseConfig("game=C:\\Games\\Win64\nlastCrash=UE4CC-1\nsetupDone=true\nautoUpdateMods=true\n"
                             "autoUpdateManager=true\nlinuxHintDone=false\n");
    CHECK(old.setupDone && !old.autoMods && !old.autoManager && old.game == "C:\\Games\\Win64" && old.lastCrash == "UE4CC-1");
    CHECK(old.checkMinutes == 5 && !old.offline && old.lastRun.empty());
    // Manager 0.2 had no setupDone: used already if it knew the game.
    CHECK(ParseConfig("game=/x\n").setupDone && !ParseConfig("game=\n").setupDone && !ParseConfig("").setupDone);
    Config c;
    c.game = "/home/me/Win64";
    c.setupDone = true;
    c.autoMods = false;
    c.autoManager = true;
    c.checkMinutes = 15;
    c.offline = true;
    c.skip["CrabMods"] = "0.14.2";
    c.lastRun = "0.4.0";
    Config back = ParseConfig(ConfigText(c));
    CHECK(back.game == c.game && back.setupDone && back.autoMods == false && back.autoManager == true);
    CHECK(back.checkMinutes == 15 && back.offline && back.skip["CrabMods"] == "0.14.2" && back.lastRun == "0.4.0");
    // Only one answered: both asked again.
    c.autoManager.reset();
    back = ParseConfig(ConfigText(c));
    CHECK(!back.autoMods && !back.autoManager);
    CHECK(ParseConfig("checkMinutes=0\n").checkMinutes == 0 && ParseConfig("checkMinutes=-3\n").checkMinutes == 0);
    CHECK(ParseConfig("checkMinutes=soon\n").checkMinutes == 5);
}

static void UpdatePolicy() {
    CHECK(DecideUpdate("0.14.1", "0.14.2", true, "") == UpdateAction::Auto);
    CHECK(DecideUpdate("0.14.1", "0.14.2", false, "") == UpdateAction::Banner);
    CHECK(DecideUpdate("0.14.1", "0.14.2", std::nullopt, "") == UpdateAction::Banner);  // not asked yet
    CHECK(DecideUpdate("0.14.1", "0.14.2", true, "0.14.2") == UpdateAction::None);      // skipped
    CHECK(DecideUpdate("0.14.1", "0.14.3", false, "0.14.2") == UpdateAction::Banner);   // newer than the skipped one
    CHECK(DecideUpdate("0.14.2", "0.14.2", true, "") == UpdateAction::None);
    CHECK(DecideUpdate("0.15.0", "0.14.2", true, "") == UpdateAction::None);            // ahead (a rollback undone by hand)
    CHECK(DecideUpdate("", "0.14.2", true, "") == UpdateAction::None);                  // not installed
    CHECK(DecideUpdate("?", "0.14.2", false, "") == UpdateAction::Banner);              // a release before versions
}

static void Polling() {
    Poller p;
    CHECK(p.Due(1000, 5) && !p.Due(1000, 0));  // 0 minutes: never on its own
    HttpResponse ok{200};
    p.Done(1000, 5, ok);
    CHECK(p.next == 1300 && !p.Due(1299, 5) && p.Due(1300, 5));
    HttpResponse same{304};
    p.Done(1300, 5, same);
    CHECK(p.next == 1600 && p.failures == 0);
    HttpResponse limited{403};
    limited.rateRemaining = 0;
    limited.rateReset = 5000;
    p.Done(1600, 5, limited);
    CHECK(p.next == 5005);
    HttpResponse down{0};
    p.Done(2000, 5, down);
    CHECK(p.next == 2300 && p.failures == 1);
    p.Done(2000, 5, down);
    CHECK(p.next == 2600);
    for (int i = 0; i < 10; i++) p.Done(2000, 5, down);
    CHECK(p.next == 2000 + 3600);  // never more than an hour
    p.Done(9000, 5, ok);
    CHECK(p.failures == 0 && p.next == 9300);

    std::string list = "[{\"tag_name\":\"crabmods-v1.0.0\",\"assets\":[]}]";
    HttpResponse r{200, list, "W/\"abc\""};
    CheckResult c = ReadCheck(r, "", "");
    CHECK(c.changed && !c.failed && c.etag == "W/\"abc\"" && c.body == list);
    c = ReadCheck(HttpResponse{304}, list, "W/\"abc\"");
    CHECK(!c.changed && !c.failed && c.body == list && c.etag == "W/\"abc\"");
    c = ReadCheck(r, list, "W/\"abc\"");
    CHECK(!c.changed && !c.failed);
    c = ReadCheck(HttpResponse{500}, list, "e");
    CHECK(c.failed && c.body == list && c.error == "GitHub answered 500");
    c = ReadCheck(limited, list, "e");
    CHECK(c.failed && c.error.find("limit") != std::string::npos);
    c = ReadCheck(HttpResponse{200, "{\"message\":\"x\"}"}, list, "e");
    CHECK(c.failed && c.body == list);
    c = ReadCheck(HttpResponse{304}, "", "");  // a 304 with nothing cached: nothing to show
    CHECK(c.failed);
}

static void SeenAndSorting() {
    auto cat = *ParseCatalog(kCatalogJson);
    Seen seen;
    CHECK(NoteSeen(seen, cat, "0.14.1", true) && seen.size() == 3 && seen["hoarder"].empty());
    CHECK(!NoteSeen(seen, cat, "0.14.2", false));
    CatalogItem extra = cat.items[0];
    extra.section = "glassCannon";
    extra.name = "Glass Cannon";
    cat.items.push_back(extra);
    CHECK(NoteSeen(seen, cat, "0.14.2", false) && seen["glassCannon"] == "0.14.2");
    CHECK(IsNew(seen, "glassCannon", "0.14.2") && !IsNew(seen, "hoarder", "0.14.2") && !IsNew(seen, "glassCannon", "0.15.0"));
    CHECK(ParseSeen(SeenJson(seen)) == seen);

    std::vector<Entry> e = {
        {"pack", "CrabMods", "Mod pack"}, {"sync", "CrabSync", "Multiplayer"},
        {"b", "Bravo", "Relic", "epic", "", true}, {"a", "Alpha", "Perk", "greed", "", false},
        {"c", "Charlie", "Perk", "common", "", true}, {"d", "Delta", "Tweak", "", "", false, nullptr, "0.14.2"},
    };
    auto ids = [&] {
        std::string s;
        for (auto& x : e) s += x.id;
        return s;
    };
    SortEntries(e, SortBy::Type, false);
    CHECK(ids() == "packsyncacbd");
    SortEntries(e, SortBy::Name, false);
    CHECK(ids() == "packsyncabcd");
    SortEntries(e, SortBy::Name, true);
    CHECK(ids() == "packsyncdcba");
    SortEntries(e, SortBy::Rarity, false);
    CHECK(ids() == "packsynccbad");  // common, epic, greed, then none
    SortEntries(e, SortBy::Installed, false);
    CHECK(ids() == "packsynccbad");  // installed first, then by type and name
    SortEntries(e, SortBy::Newest, false);
    CHECK(ids().rfind("packsyncd", 0) == 0);
}

static void SettingsWords() {
    IniValues now = ReadIni("[hoarder]\nenabled = true\ndamagePercent = 3\n");
    auto words = DescribeSettings(now, "[hoarder]\nenabled = false\ndamagePercent = 5\n[martyr]\nenabled = true\n",
                                  {{"hoarder", "Hoarder"}, {"martyr", "Martyr"}});
    CHECK(words.size() == 2);  // Martyr was on already (no section: the default)
    CHECK(words.size() == 2 && words[0] == "Hoarder: damagePercent 3 \xE2\x86\x92 5" && words[1] == "Hoarder: turned off");
    words = DescribeSettings({}, "healthPerPlayer = 300\n", {}, {{"", {{"healthPerPlayer", "250"}}}});
    CHECK(words.size() == 1 && words[0] == "healthPerPlayer 250 \xE2\x86\x92 300");
    CHECK(DescribeSettings(now, "[hoarder]\ndamagePercent = 3\n", {}).empty());
}

static void LaunchOptions() {
    const char* vdf = R"("UserLocalConfigStore" { "Software" { "Valve" { "Steam" { "apps" {
        "774801" { "LastPlayed" "1" "LaunchOptions" "WINEDLLOVERRIDES=\"xinput1_3=n,b\" %command%" }
        "7748010" { "LaunchOptions" "nothing" } } } } } })";
    CHECK(LaunchOptionInVdf(vdf));
    CHECK(!LaunchOptionInVdf(R"("774801" { "LaunchOptions" "-dx11" } "x" { "LaunchOptions" "xinput1_3" })"));
    CHECK(!LaunchOptionInVdf("\"774801\" \"1\" { \"LaunchOptions\" \"xinput1_3\" }"));  // a value, not its block
    CHECK(!LaunchOptionInVdf(""));
}

// --- Files: a scratch game folder ---------------------------------------------------------------------------

struct Scratch {
    fs::path root, game, state, cache;
    explicit Scratch(const char* name) {
        root = fs::temp_directory_path() / ("crabmgr-test-" + std::string(name));
        std::error_code ec;
        fs::remove_all(root, ec);
        game = root / GameSub();
        state = root / "state";
        cache = root / "cache";
        fs::create_directories(game);
        WriteFile(game / kGameExe, "MZ the game");
    }
    ~Scratch() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
    WantFile Want(const std::string& name, const std::string& mod, const std::string& version, const std::string& content) {
        fs::path src = CachedFile(cache, mod, version, name);
        WriteFile(src, content);
        WantFile w;
        w.name = name, w.mod = mod, w.version = version, w.sha256 = Sha256Hex(content), w.source = src, w.size = content.size();
        return w;
    }
};

// Every file under a folder with its hash, to compare before and after.
static std::map<std::string, std::string> Tree(const fs::path& dir) {
    std::map<std::string, std::string> t;
    for (auto& f : fs::recursive_directory_iterator(dir))
        if (f.is_regular_file()) t[U8(fs::relative(f.path(), dir))] = FileSha(f.path());
    return t;
}

static void Installing() {
    Scratch s("install");
    WriteFile(s.game / "xinput1_3.dll", "someone else's xinput");  // a controller fix, say
    auto before = Tree(s.root / "steamapps");
    InstallRecord rec;
    ChangePlan plan;
    plan.title = "Install CrabMods 0.14.1";
    PlanAdd(plan, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.1", "mods 0.14.1"));
    PlanAdd(plan, s.game, rec, s.Want("xinput1_3.dll", "loader", "1.0.0", "loader A"));
    CHECK(plan.files.size() == 2 && plan.files[0].op == FileChange::Add && plan.files[1].op == FileChange::Replace);
    CHECK(plan.files.size() == 2 && plan.files[1].backup);  // not the manager's: kept
    auto lines = plan.Lines();
    CHECK(lines.size() == 2 && lines[0] == "Add CrabMods.dll 0.14.1 (1 KB)" && lines[1].find("backed up") != std::string::npos);
    CHECK(Tree(s.root / "steamapps") == before && !Exists(s.state));  // planning changed nothing

    std::string error;
    CHECK(ApplyFiles(s.game, s.state, plan, rec, "t1", error) && error.empty());
    CHECK(ReadFile(s.game / "CrabMods.dll") == "mods 0.14.1" && ReadFile(s.game / "xinput1_3.dll") == "loader A");
    CHECK(rec.files.size() == 2 && rec.Find("xinput1_3.dll") && rec.Find("xinput1_3.dll")->backup == "backups/t1/xinput1_3.dll");
    CHECK(ReadFile(s.state / "backups" / "t1" / "xinput1_3.dll") == "someone else's xinput");
    InstallRecord saved = LoadRecord(s.state);
    CHECK(saved.exists && saved.files.size() == 2 && saved.Find("CrabMods.dll") &&
          saved.Find("CrabMods.dll")->sha256 == Sha256Hex("mods 0.14.1"));
    CHECK(!Exists(s.game / "CrabMods.dll.download"));

    // The same again: nothing to do.
    ChangePlan again;
    PlanAdd(again, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.1", "mods 0.14.1"));
    CHECK(again.Empty() && again.files[0].op == FileChange::Keep && again.Lines().empty());

    // An update replaces the manager's own file without a backup, and keeps the player's original backup.
    ChangePlan update;
    PlanAdd(update, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.2", "mods 0.14.2"));
    PlanAdd(update, s.game, rec, s.Want("xinput1_3.dll", "loader", "1.0.0", "loader B"));
    CHECK(update.files[0].op == FileChange::Replace && !update.files[0].backup && !update.files[1].backup);
    CHECK(ApplyFiles(s.game, s.state, update, rec, "t2", error));
    CHECK(rec.Find("xinput1_3.dll")->backup == "backups/t1/xinput1_3.dll" && rec.Find("CrabMods.dll")->version == "0.14.2");
    CHECK(!Exists(s.state / "backups" / "t2"));

    // A changed file in the cache is refused, and the game folder is left as it was.
    ChangePlan bad;
    PlanAdd(bad, s.game, rec, s.Want("CrabSync.dll", "CrabSync", "1.0.8", "sync"));
    WriteFile(bad.files[0].source, "sync, tampered with");
    auto mid = Tree(s.game);
    CHECK(!ApplyFiles(s.game, s.state, bad, rec, "t3", error) && error.find("checksum") != std::string::npos);
    CHECK(Tree(s.game) == mid && !rec.Find("CrabSync.dll"));

    // Someone changed the installed file by hand: replacing it backs theirs up.
    WriteFile(s.game / "CrabMods.dll", "patched by hand");
    ChangePlan fix;
    PlanAdd(fix, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.2", "mods 0.14.2"));
    CHECK(fix.files[0].op == FileChange::Replace && fix.files[0].backup);
}

static void RemoveAllRestores() {
    Scratch s("removeall");
    WriteFile(s.game / "xinput1_3.dll", "someone else's xinput");
    WriteFile(s.game / "Other.txt", "the game's");
    auto before = Tree(s.root / "steamapps");
    InstallRecord rec;
    ChangePlan plan;
    PlanAdd(plan, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.1", "mods"));
    PlanAdd(plan, s.game, rec, s.Want("CrabSync.dll", "CrabSync", "1.0.8", "sync"));
    PlanAdd(plan, s.game, rec, s.Want("xinput1_3.dll", "loader", "1.0.0", "loader"));
    std::string error;
    CHECK(ApplyFiles(s.game, s.state, plan, rec, "t1", error));
    WriteFile(s.game / "CrabMods.ini", "[hoarder]\nenabled = false\n");  // the mods' own files
    WriteFile(s.game / "CrabMods.log", "log");

    ChangePlan remove;
    for (const RecordEntry& e : rec.files) PlanRemove(remove, s.game, rec, e.name);
    remove.deletes = {"CrabMods.ini", "CrabMods.log"};
    CHECK(remove.files.size() == 3 && remove.files[2].restore == "backups/t1/xinput1_3.dll" && !remove.files[0].backup);
    CHECK(ApplyFiles(s.game, s.state, remove, rec, "t2", error));
    CHECK(Tree(s.root / "steamapps") == before);  // exactly as before
    CHECK(rec.files.empty() && LoadRecord(s.state).files.empty() && !Exists(s.state / "backups" / "t1"));

    // Uninstalling a file the manager didn't write keeps a copy of it.
    WriteFile(s.game / "CrabSync.dll", "a hand-installed CrabSync");
    ChangePlan un;
    PlanRemove(un, s.game, rec, "CrabSync.dll");
    PlanRemove(un, s.game, rec, "NotThere.dll");
    CHECK(un.files.size() == 1 && un.files[0].backup);
    CHECK(ApplyFiles(s.game, s.state, un, rec, "t3", error));
    CHECK(!Exists(s.game / "CrabSync.dll") && ReadFile(s.state / "backups" / "t3" / "CrabSync.dll") == "a hand-installed CrabSync");
}

static void Migration() {
    Scratch s("migrate");
    WriteFile(s.game / "CrabMods.dll", FakePe(0, 14, 1, true));
    WriteFile(s.game / "xinput1_3.dll", FakePe(1, 0, 0, true));
    auto before = Tree(s.game);
    InstallRecord rec = LoadRecord(s.state);
    CHECK(!rec.exists);
    CHECK(AdoptInstalled(s.game, rec));
    CHECK(rec.files.size() == 2 && rec.Find("CrabMods.dll") && rec.Find("CrabMods.dll")->adopted &&
          rec.Find("CrabMods.dll")->version == "0.14.1");
    CHECK(rec.Find("xinput1_3.dll") && rec.Find("xinput1_3.dll")->mod == "loader" && rec.Find("xinput1_3.dll")->backup.empty());
    CHECK(Tree(s.game) == before);
    // Adopted files are the manager's now: updating them needs no backup.
    ChangePlan plan;
    PlanAdd(plan, s.game, rec, s.Want("CrabMods.dll", "CrabMods", "0.14.2", "new"));
    CHECK(plan.files[0].op == FileChange::Replace && !plan.files[0].backup);
    SaveRecord(s.state, rec);
    InstallRecord again = LoadRecord(s.state);
    CHECK(!AdoptInstalled(s.game, again) && again.files.size() == 2);  // once
    // Only a loader (some other program's): not taken.
    Scratch t("migrate2");
    WriteFile(t.game / "xinput1_3.dll", "other");
    InstallRecord r2;
    AdoptInstalled(t.game, r2);
    CHECK(r2.files.empty());
}

static void Downloads() {
    Scratch s("download");
    int calls = 0;
    std::string served = "the dll";
    Fetch fetch = [&](const std::string& url) {
        calls++;
        HttpResponse r;
        r.status = url == "https://x/ok" ? 200 : 404;
        if (r.status == 200) r.body = served;
        return r;
    };
    Asset a{"CrabMods.dll", "https://x/ok", Sha256Hex("the dll"), 7};
    fs::path target = CachedFile(s.cache, "CrabMods", "0.14.1", "CrabMods.dll");
    std::string error;
    CHECK(DownloadChecked(fetch, a, target, error) && ReadFile(target) == "the dll" && calls == 1);
    CHECK(DownloadChecked(fetch, a, target, error) && calls == 1);  // cached
    WriteFile(target, "damaged");
    CHECK(DownloadChecked(fetch, a, target, error) && calls == 2 && ReadFile(target) == "the dll");
    served = "something else";  // changed on the way (or on the server)
    fs::path t2 = s.cache / "x.dll";
    CHECK(!DownloadChecked(fetch, a, t2, error) && !Exists(t2) && error.find("checksum") != std::string::npos);
    Asset noHash{"x.dll", "https://x/ok", "", 1};
    CHECK(!DownloadChecked(fetch, noHash, t2, error) && error.find("no checksum") != std::string::npos);
    Asset missing{"y.dll", "https://x/404", Sha256Hex("y"), 1};
    CHECK(!DownloadChecked(fetch, missing, t2, error) && error.find("404") != std::string::npos);
}

static void CacheKeepsTwo() {
    Scratch s("cache");
    for (const char* v : {"0.13.4", "0.14.0", "0.14.1", "0.9.0", "0.12.0"})
        WriteFile(CachedFile(s.cache, "CrabMods", v, "CrabMods.dll"), v);
    PruneCache(s.cache / "CrabMods", 2, {"0.12.0"});
    std::set<std::string> left;
    for (auto& d : fs::directory_iterator(s.cache / "CrabMods")) left.insert(U8(d.path().filename()));
    CHECK(left == (std::set<std::string>{"0.14.1", "0.14.0", "0.12.0"}));
    PruneCache(s.cache / "Nothing", 2, {});  // no folder: nothing happens
}

static void GameFolders() {
    Scratch s("folders");
    fs::path lib = s.root;
    CHECK(NormalizeGameDir(s.game) == s.game);
    CHECK(NormalizeGameDir(lib / "steamapps" / "common" / "Crab Champions") == s.game);
    CHECK(NormalizeGameDir(lib) == s.game);
    CHECK(NormalizeGameDir(PathU8(U8(lib) + "/")) == s.game);
    CHECK(NormalizeGameDir(lib / "nowhere").empty() && NormalizeGameDir({}).empty());
    CHECK(LibraryOf(s.game) == lib);
    CHECK(ProtonSaved(s.game) == lib / "steamapps" / "compatdata" / "774801" / "pfx" / "drive_c" / "users" / "steamuser" /
                                     "AppData" / "Local" / "CrabChampions" / "Saved");
    // Found through another Steam folder's libraryfolders.vdf.
    fs::path steam = s.root / "steamroot";
    WriteFile(steam / "steamapps" / "libraryfolders.vdf",
              "\"libraryfolders\" { \"0\" { \"path\" \"ELSEWHERE\" } \"1\" { \"path\" \"LIB\" } }");
    auto libPath = [&](const std::string& p) { return p == "LIB" ? lib : s.root / "nothing" / PathU8(p); };
    CHECK(FindGameIn({s.root / "nosteam", steam}, libPath) == s.game);
    CHECK(FindGameIn({s.root / "nosteam"}, libPath).empty());
    // Launch options in userdata.
    CHECK(!LaunchOptionSetIn({steam}).has_value());
    WriteFile(steam / "userdata" / "123" / "config" / "localconfig.vdf", "\"774801\" { \"LaunchOptions\" \"-x\" }");
    CHECK(LaunchOptionSetIn({steam}) == false);
    WriteFile(steam / "userdata" / "456" / "config" / "localconfig.vdf", "\"774801\" { \"LaunchOptions\" \"xinput1_3=n,b\" }");
    CHECK(LaunchOptionSetIn({steam}) == true);
}

static void CrashFolders() {
    Scratch s("crash");
    fs::path saved = s.root / "Saved";
    CHECK(!NewestCrash({{saved, false}}));
    WriteFile(saved / "Crashes" / "UE4CC-Old" / "CrashContext.runtime-xml", "<ErrorMessage>old</ErrorMessage>");
    WriteFile(saved / "Crashes" / "UE4CC-NoContext" / "x.log", "x");
    WriteFile(saved / "Crashes" / "UE4CC-New" / "CrashContext.runtime-xml",
              "<ErrorMessage>Unhandled Exception: boom</ErrorMessage><MachineId>SECRETID</MachineId>");
    fs::last_write_time(saved / "Crashes" / "UE4CC-Old" / "CrashContext.runtime-xml", FileNow() - std::chrono::hours(5));
    auto c = NewestCrash({{saved, true}});
    CHECK(c && c->name == "UE4CC-New" && c->proton && c->size > 0);
    WriteFile(s.game / "CrabMods.log", "started on PC-SECRET\n");
    ReportInput in{s.game, {{saved, true}}, c, s.root / "CrashReports", {"PC-SECRET"}, "CrabMods 0.14.1", "Windows"};
    BuiltReport b = BuildReport(in);
    CHECK(b.error.empty() && Exists(b.zip) && b.report.platform == "Linux (Proton)");
    CHECK(b.report.error.find("boom") != std::string::npos);
    auto log = ReadFile(b.folder / "CrabMods.log");
    CHECK(log && log->find("PC-SECRET") == std::string::npos && log->find("started on") != std::string::npos);
    auto xml = ReadFile(b.folder / "crash" / "CrashContext.runtime-xml");
    CHECK(xml && xml->find("SECRETID") == std::string::npos);
    auto zip = ReadFile(b.zip);
    CHECK(zip && zip->find("PC-SECRET") == std::string::npos && zip->find("report.md") != std::string::npos);
    CHECK(LocalTime(FileNow(), true).size() == 19);
    // Logs only, without a crash.
    ReportInput logs{s.game, {}, std::nullopt, s.root / "CrashReports", {}, "CrabMods 0.14.1", "Windows"};
    BuiltReport l = BuildReport(logs);
    CHECK(l.error.empty() && U8(l.folder.filename()).rfind("CrabLogs-", 0) == 0 && l.report.platform == "Windows");
}

// --- The real catalogs, from the build --------------------------------------------------------------------------

static void RealCatalog() {
    auto json = ReadFile(g_build / "CrabMods.catalog.json");
    auto syncJson = ReadFile(g_build / "CrabSync.catalog.json");
    if (!json || !syncJson) {
        std::printf("  (no catalogs in %s: real catalog checks skipped)\n", U8(g_build).c_str());
        return;
    }
    auto cat = ParseCatalog(*json);
    CHECK(cat && cat->items.size() >= 30);
    if (!cat) return;
    for (const CatalogItem& it : cat->items) {
        if (it.about.empty() || it.name.empty() || KindRank(it.kind) >= int(std::size(kKinds)) || !it.toggle)
            std::printf("  incomplete: %s (%s)\n", it.section.c_str(), it.kind.c_str());
        CHECK(!it.about.empty() && !it.name.empty() && KindRank(it.kind) < int(std::size(kKinds)) && it.toggle);
        for (const Setting& s : it.settings)
            if (s.key == "rarity") CHECK(s.type == Setting::Choice);
    }
    Setup mods;
    mods.crabMods = true;
    CHECK(MakeSetupCode(*cat, nullptr, mods).size() == 12);
    // Every item starts on in a new ini, and a setup code of everything reads back.
    IniValues fresh = ReadIni(DefaultCrabModsIni(*cat));
    for (const CatalogItem& it : cat->items) CHECK(ItemOn(fresh, it));
    auto ini = ParseSyncCatalog(*syncJson);
    CHECK(ini && CrabSyncItem(*ini).settings.size() > 10);
    if (!ini) return;
    CatalogItem sync = CrabSyncItem(*ini);
    Setup all;
    all.crabMods = all.crabSync = true;
    for (const CatalogItem& it : cat->items) all.on[it.section] = true;
    auto back = ReadSetupCode(*cat, &sync, MakeSetupCode(*cat, &sync, all));
    CHECK(back && back->crabMods && back->crabSync && back->unknown == 0);
    std::printf("  catalog: %zu items, CrabSync %zu settings\n", cat->items.size(), sync.settings.size());
}

int main(int argc, char** argv) {
    g_build = argc > 1 ? fs::path(argv[1]) : fs::path(".");
    Versions();
    Releases();
    ShareCodes();
    Merge();
    Libraries();
    Crashes();
    CatalogParsing();
    SetupCodes();
    Hashes();
    PeVersions();
    ReleaseDetails();
    Configs();
    UpdatePolicy();
    Polling();
    SeenAndSorting();
    SettingsWords();
    LaunchOptions();
    Installing();
    RemoveAllRestores();
    Migration();
    Downloads();
    CacheKeepsTwo();
    GameFolders();
    CrashFolders();
    RealCatalog();
    std::printf(g_failures ? "manager tests: %d FAILED\n" : "manager tests: all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
