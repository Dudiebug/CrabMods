// Shared by the manager tests: a stand-in for a built DLL, and a folder's files with their hashes.
#pragma once
#include "../manager/files.hpp"

namespace mgr::test {

// A minimal 64-bit PE with a version resource of the given version (or none).
inline std::string FakePe(int major, int minor, int patch, bool withVersion) {
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

// Every file under a folder with its hash, to compare before and after.
inline std::map<std::string, std::string> Tree(const fs::path& dir) {
    std::map<std::string, std::string> t;
    for (auto& f : fs::recursive_directory_iterator(dir))
        if (f.is_regular_file()) t[U8(fs::relative(f.path(), dir))] = FileSha(f.path());
    return t;
}

}  // namespace mgr::test
