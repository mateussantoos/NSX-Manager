// Host harness: run the real extractor against real archives. Not shipped.
#include <cstdio>
#include <string>

#include "nsx/core/paths/extraction_policy.hpp"
#include "nsx/infra/archive/zip_extractor.hpp"

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("usage: harness <zip> <dest>\n");
        return 2;
    }
    nsx::core::ExtractionPolicy policy(argv[2], nsx::core::PreserveRules::defaults());
    const auto r = nsx::infra::extractZip(argv[1], policy, 64ull * 1024 * 1024);
    if (r.hasValue()) {
        std::printf("OK files=%zu dirs=%zu preserved=%zu\n", r.value().filesWritten,
                    r.value().directoriesCreated, r.value().skippedPreserved);
        return 0;
    }
    const auto d = nsx::infra::describe(r.error());
    std::printf("REFUSED %.*s\n", (int)d.size(), d.data());
    return 1;
}
