// src/helpers/Hash.h

#include <fstream>

#include "Check.h"
#include "TestData.h"
#include "src/helpers/Hash.h"

int main() {
    const auto directory = testdata::scratchDirectory("hash é音");  // non-ASCII path

    const auto empty = directory / "empty.bin";
    { std::ofstream { empty, std::ios::binary }; }
    CHECK(hash::md5File(empty) == "d41d8cd98f00b204e9800998ecf8427e", "empty file");

    const auto abc = directory / "abc.bin";
    { std::ofstream { abc, std::ios::binary } << "abc"; }
    CHECK(hash::md5File(abc) == "900150983cd24fb0d6963f7d28e17f72", "RFC 1321 vector");

    // larger than the read chunks
    const auto big = directory / "big.bin";
    { std::ofstream { big, std::ios::binary } << std::string(1000000, 'a'); }
    CHECK(hash::md5File(big) == "7707d6ae4e027c70eea2a935c2296f21", "1 MB file, read by chunks");

    CHECK(hash::md5File(directory / "missing").empty(), "missing file : empty");
    CHECK(hash::md5File(directory).empty(), "directory : empty");

    testdata::removeScratchDirectories();
    return check::summary();
}
