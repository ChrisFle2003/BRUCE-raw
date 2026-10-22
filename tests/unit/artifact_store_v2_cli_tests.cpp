#include "../../apps/artifact_store_v2_cli.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace bruce::artifact_store_v2_cli_detail;
    try {
        constexpr std::string_view digest =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        const auto valid = [&](std::string_view size) {
            return std::vector<std::string_view>{"fetch", digest, size, "--store", "store",
                "--provider", "mirror-a", "--providers", "providers.json"};
        };
        const auto request = parse(valid("17"));
        require(request.expected.sha256 == digest && request.expected.size_bytes == 17 &&
                    request.store == "store" && request.provider == "mirror-a" &&
                    request.providers == "providers.json", "complete fetch request");
        require(parse(valid("0")).expected.size_bytes == 0, "zero size rejected");
        const auto limit = std::to_string(bruce::max_file_bytes);
        require(parse(valid(limit)).expected.size_bytes == bruce::max_file_bytes,
                "maximum size rejected");
        for (const auto size : {"", "00", "01", "+1", "-1", "1.0", " 1", "1 ",
                                "33554433", "18446744073709551616", "999999999999999999999999"}) {
            bool rejected = false;
            try { (void)parse(valid(size)); }
            catch (const bruce::Error& error) { rejected = std::string_view(error.what()) == "PROTOCOL_ERROR"; }
            require(rejected, "noncanonical or excessive size accepted");
        }
        auto malformed = std::vector<std::vector<std::string_view>>{{}, {"fetch"}};
        const auto reject_at = [&](std::size_t index, std::string_view value) {
            auto args = valid("17");
            args[index] = value;
            malformed.push_back(args);
        };
        for (const auto action : {"import", "inspect", "verify", "resolve", "--help"}) reject_at(0, action);
        for (const auto sha : {"ABC", "../digest", "", "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffG"}) reject_at(1, sha);
        for (const auto index : {3U, 5U, 7U}) {
            reject_at(index, "--unknown");
            if (index != 3) reject_at(index, "--store");
        }
        for (const auto index : {4U, 8U}) {
            reject_at(index, "");
            reject_at(index, "--option");
            reject_at(index, std::string_view("a\0b", 3));
        }
        const std::string long_provider(129, 'a');
        for (const auto provider : {std::string_view(""), std::string_view("../mirror"),
                 std::string_view("https://mirror"), std::string_view("-mirror"),
                 std::string_view("a b"), std::string_view("a\0b", 3),
                 std::string_view("\xc3\xa4"), std::string_view(long_provider)}) reject_at(6, provider);
        for (std::size_t count = 2; count < 9; ++count) {
            auto args = valid("17");
            args.resize(count);
            malformed.push_back(args);
        }
        auto extra = valid("17");
        extra.push_back("extra");
        malformed.push_back(extra);
        extra = valid("17");
        extra.insert(extra.end(), {"--provider", "mirror-b"});
        malformed.push_back(extra);
        malformed.push_back({"fetch", digest, "17", "--provider", "mirror-a",
                             "--store", "store", "--providers", "providers.json"});
        for (const auto& args : malformed) {
            bool rejected = false;
            try { (void)parse(args); }
            catch (const bruce::Error& error) { rejected = std::string_view(error.what()) == "PROTOCOL_ERROR"; }
            require(rejected, "open CLI grammar accepted");
        }
        require(render(request.provider, request.expected) ==
            "{\"contract\":\"artifact-store-fetch-v2\",\"action\":\"fetch\",\"status\":\"FETCHED\","
            "\"provider\":\"mirror-a\",\"sha256\":\"0123456789abcdef0123456789abcdef"
            "0123456789abcdef0123456789abcdef\",\"size_bytes\":17}", "canonical metadata render");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
