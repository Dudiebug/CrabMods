// Manager tests: versions, the releases list, share codes, ini merging and crash reports (manager/core.hpp).
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

int main() {
    Versions();
    Releases();
    ShareCodes();
    Merge();
    Libraries();
    Crashes();
    std::printf(g_failures ? "manager tests: %d FAILED\n" : "manager tests: all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
