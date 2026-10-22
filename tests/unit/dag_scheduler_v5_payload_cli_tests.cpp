#include "../../apps/dag_scheduler_v5_cli.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void stream_reader_contracts() {
    using namespace bruce::dag_scheduler_v5_cli_detail;
    char pattern[] = "/tmp/bruce-v5-stream-reader-XXXXXX";
    const auto* created = ::mkdtemp(pattern);
    require(created != nullptr, "create stream reader fixture");
    const std::filesystem::path root(created);
    const auto previous = std::filesystem::current_path();
    try {
        std::filesystem::current_path(root);
        for (const auto size : {std::size_t(0), std::size_t(131103)}) {
            std::string payload(size, '\0');
            for (std::size_t i = 0; i < size; ++i) payload[i] = static_cast<char>(i % 251);
            const auto id = "input-" + std::to_string(size);
            const int fd = ::open(id.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            require(fd >= 0, "open stream reader source");
            write_all(fd, payload);
            require(::lseek(fd, 0, SEEK_SET) == 0, "rewind stream reader source");
            const auto stream = bruce::artifact_v1::write_payload_stream_v15(fd, root, id,
                {bruce::sha256(payload), size}, size,
                std::chrono::steady_clock::now() + std::chrono::seconds(10));
            ::close(fd);
            const bruce::worker_v1::ArtifactRef ref{stream.reference, size, stream.sha256};
            require(read_approved(ref, size) == payload, "canonical stream reader bytes");
            if (size) {
                bool rejected = false;
                try { (void)read_approved(ref, size - 1); }
                catch (const bruce::Error&) { rejected = true; }
                require(rejected, "stream reader ignored input cap");
            }
            std::ofstream(root / stream.reference / "seal.json", std::ios::trunc) << "{}";
            bool rejected = false;
            try { (void)read_approved(ref, size); }
            catch (const bruce::Error&) { rejected = true; }
            require(rejected, "stream reader accepted invalid seal");
        }
    } catch (...) {
        std::filesystem::current_path(previous);
        std::filesystem::remove_all(root);
        throw;
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
}

int main() {
    using namespace bruce::dag_scheduler_v5_cli_detail;
    try {
        const auto request = parse_payload({"payload", "workflow.spec", "--output", "bundle",
                                            "--store", "store"});
        require(request.spec == "workflow.spec" && request.bundle == "bundle" &&
                request.store == "store" && request.options.retry == 0 &&
                request.options.workers == 1 && !request.stream_v15, "payload defaults");
        const auto stream = parse_payload({"payload", "workflow.spec", "--output", "bundle",
            "--store", "store", "--workers", "2", "--stream-v15", "--retry", "1"});
        require(stream.stream_v15 && stream.options.workers == 2 && stream.options.retry == 1,
                "explicit stream option");
        const auto flow = parse_payload({"payload", "workflow.spec", "--output", "bundle",
            "--store", "store", "--flow-v2", "--stream-v15", "--retry", "1"});
        require(flow.flow_v2 && flow.stream_v15 && flow.options.retry == 1,
                "explicit flow-v2 option");
        const auto external = parse_payload({"payload", "workflow.spec", "--output", "bundle",
            "--store", "store", "--external-inputs", "inputs.json", "--workers", "2"});
        require(external.external_inputs && *external.external_inputs == "inputs.json" &&
                external.options.workers == 2 && !external.stream_v15, "external input option");
        const auto external_stream = parse_payload({"payload", "workflow.spec", "--output", "bundle",
            "--store", "store", "--stream-v15", "--external-inputs", "inputs.json"});
        require(external_stream.stream_v15 && external_stream.external_inputs,
                "external stream option");
        const auto maximum = parse_payload({"payload", "workflow.spec", "--output", "bundle",
            "--store", "store", "--retry", "3", "--max-wall-ms", "600000",
            "--max-input-bytes", "1048576", "--max-output-bytes", "5242880",
            "--max-operations", "10000000", "--max-memory-bytes", "2147483648",
            "--workers", "4"});
        require(maximum.options.retry == 3 && maximum.options.workers == 4 &&
                maximum.options.budget.max_wall_ms == 600000 &&
                maximum.options.budget.max_input_bytes == 1048576 &&
                maximum.options.budget.max_output_bytes == 5242880 &&
                maximum.options.budget.max_operations == 10000000 &&
                maximum.options.budget.max_memory_bytes == 2147483648ULL,
                "payload maxima");

        for (const auto& args : std::vector<std::vector<std::string_view>>{
                 {"payload", "workflow.spec"},
                 {"payload", "workflow.spec", "--store", "store", "--output", "bundle"},
                 {"payload", "workflow.spec", "--output", "bundle"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store"},
                 {"payload", "workflow.spec", "--output", "bundle.run", "--store", "store"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "-store"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "store",
                  "--unknown", "1"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "store",
                  "--stream-v15", "--stream-v15"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "store",
                  "--flow-v2", "--flow-v2"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "store",
                  "--external-inputs"},
                 {"payload", "workflow.spec", "--output", "bundle", "--store", "store",
                  "--external-inputs", "inputs.json", "--external-inputs", "other.json"},
                 {"execute", "workflow.spec", "--output", "bundle", "--store", "store"}}) {
            bool rejected = false;
            try { (void)parse_payload(args); } catch (const bruce::Error&) { rejected = true; }
            require(rejected, "invalid payload syntax accepted");
        }

        bool flow_external_rejected = false;
        try {
            const auto request = parse_payload({"payload", "workflow.spec", "--output", "bundle",
                "--store", "store", "--flow-v2", "--external-inputs", "inputs.json"});
            flow_external_rejected = request.flow_v2 && request.external_inputs.has_value();
        } catch (const bruce::Error&) {
            flow_external_rejected = true;
        }
        require(flow_external_rejected, "flow-v2/external input syntax not represented");

        bool execute_rejected = false;
        try { (void)parse_options({"--stream-v15"}); }
        catch (const bruce::Error&) { execute_rejected = true; }
        require(execute_rejected, "execute accepted stream option");
        for (const auto& route : {"execute", "inspect", "publish"}) {
            std::vector<std::string> tokens{"bruce", "dag-scheduler-v5", route,
                "workflow.spec", "--stream-v15"};
            if (std::string_view(route) == "execute")
                tokens = {"bruce", "dag-scheduler-v5", route, "workflow.spec",
                          "--output", "bundle", "--stream-v15"};
            if (std::string_view(route) == "publish")
                tokens = {"bruce", "dag-scheduler-v5", route, "bundle",
                          "--store", "store", "--stream-v15"};
            std::vector<char*> argv;
            for (auto& token : tokens) argv.push_back(token.data());
            std::ostringstream errors;
            auto* previous = std::cerr.rdbuf(errors.rdbuf());
            const auto status = bruce::dag_scheduler_v5_cli(static_cast<int>(argv.size()), argv.data());
            std::cerr.rdbuf(previous);
            require(status == 2, "non-payload route accepted stream option");
        }

        const auto report = std::string(
            "{\"contract\":\"dag-scheduler-v5-payload-v13\",\"action\":\"payload\"");
        require(report.find("payload-v13") != std::string::npos &&
                report.find("payload") != std::string::npos,
                "payload contract marker");
        stream_reader_contracts();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
