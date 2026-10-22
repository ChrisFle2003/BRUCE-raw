#include "../../apps/artifact_store_v3_cli.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace bruce::artifact_store_v3_cli_detail;
    try {
        constexpr std::string_view sha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        const auto valid = [&](std::string_view size) {
            return std::vector<std::string_view>{"fetch", sha, size, "--store", "private-store",
                "--provider", "https-a", "--providers", "private-config"};
        };
        const auto request = parse(valid("17"));
        require(request.expected.sha256 == sha && request.expected.size_bytes == 17 &&
                request.store == "private-store" && request.provider == "https-a" &&
                request.providers == "private-config", "fetch request lost an argument");
        require(parse(valid("0")).expected.size_bytes == 0, "empty object rejected");
        require(parse(valid("33554432")).expected.size_bytes == 33554432, "maximum rejected");
        auto bad = std::vector<std::vector<std::string_view>>{{}};
        const auto at = [&](std::size_t index, std::string_view value) {
            auto args = valid("17"); args[index] = value; bad.push_back(args);
        };
        for (auto value : {"import", "resolve", "publish", "--help", "FETCH"}) at(0, value);
        for (auto value : {"", "../digest", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
                "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeffg"}) at(1, value);
        for (auto value : {"", "00", "01", "+1", "-1", "1.0", " 1", "1 ", "33554433",
                "18446744073709551616", "999999999999999999999999999"}) at(2, value);
        for (auto index : {3U, 5U, 7U}) at(index, "--url");
        at(5, "--store"); at(7, "--provider");
        for (auto index : {4U, 8U}) {
            at(index, ""); at(index, "--option"); at(index, std::string_view("a\0b", 3));
        }
        const std::string long_id(129, 'a');
        for (auto value : {std::string_view(""), std::string_view("-a"), std::string_view("../a"),
                std::string_view("https://a"), std::string_view("a b"), std::string_view("a\0b", 3),
                std::string_view("\xc3\xa4"), std::string_view(long_id)}) at(6, value);
        for (std::size_t count = 1; count < 9; ++count) {
            auto args = valid("17"); args.resize(count); bad.push_back(args);
        }
        auto extra = valid("17"); extra.push_back("extra"); bad.push_back(extra);
        extra = valid("17"); extra.insert(extra.end(), {"--provider", "https-b"}); bad.push_back(extra);
        bad.push_back({"fetch", sha, "17", "--provider", "https-a", "--store", "store", "--providers", "config"});
        for (const auto& args : bad) {
            bool rejected = false;
            try { (void)parse(args); }
            catch (const bruce::Error& error) { rejected = std::string_view(error.what()) == "PROTOCOL_ERROR"; }
            require(rejected, "open argument grammar accepted");
        }
        require(render(request.provider, request.expected) ==
            "{\"contract\":\"artifact-store-fetch-v3\",\"action\":\"fetch\",\"status\":\"FETCHED\","
            "\"provider\":\"https-a\",\"sha256\":\"0123456789abcdef0123456789abcdef"
            "0123456789abcdef0123456789abcdef\",\"size_bytes\":17}", "noncanonical or private output");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
