// src/helpers/Compression.h : compressed by the system, decompressed by the system again

#include <format>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <compressapi.h>
#else
#include <zlib.h>
#endif

#include "Check.h"
#include "src/helpers/Compression.h"

namespace {

// decompressed by the system's decompressor matching compression::compress()
std::string decompress(const std::string& compressed, std::size_t originalSize) {
    std::string output(originalSize, '\0');
#ifdef _WIN32
    DECOMPRESSOR_HANDLE decompressor = nullptr;
    if (CreateDecompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &decompressor) == 0) return {};
    SIZE_T size = 0;
    const auto ok = Decompress(decompressor, const_cast<char*>(compressed.data()), compressed.size(), output.data(), output.size(), &size);
    CloseDecompressor(decompressor);
    if (ok == 0) return {};
#else
    auto size = static_cast<uLongf>(output.size());
    if (uncompress(reinterpret_cast<Bytef*>(output.data()), &size, reinterpret_cast<const Bytef*>(compressed.data()), compressed.size()) !=
        Z_OK)
        return {};
#endif
    output.resize(size);
    return output;
}

}  // namespace

int main() {
    // JSON alike, larger than the 32 KB blocks of MSZIP
    std::string library = "[";
    for (int i = 0; i < 20000; ++i) {
        library += std::format(R"({{"Name":"Track {} café","Artist":"Artist {}","Year":"{}"}},)", i, i % 97, 1960 + (i % 60));
    }
    library.back() = ']';

    const auto compressed = compression::compress(library);
    std::printf("  %zu -> %zu bytes\n", library.size(), compressed.size());

    CHECK(!compressed.empty() && compressed.size() < library.size() / 4, "compressed, at least 4 times smaller");
#ifdef _WIN32
    CHECK(compressed.starts_with("\x0A\x51\xE5\xC0"), "MSZIP buffer, as SoundVitrine detects it");
#else
    CHECK(static_cast<unsigned char>(compressed[0]) == 0x78, "zlib stream");
#endif
    CHECK(decompress(compressed, library.size()) == library, "round trip through the system's decompressor");

    const std::string small = "{}";
    CHECK(decompress(compression::compress(small), small.size()) == small, "tiny input");

    return check::summary();
}
