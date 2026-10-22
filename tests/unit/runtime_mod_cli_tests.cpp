#include "../../apps/runtime_mod_cli.hpp"
#include <iostream>
#include <limits>
#include <vector>
#include <sstream>
#include <unistd.h>

namespace {
struct FixedClock final : bruce::RuntimeTrustClockV2 {
    std::string now_utc() const override { return "2026-10-19T12:00:00Z"; }
};
void blocked_trust_cli_reports_affected() {
    char pattern[] = "/tmp/bruce-trust-cli-unit-XXXXXX";
    const auto made = ::mkdtemp(pattern);
    if (!made) throw bruce::Error("TrustCliFixtureFailed");
    struct Temporary {
        std::string path;
        ~Temporary() { std::filesystem::remove_all(path); }
    } temporary{made};
    const auto root = temporary.path + "/mods";
    const auto fixture = std::string(BRUCE_RUNTIME_FIXTURE_DIR);
    const auto trust = bruce::read_runtime_mod_truststore_v1(bruce::LocalFiles::read(fixture + "/test-truststore-v1.pub"));
    auto host = bruce::InstalledModHostV2::open(root, trust);
    host.install(bruce::detect_mod_package_v2(bruce::LocalFiles::read(fixture + "/math-rational-power-v1.brucemod")));
    host.activate("user.math-rational-power", "1.0.0");
    FixedClock clock;
    host.import_trust_v1(clock);
    const auto external = temporary.path + "/next.pub";
    bruce::LocalFiles::publish_new_atomically(external, "BRUCE_RUNTIME_MOD_TRUSTSTORE_V1\n");
    host.prepare_trust(external, clock);
    const auto image = [&] {
        std::map<std::string, std::string> bytes;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            if (entry.is_regular_file()) bytes.emplace(entry.path().string(), bruce::LocalFiles::read(entry.path().string()));
        return bytes;
    };
    const auto before = image();
    const auto alias = temporary.path + "/ancestor";
    std::filesystem::create_directory_symlink(temporary.path, alias);
    for (const auto& spelling : {alias + "/mods", alias + "/./mods", alias + "/../" +
            std::filesystem::path(temporary.path).filename().string() + "/mods"}) {
        for (const bool emergency : {false, true}) {
            auto unsafe = bruce::InstalledModHostV2::for_trust_operations(spelling);
            for (const bool rollback : {false, true}) {
                try {
                    if (rollback) (void)unsafe.rollback_trust(emergency);
                    else (void)unsafe.activate_trust(emergency);
                } catch (const bruce::Error& error) {
                    if (std::string_view(error.what()) != "RuntimeModFileOpenFailed") throw;
                    if (image() != before) throw bruce::Error("UnsafeTrustHostChangedFilesystem");
                    continue;
                }
                throw bruce::Error("TrustHostAcceptedAncestorSymlink");
            }
        }
    }
    std::vector<std::string> words{"bruce", "mods", "trust", "activate", "--mod-root", root};
    std::vector<char*> argv;
    for (auto& word : words) argv.push_back(word.data());
    std::ostringstream output, error;
    struct Capture {
        std::streambuf *out, *err;
        ~Capture() { std::cout.rdbuf(out); std::cerr.rdbuf(err); }
    } capture{std::cout.rdbuf(output.rdbuf()), std::cerr.rdbuf(error.rdbuf())};
    int code = 0;
    try { code = bruce::runtime_mod_cli(static_cast<int>(argv.size()), argv.data()); }
    catch (const bruce::Error&) { throw bruce::Error("BlockedTrustCliDidNotReturnStructuredAffected"); }
    const std::string expected = "{\"error\":\"RuntimeTrustActiveModsIncompatible\",\"affected\":[{\"kind\":\"data\",\"mod_id\":\"user.math-rational-power\",\"version\":\"1.0.0\",\"signing_key_id\":\"test-rfc8032\"}]}\n";
    if (code != 2 || !output.str().empty() || error.str() != expected)
        throw bruce::Error("BlockedTrustCliOutputContractMismatch");
    if (image() != before) throw bruce::Error("BlockedTrustCliChangedFilesystem");
}

void trust_parser_contracts() {
    // Every grammar failure must be detected before opening a root, and may
    // never echo user-controlled option names, paths or values.
    const std::vector<std::pair<std::vector<std::string>, std::string>> cases{
        {{"trust"}, "MissingRuntimeTrustAction"},
        {{"trust", "unknown"}, "UnknownRuntimeTrustAction"},
        {{"trust", "status"}, "MissingOption"},
        {{"trust", "verify"}, "MissingOption"},
        {{"trust", "import-v1"}, "MissingOption"},
        {{"trust", "prepare", "next.pub"}, "MissingOption"},
        {{"trust", "activate"}, "MissingOption"},
        {{"trust", "rollback"}, "MissingOption"},
        {{"trust", "prepare"}, "MissingRuntimeTrustStore"},
        {{"trust", "status", "--mod-root"}, "MissingOptionValue"},
        {{"trust", "status", "--mod-root", ""}, "MissingOption"},
        {{"trust", "status", "--mod-root", ".", "--mod-root", "."}, "DuplicateOption"},
        {{"trust", "status", "--secret-path", "/private/key"}, "UnknownOption"},
        {{"trust", "verify", "unexpected", "--mod-root", "."}, "UnknownOption"},
        {{"trust", "import-v1", "--trust-store", "/private/key", "--mod-root", "."}, "UnknownOption"},
        {{"trust", "prepare", "--mod-root", "."}, "MissingRuntimeTrustStore"},
        {{"trust", "status", "--mod-root", "--deactivate-untrusted"}, "MissingOptionValue"},
        {{"trust", "verify", "--deactivate-untrusted", "--mod-root", "."}, "UnknownOption"},
        {{"trust", "activate", "--mod-root", ".", "--deactivate-untrusted", "--deactivate-untrusted"}, "DuplicateOption"},
        {{"trust", "rollback", "--mod-root", ".", "--deactivate-untrusted", "--no-deactivate-untrusted"}, "UnknownOption"}
    };
    for (const auto& [tail, expected] : cases) {
        std::vector<std::string> words{"bruce", "mods"};
        words.insert(words.end(), tail.begin(), tail.end());
        std::vector<char*> argv;
        for (auto& word : words) argv.push_back(word.data());
        try {
            bruce::runtime_mod_cli(static_cast<int>(argv.size()), argv.data());
        } catch (const bruce::Error& error) {
            if (error.what() != expected)
                throw bruce::Error("TrustParserExpected:" + expected + ":got:" + error.what());
            continue;
        }
        throw bruce::Error("InvalidTrustArgumentsAccepted");
    }
}
}

int main() {
    try {
        trust_parser_contracts();
        blocked_trust_cli_reports_affected();
        // A valid large tree can exceed the replay-file budget even when every
        // individual package is small. The renderer must reject before export.
        bruce::TreeFragmentV1 fragment;
        fragment.mount = {1, 1};
        bruce::TreeAddressV1 prefix = {1, 1, 7};
        for (unsigned depth = 3; depth <= 31; ++depth) {
            fragment.nodes.push_back({prefix, "level", "", {}, bruce::HostClaimScopeV1::unknown, "", {}});
            if (depth < 31) prefix.push_back(std::numeric_limits<std::uint32_t>::max());
        }
        for (std::uint32_t index = 1; index <= 4000; ++index) {
            auto address = prefix; address.push_back(index);
            fragment.nodes.push_back({address, std::to_string(index), "test.template", {},
                bruce::HostClaimScopeV1::candidate_only, "", {"x"}});
        }
        const auto tree = bruce::core_tree_registry_v1().mount(fragment, "large-test");
        const bruce::InstalledDataModRouteV1 routed{
            {}, tree.digest(), tree.route(bruce::normalize_route_text_v1("x"))};
        try {
            (void)bruce::runtime_mod_cli_detail::render_route(routed, "x");
        } catch (const bruce::Error& error) {
            if (std::string_view(error.what()) == "RouteReportTooLarge") return 0;
            throw;
        }
        throw bruce::Error("OversizedRouteReportWasExportable");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
