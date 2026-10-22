#include "../../apps/dag_scheduler_v5_cli.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    using namespace bruce::dag_scheduler_v5_cli_detail;
    try {
        const auto request = parse_publish({"publish", "bundle", "--store", "store"});
        require(request.bundle == "bundle" && request.store == "store", "publish parse");
        for (const auto& args : std::vector<std::vector<std::string_view>>{
                 {"publish", "bundle", "--store"},
                 {"publish", "bundle", "--store", "store", "--extra", "x"},
                 {"publish", "bundle.run", "--store", "store"},
                 {"publish", "-bundle", "--store", "store"},
                 {"publish", "bundle", "--store", "-store"},
                 {"inspect", "bundle", "--store", "store"}}) {
            bool rejected = false;
            try { (void)parse_publish(args); } catch (const bruce::Error&) { rejected = true; }
            require(rejected, "invalid publish syntax accepted");
        }

        V5ArtifactPublishResultV1 result;
        result.run_id = "run-123";
        result.artifacts.push_back({"a", "a-1", "artifact/a",
            "v5-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
            {"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789", 12}});
        const auto report = render_publish_report(result);
        require(report ==
            "{\"contract\":\"dag-scheduler-v5-publish-v1\",\"action\":\"publish\","
            "\"status\":\"PUBLISHED\",\"run_id\":\"run-123\",\"manifests\":["
            "{\"node_id\":\"a\",\"attempt_id\":\"a-1\",\"reference\":\"artifact/a\","
            "\"manifest_id\":\"v5-0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
            "\"sha256\":\"abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\","
            "\"size_bytes\":12}]}\n", "publish report changed");
        require(report.find("attempts/") == std::string::npos &&
                report.find("/tmp") == std::string::npos &&
                report.find("payload") == std::string::npos, "publish report leaked private data");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
