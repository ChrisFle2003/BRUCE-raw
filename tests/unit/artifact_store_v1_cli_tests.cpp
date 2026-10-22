#include "../../apps/artifact_store_v1_cli.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    using namespace bruce::artifact_store_v1_cli_detail;
    try {
        const auto import = parse({"import", "input.bin", "--store", "/tmp/store"});
        require(import.action == Action::import_file && import.source == "input.bin" &&
                    import.store == "/tmp/store", "import parse");
        constexpr std::string_view digest =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        const auto inspect = parse({"inspect", digest, "--store", "store"});
        require(inspect.action == Action::inspect && inspect.digest == digest, "inspect parse");
        const auto verify = parse({"verify", digest, "--store", "store"});
        require(verify.action == Action::verify, "verify parse");
        const auto resolve = parse({"resolve", "manifest-1", "--store", "store"});
        require(resolve.action == Action::resolve && resolve.manifest_id == "manifest-1" &&
                    resolve.store == "store", "resolve parse");
        for (const auto& args : std::vector<std::vector<std::string_view>>{
                 {"import", "input", "store", "store"},
                 {"inspect", "../digest", "--store", "store"},
                 {"verify", "ABC", "--store", "store"},
                 {"resolve", "../manifest", "--store", "store"},
                 {"resolve", "/manifest", "--store", "store"},
                 {"resolve", "manifest", "store", "store"},
                 {"inspect", digest, "--store", "store", "--extra"},
                 {"import", "-", "--store", "store"}}) {
            bool rejected = false;
            try { (void)parse(args); } catch (const bruce::Error&) { rejected = true; }
            require(rejected, "invalid CLI accepted");
        }
        const bruce::artifact_v1::ArtifactIdV1 id{std::string(digest), 17};
        const auto rendered = render(Action::verify, id);
        require(rendered ==
                    "{\"contract\":\"artifact-store-v1\",\"action\":\"verify\","
                    "\"status\":\"VERIFIED\",\"sha256\":\"0123456789abcdef0123456789abcdef"
                    "0123456789abcdef0123456789abcdef\",\"size_bytes\":17}",
                "stable render");
        require(rendered.find("/tmp") == std::string::npos && rendered.find("payload") == std::string::npos,
                "metadata leak");
        bruce::artifact_v1::ArtifactResolvedManifestV1 resolved;
        resolved.manifest_id = "manifest-1";
        resolved.run_id = "run-1";
        resolved.node_id = "node-1";
        resolved.attempt_id = "attempt-1";
        resolved.manifest_status = "READY";
        resolved.result_code = "OK";
        resolved.objects.push_back({bruce::artifact_v1::ArtifactIdV1{std::string(digest), 17}, ""});
        resolved.objects.push_back({bruce::artifact_v1::ArtifactIdV1{std::string(64, 'f'), 0}, "second"});
        const auto resolved_rendered = render_resolve(resolved);
        require(resolved_rendered ==
                    "{\"contract\":\"artifact-store-resolve-v1\",\"action\":\"resolve\","
                    "\"status\":\"RESOLVED\",\"manifest_id\":\"manifest-1\","
                    "\"run_id\":\"run-1\",\"node_id\":\"node-1\","
                    "\"attempt_id\":\"attempt-1\",\"manifest_status\":\"READY\","
                    "\"result_code\":\"OK\",\"objects\":["
                    "{\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
                    "\"size_bytes\":17,\"origin\":\"\"},"
                    "{\"sha256\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\","
                    "\"size_bytes\":0,\"origin\":\"second\"}]}",
                "resolve stable render");
        resolved.objects.clear();
        require(render_resolve(resolved).ends_with("\"objects\":[]}"),
                "empty resolve objects render");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
