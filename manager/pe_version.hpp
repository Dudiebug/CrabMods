// The version of a Windows DLL or exe, read from its bytes (the VERSIONINFO resource), on any system. The manager
// uses it to tell which mod version is installed without asking Windows to open the file as a program.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace mgr {

namespace pe {
inline bool Read16(const std::string& b, size_t at, uint32_t& v) {
    if (at + 2 > b.size()) return false;
    v = uint8_t(b[at]) | uint32_t(uint8_t(b[at + 1])) << 8;
    return true;
}
inline bool Read32(const std::string& b, size_t at, uint32_t& v) {
    if (at + 4 > b.size()) return false;
    v = uint8_t(b[at]) | uint32_t(uint8_t(b[at + 1])) << 8 | uint32_t(uint8_t(b[at + 2])) << 16 |
        uint32_t(uint8_t(b[at + 3])) << 24;
    return true;
}
}  // namespace pe

// "0.14.1" (major.minor.patch of FILEVERSION). "" when the bytes are not a Windows program, "?" when it has no
// version resource (the mods' releases from before the manager).
inline std::string PeFileVersion(const std::string& b) {
    using namespace pe;
    uint32_t lfanew, sig, sections, optSize, magic;
    if (b.size() < 64 || b[0] != 'M' || b[1] != 'Z' || !Read32(b, 0x3C, lfanew) || !Read32(b, lfanew, sig) ||
        sig != 0x00004550)
        return "";
    size_t coff = lfanew + 4, opt = coff + 20;
    if (!Read16(b, coff + 2, sections) || !Read16(b, coff + 16, optSize) || !Read16(b, opt, magic)) return "";
    size_t dirs = magic == 0x20b ? opt + 112 : magic == 0x10b ? opt + 96 : 0;
    if (!dirs) return "";
    uint32_t rsrcRva, rsrcSize;
    if (!Read32(b, dirs + 2 * 8, rsrcRva) || !Read32(b, dirs + 2 * 8 + 4, rsrcSize) || !rsrcRva) return "?";
    // RVA -> file offset through the section table.
    size_t table = opt + optSize;
    auto offsetOf = [&](uint32_t rva, size_t& off) {
        for (uint32_t i = 0; i < sections; i++) {
            size_t s = table + i * 40;
            uint32_t vsize, va, rawSize, raw;
            if (!Read32(b, s + 8, vsize) || !Read32(b, s + 12, va) || !Read32(b, s + 16, rawSize) || !Read32(b, s + 20, raw))
                return false;
            uint32_t span = vsize > rawSize ? vsize : rawSize;
            if (rva >= va && rva < va + span) {
                off = raw + (rva - va);
                return off < b.size();
            }
        }
        return false;
    };
    size_t root;
    if (!offsetOf(rsrcRva, root)) return "?";
    // A resource directory's entry: by id `want` (or the first one when want < 0). Returns its OffsetToData.
    auto entry = [&](size_t dir, int want, uint32_t& data) {
        uint32_t named, ids;
        if (!Read16(b, dir + 12, named) || !Read16(b, dir + 14, ids)) return false;
        for (uint32_t i = 0; i < named + ids; i++) {
            uint32_t name, off;
            if (!Read32(b, dir + 16 + i * 8, name) || !Read32(b, dir + 16 + i * 8 + 4, off)) return false;
            if (want < 0 || (!(name & 0x80000000) && int(name) == want)) {
                data = off;
                return true;
            }
        }
        return false;
    };
    uint32_t o;
    if (!entry(root, 16 /*RT_VERSION*/, o) || !(o & 0x80000000)) return "?";
    if (!entry(root + (o & 0x7FFFFFFF), -1, o) || !(o & 0x80000000)) return "?";  // the first name
    if (!entry(root + (o & 0x7FFFFFFF), -1, o) || (o & 0x80000000)) return "?";   // the first language
    uint32_t dataRva, dataSize;
    size_t data;
    if (!Read32(b, root + o, dataRva) || !Read32(b, root + o + 4, dataSize) || !offsetOf(dataRva, data)) return "?";
    // VS_FIXEDFILEINFO starts with its signature, after the "VS_VERSION_INFO" header.
    size_t end = std::min<size_t>(b.size(), data + dataSize);
    for (size_t i = data; i + 16 <= end; i += 4) {
        uint32_t s, ms, ls;
        Read32(b, i, s);
        if (s != 0xFEEF04BD) continue;
        Read32(b, i + 8, ms);
        Read32(b, i + 12, ls);
        return std::to_string(ms >> 16) + "." + std::to_string(ms & 0xFFFF) + "." + std::to_string(ls >> 16);
    }
    return "?";
}

}  // namespace mgr
