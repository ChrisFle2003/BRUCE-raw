#include "bruce/mod/catalog.hpp"
#include "bruce/mods/cockpit/mod.hpp"
#include "bruce/evidence/bundle.hpp"
#include "cockpit_imports.hpp"
#include "cockpit_jobs.hpp"
#include "cockpit_management.hpp"
#include "cockpit_origin.hpp"
#include "cockpit_process.hpp"
#include "cockpit_redis.hpp"
#include "cockpit_server.hpp"
#include "cockpit_state.hpp"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <string>
#include <vector>

#if BRUCE_BUILD_COCKPIT_REDIS && !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] bool rejects(const auto& action) {
    try {
        action();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

#if BRUCE_BUILD_COCKPIT_REDIS
void test_redis_configuration_and_resp_contract() {
    using namespace bruce;
    require(cockpit_state_backend_name(CockpitStateBackendModeV1::local) == "local",
            "local state backend name is stable");
    require(cockpit_state_backend_name(CockpitStateBackendModeV1::redis_metadata) ==
                "redis-metadata",
            "metadata state backend name is stable");
    require(cockpit_state_backend_name(CockpitStateBackendModeV1::redis_full) == "redis-full",
            "full state backend name is stable");
    require(parse_cockpit_state_backend("redis-metadata") ==
                CockpitStateBackendModeV1::redis_metadata,
            "metadata state backend parses");

    const auto defaults = parse_cockpit_redis_url("redis://localhost");
    require(defaults.host == "localhost" && defaults.port == 6379 && defaults.database == 0,
            "Redis URL defaults are stable");
    const auto explicit_values = parse_cockpit_redis_url("redis://redis.example:6380/12");
    require(explicit_values.host == "redis.example" && explicit_values.port == 6380 &&
                explicit_values.database == 12,
            "Redis URL accepts explicit port and database");
    const auto ipv6 = parse_cockpit_redis_url("redis://[::1]:6379/2");
    require(ipv6.host == "::1" && ipv6.port == 6379 && ipv6.database == 2,
            "Redis URL accepts valid bracketed IPv6");
    for (const auto url : {"", "http://localhost", "rediss://localhost", "redis://",
                           "redis://*", "redis://localhost:0", "redis://localhost:65536",
                           "redis://localhost:not-a-port", "redis://localhost/",
                           "redis://localhost/4294967296", "redis://user:secret@localhost",
                           "redis://localhost/path", "redis://localhost?secret",
                           "redis://redis]example", "redis://redis%2fexample",
                           "redis://[not-ipv6]", "redis://[::1]trailing",
                           "redis://foo,bar", "redis://foo;bar", "redis://foo=bar",
                           "redis://foo..bar", "redis://-foo", "redis://foo-",
                           "redis://[1:2]"}) {
        require(rejects([&] { static_cast<void>(parse_cockpit_redis_url(url)); }),
                std::string("invalid Redis URL is rejected: ") + url);
    }
    require(rejects([] { static_cast<void>(parse_cockpit_state_backend("unknown")); }),
            "unknown state backend is rejected");

    CockpitRedisConfigV1 password_config = defaults;
    password_config.password = "secret-not-in-url";
    require(password_config.host.find("secret-not-in-url") == std::string::npos,
            "Redis password is configuration-only");

    const auto simple = decode_cockpit_resp2("+OK\r\n");
    require(simple.type == CockpitRedisValueTypeV1::simple_string && simple.string_value == "OK",
            "RESP simple string decodes");
    const auto integer = decode_cockpit_resp2(":42\r\n");
    require(integer.type == CockpitRedisValueTypeV1::integer && integer.integer_value == 42,
            "RESP integer decodes");
    const auto bulk = decode_cockpit_resp2("$5\r\nhello\r\n");
    require(bulk.type == CockpitRedisValueTypeV1::bulk_string &&
                bulk.string_value == "hello",
            "RESP bulk string decodes");
    const auto nil = decode_cockpit_resp2("$-1\r\n");
    require(nil.type == CockpitRedisValueTypeV1::nil, "RESP nil decodes");
    const auto array = decode_cockpit_resp2("*3\r\n:1\r\n$3\r\nfoo\r\n*-1\r\n");
    require(array.type == CockpitRedisValueTypeV1::array && array.array_value.size() == 3 &&
                array.array_value[0].integer_value == 1 &&
                array.array_value[1].string_value == "foo" &&
                array.array_value[2].type == CockpitRedisValueTypeV1::nil,
            "RESP arrays and nested nil decode");
    require(rejects([] { static_cast<void>(decode_cockpit_resp2("$5\r\nabc\r\n")); }),
            "truncated RESP bulk string is rejected");
    require(rejects([] { static_cast<void>(decode_cockpit_resp2("$999999999\r\n")); }),
            "oversized RESP bulk string is rejected");
    require(rejects([] { static_cast<void>(decode_cockpit_resp2("*1\r\n?bad\r\n")); }),
            "malformed RESP type is rejected");
    require(rejects([] { static_cast<void>(decode_cockpit_resp2("+OK\rX\n\r\n")); }),
            "RESP simple strings reject naked CR/LF bytes");
    require(rejects([] { static_cast<void>(decode_cockpit_resp2("-ERR\nreason\r\n")); }),
            "RESP errors reject naked CR/LF bytes");
}

#if !defined(_WIN32)
void test_redis_timeout_contract() {
    using namespace bruce;
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return;
    int reuse = 1;
    static_cast<void>(::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    if (::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(listener);
        return;
    }
    if (::listen(listener, 1) != 0) {
        ::close(listener);
        return;
    }
    socklen_t address_length = sizeof(address);
    if (::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
        ::close(listener);
        return;
    }
    std::thread server([listener] {
        const int accepted = ::accept(listener, nullptr, nullptr);
        if (accepted >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ::close(accepted);
        }
        ::close(listener);
    });
    auto config = parse_cockpit_redis_url(
        "redis://127.0.0.1:" + std::to_string(ntohs(address.sin_port)));
    config.timeout = std::chrono::milliseconds(25);
    const auto started = std::chrono::steady_clock::now();
    bool failed = false;
    try {
        CockpitRedisClientV1 client(std::move(config));
        static_cast<void>(client.command({"PING"}));
    } catch (const std::exception&) {
        failed = true;
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    server.join();
    require(failed, "Redis command timeout is reported as unavailable");
    require(elapsed < std::chrono::seconds(1),
            "Redis command timeout remains bounded");
}
#endif
#endif

void test_endpoint_contract() {
    using namespace bruce;
    const auto loopback = parse_cockpit_endpoint("http://127.0.0.1:8123");
    require(loopback.scheme == "http" && loopback.host == "127.0.0.1" &&
                loopback.port == 8123, "loopback endpoint is parsed");
    const auto hostname = parse_cockpit_endpoint("https://cockpit.example");
    require(hostname.host == "cockpit.example" && !hostname.port,
            "hostname endpoint preserves absent port");
    require(parse_cockpit_endpoint("http://bruce0x.lan").host == "bruce0x.lan",
            "non-numeric hostname with embedded hex marker is accepted");
    for (const auto host : {"example.0xg", "example.0xyz", "0xyz"}) {
        require(parse_cockpit_endpoint(std::string("http://") + host).host == host,
                std::string("non-hex DNS hostname is accepted: ") + host);
    }
    require(parse_cockpit_endpoint("https://[::1]:443").host == "::1",
            "bracketed IPv6 endpoint is parsed");
    require(cockpit_endpoint_origin("https", "::1", 8123) == "https://[::1]:8123",
            "IPv6 origin uses brackets");
    require(cockpit_endpoint_origin("https", "Cockpit.Example", 443) == "https://cockpit.example",
            "Origin uses browser hostname and default-port serialization");
    for (const auto endpoint : {"http://host/path", "http://host?query", "host:80",
                               "ftp://host", "http://0.0.0.0", "http://[::]",
                               "http://*", "http://host:0", "http://host:65536",
                               "http://host:abc", "http://host:", "http://user@host",
                               "http://host#fragment", "http://[not-ipv6]",
                               "http://0.0.0.00", "http://0000", "http://0x0",
                               "http://0x0.0x0.0x0.0x0", "http://0.0.0x0",
                               "http://0x7f.0.0.1", "http://0x.0.0.0",
                               "http://0x.0x.0x.0x", "http://example.0x1",
                               "http://EXAMPLE.0X1", "http://127.00.0.1",
                               "http://0127.0.0.1", "http://a.123",
                               "http://example.0x", "http://[::ffff:127.0.0.1]",
                               "http://[::127.0.0.1]", "http://[::ffff:7f00:1]",
                               "http://[::7f00:1]"}) {
        require(rejects([&] { static_cast<void>(parse_cockpit_endpoint(endpoint)); }),
                std::string("unsafe endpoint is rejected: ") + endpoint);
    }
}

void test_state_store_contract() {
    using namespace bruce;
    auto transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto state = std::make_shared<CockpitStateStoreV1>(transport);

    CockpitJobMetaV1 first{
        .id = {},
        .state = "QUEUED",
        .directory = "/tmp/job-000001",
        .artifacts_directory = "/tmp/job-000001/artifacts",
        .executable = "/usr/bin/bruce",
        .argv = {"research"},
        .read_only_inputs = {},
        .return_code = std::nullopt,
    };
    first.id = state->allocate_job_id();
    state->put_job_meta(first);
    auto second = first;
    second.id = state->allocate_job_id();
    second.state = "SUCCEEDED";
    state->put_job_meta(second);
    const auto listed = state->list_job_meta();
    require(listed.size() == 2 && listed[0].id == "job-000001" &&
                listed[1].id == "job-000002" && listed[0].schema_version == 1 &&
                listed[0].contract == "bruce-cockpit-redis-v1",
            "Redis state records have stable IDs, schema and sorted snapshots");
    auto restarted = std::make_shared<CockpitStateStoreV1>(transport);
    require(restarted->list_job_meta().size() == 2 &&
                restarted->list_job_meta()[1].state == "SUCCEEDED",
            "Redis state records rehydrate after store restart");

    transport->fail_next_publication();
    auto failed = second;
    failed.state = "FAILED";
    bool rejected = false;
    try {
        state->put_job_meta(failed);
    } catch (const std::exception&) {
        rejected = true;
    }
    const auto after_failure = restarted->list_job_meta();
    require(rejected && after_failure.size() == 2 && after_failure[1].state == "SUCCEEDED",
            "failed publication leaves the last complete record and index visible");

    auto no_gap_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto no_gap_state = std::make_shared<CockpitStateStoreV1>(no_gap_transport);
    auto no_gap_record = first;
    no_gap_record.id = no_gap_state->allocate_job_id();
    no_gap_transport->fail_next_publication();
    require(rejects([&] { no_gap_state->put_job_meta(no_gap_record); }),
            "failed first publication is rejected");
    const auto no_gap_next = no_gap_state->allocate_job_id();
    require(no_gap_next == "job-000001",
            "failed first publication rolls back the reserved sequence: " + no_gap_next);

    CockpitImportMetaV1 imported{
        .id = state->allocate_import_id(),
        .name = "bundle.bruce",
        .path = "/tmp/import-000001/payload.bruce",
        .bytes = 8,
        .digest = std::string(64, 'a'),
    };
    state->put_import_meta(imported);
    require(state->list_import_meta().front().id == "import-000001" &&
                state->key_prefix() == "bruce:cockpit:v1:",
            "import metadata uses the versioned Redis key prefix");

    auto full_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto full_state = std::make_shared<CockpitStateStoreV1>(full_transport, true);
    const auto full_job_id = full_state->allocate_job_id();
    CockpitJobMetaV1 terminal = first;
    terminal.id = full_job_id;
    terminal.state = "SUCCEEDED";
    const std::vector<CockpitBlobV1> terminal_blobs{
        {.kind = "stdout", .relative_path = {}, .bytes = std::string("out\r\n\0", 6)},
        {.kind = "stderr", .relative_path = {}, .bytes = std::string("err\n", 4)},
        {.kind = "artifact", .relative_path = "nested/result.bruce", .bytes = "BRUCE_ARTIFACT"},
    };
    full_state->publish_terminal_job(terminal, terminal_blobs);
    require(full_state->read_blob(full_job_id, "stdout", "").value() ==
                std::string("out\r\n\0", 6) &&
                full_state->read_blob(full_job_id, "artifact", "nested/result.bruce").value() ==
                "BRUCE_ARTIFACT" &&
                full_state->list_blobs(full_job_id, "artifact").size() == 1,
            "redis-full blobs round-trip byte-identically with safe relative paths");
    const auto empty_log_job_id = full_state->allocate_job_id();
    auto empty_log_terminal = terminal;
    empty_log_terminal.id = empty_log_job_id;
    full_state->publish_terminal_job(empty_log_terminal, {
        {.kind = "stdout", .relative_path = {}, .bytes = {}},
        {.kind = "stderr", .relative_path = {}, .bytes = {}},
    });
    require(full_state->read_blob(empty_log_job_id, "stdout", "").has_value() &&
                full_state->read_blob(empty_log_job_id, "stdout", "")->empty() &&
                full_state->read_blob(empty_log_job_id, "stderr", "").has_value() &&
                full_state->read_blob(empty_log_job_id, "stderr", "")->empty(),
            "redis-full preserves valid empty terminal logs");
    require(rejects([&] {
        full_state->put_blob(full_job_id, "artifact", "../escape", "bad");
    }), "redis-full rejects artifact traversal before key creation");
    require(rejects([&] {
        full_state->put_blob(full_job_id, "artifact", "oversized",
                             std::string(cockpit_full_blob_max_bytes_v1 + 1, 'x'));
    }), "redis-full rejects blobs over the explicit size limit");
    full_transport->fail_next_publication();
    auto failed_terminal = terminal;
    failed_terminal.state = "FAILED";
    require(rejects([&] {
        full_state->publish_terminal_job(failed_terminal, terminal_blobs);
    }), "redis-full failed terminal publication is rejected atomically");
    require(full_state->list_job_meta().front().state == "SUCCEEDED" &&
                full_state->read_blob(full_job_id, "stdout", "").value() ==
                    std::string("out\r\n\0", 6),
            "redis-full failed publication preserves the complete terminal record and blobs");

    const auto full_import_root = std::filesystem::temp_directory_path() /
        "bruce-cockpit-full-import-contract";
    std::error_code full_import_cleanup;
    std::filesystem::remove_all(full_import_root, full_import_cleanup);
    CockpitImportStoreV1 full_import_store(full_import_root, full_state);
    const auto full_import = full_import_store.import_bundle(
        "full.bruce", "BRUCE_FULL_PAYLOAD");
    const auto materialized = full_import_store.materialize_for_job(full_import.id);
    std::ifstream materialized_input(materialized, std::ios::binary);
    const std::string materialized_bytes(
        (std::istreambuf_iterator<char>(materialized_input)), {});
    require(full_import_store.full_mode() && materialized_bytes == "BRUCE_FULL_PAYLOAD" &&
                full_import_store.snapshot().front().id == full_import.id,
            "redis-full import payloads materialize byte-identically for child jobs");
    const auto full_snapshot = full_import_store.materialize_snapshot_for_job(full_import.id);
    std::ifstream full_snapshot_input(full_snapshot, std::ios::binary);
    require(std::string((std::istreambuf_iterator<char>(full_snapshot_input)), {}) ==
                "BRUCE_FULL_PAYLOAD",
            "redis-full import actions also bind an atomically written job snapshot");
    {
        std::ofstream changed_file(materialized, std::ios::binary | std::ios::trunc);
        changed_file << "BRUCE_LOCAL_CHANGE";
    }
    require(full_state->read_blob(full_import.id, "payload", "").value() ==
                "BRUCE_FULL_PAYLOAD",
            "changing a redis-full materialization cannot change the published blob");
    require(!full_state->read_blob("import-000999", "payload", ""),
            "redis-full missing blobs remain explicit and do not synthesize local data");
    full_transport->fail_next_publication();
    require(rejects([&] {
        static_cast<void>(full_import_store.import_bundle("failed.bruce", "BRUCE_FAILED"));
    }), "redis-full failed import publication is rejected atomically");
    require(full_import_store.snapshot().size() == 1 &&
                full_state->list_import_meta().size() == 1 &&
                !full_state->read_blob("import-000002", "payload", ""),
            "redis-full failed import leaves no partial metadata or payload blob");
    std::filesystem::remove(materialized, full_import_cleanup);
    std::filesystem::remove_all(full_snapshot.parent_path(), full_import_cleanup);
    require(!full_import_cleanup,
            "redis-full materialization can be removed before store restart");
    auto full_restarted_state = std::make_shared<CockpitStateStoreV1>(full_transport, true);
    CockpitImportStoreV1 full_import_restart(full_import_root, full_restarted_state);
    const auto rematerialized = full_import_restart.materialize_for_job(full_import.id);
    std::ifstream rematerialized_input(rematerialized, std::ios::binary);
    const auto recovered_import = full_import_restart.find(full_import.id);
    require(full_import_restart.snapshot().size() == 1 && recovered_import.has_value() &&
                recovered_import->id == full_import.id &&
                recovered_import->path == full_import.path &&
                recovered_import->digest == full_import.digest &&
                recovered_import->bytes == full_import.bytes &&
                std::string((std::istreambuf_iterator<char>(rematerialized_input)), {}) ==
                    "BRUCE_FULL_PAYLOAD",
            "redis-full metadata and blob payload rehydrate after state and import store restart");
    std::filesystem::remove_all(full_import_root, full_import_cleanup);
}

void test_job_contract(const std::filesystem::path& executable) {
    using namespace bruce;
    require(cockpit_job_state_name(CockpitJobStateV1::queued) == "QUEUED",
            "queued state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::running) == "RUNNING",
            "running state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::succeeded) == "SUCCEEDED",
            "succeeded state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::failed) == "FAILED",
            "failed state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::cancel_requested) == "CANCEL_REQUESTED",
            "cancel-requested state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::cancelled) == "CANCELLED",
            "cancelled state is stable");
    require(cockpit_job_state_name(CockpitJobStateV1::unresolved_after_server_exit) ==
                "UNRESOLVED_AFTER_SERVER_EXIT",
            "unresolved state is stable");
    require(cockpit_command_allowed("research") && cockpit_command_allowed("replay") &&
                cockpit_command_allowed("language") && cockpit_command_allowed("nanocell"),
            "public and enabled mod commands are catalogued");
    require(!cockpit_command_allowed("rule") && !cockpit_command_allowed("shell"),
            "mutable and foreign commands are excluded");

    const auto base = std::filesystem::temp_directory_path() / "bruce-cockpit-job-contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(base, cleanup_error);
    std::filesystem::create_directories(base);
    const auto input = base / "input.csv";
    std::ofstream(input) << "1,2\n";
    const auto job_directory = base / "job-000001";
    const auto valid = [&] {
        return CockpitJobRequestV1{
            .executable = executable,
            .argv = {"research", "--output", "result.bruce"},
            .job_directory = job_directory,
            .read_only_inputs = {input},
        };
    };
    const auto normalized = validate_cockpit_job_request(valid());
    require(normalized.argv[2] == (job_directory / "artifacts" / "result.bruce").string(),
            "relative output is canonicalized into the job artifacts directory");
    require(normalized.read_only_inputs.front() == std::filesystem::absolute(input),
            "read-only input is canonicalized");

    require(rejects([&] {
        auto request = valid();
        request.argv = {"research", ";"};
        static_cast<void>(validate_cockpit_job_request(std::move(request)));
    }), "shell token is rejected");
    require(rejects([&] {
        auto request = valid();
        request.argv = {"research", std::string("nul\0token", 9)};
        static_cast<void>(validate_cockpit_job_request(std::move(request)));
    }), "NUL argument is rejected");
    require(rejects([&] {
        auto request = valid();
        request.executable = "/bin/sh";
        static_cast<void>(validate_cockpit_job_request(std::move(request)));
    }), "foreign executable is rejected");
    require(rejects([&] {
        auto request = valid();
        request.argv = {"research", "--output", (base / "outside.bruce").string()};
        static_cast<void>(validate_cockpit_job_request(std::move(request)));
    }), "external output is rejected");
    require(rejects([&] {
        auto request = valid();
        request.argv = {"rule", "activate"};
        static_cast<void>(validate_cockpit_job_request(std::move(request)));
    }), "rule activation is rejected");

    CockpitJobStoreV1 store(base / "runs" / "cockpit");
    auto first = store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research", "--output", "result.bruce"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    const auto second = store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"inspect-run", input.string()},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    require(first.id == "job-000001" && second.id == "job-000002",
            "job IDs are fresh and sequential");
    require(std::filesystem::is_directory(first.directory / "artifacts"),
            "job artifacts directory is created");
    const auto artifacts_directory = first.artifacts_directory;
    std::ofstream(artifacts_directory / "z-result.bruce", std::ios::binary) << "12345";
    std::ofstream(artifacts_directory / "a-result.bruce", std::ios::binary) << "12";
    std::filesystem::create_directory(artifacts_directory / "nested");
    std::ofstream(artifacts_directory / "nested" / "hidden.bruce") << "nested";
    const auto outside_artifact = base / "outside.bruce";
    std::ofstream(outside_artifact) << "outside";
    std::error_code symlink_error;
    std::filesystem::create_symlink("../../../../outside.bruce",
                                    artifacts_directory / "escape.bruce", symlink_error);
    std::error_code local_symlink_error;
    std::filesystem::create_symlink("a-result.bruce",
                                    artifacts_directory / "alias.bruce", local_symlink_error);
    const auto artifacts = list_cockpit_artifacts(artifacts_directory);
    require(artifacts.size() == 2, "only direct regular artifact files are listed");
    require(artifacts[0].relative_path == "a-result.bruce" && artifacts[0].bytes == 2 &&
                artifacts[1].relative_path == "z-result.bruce" && artifacts[1].bytes == 5,
            "artifact paths are sorted and byte counts are exact");
    for (const auto& artifact : artifacts) {
        const std::filesystem::path relative(artifact.relative_path);
        require(!relative.is_absolute() && !relative.has_parent_path() &&
                    artifact.relative_path == relative.generic_string(),
                "artifact paths are direct generic relative paths without escape");
    }
    require(list_cockpit_artifacts(base / "missing-artifacts").empty(),
            "missing artifacts directory has an empty listing");
    const auto raw_log = base / "raw.log";
    const std::string raw_log_bytes("stdout\r\n\0stderr\n", 16);
    {
        std::ofstream raw_output(raw_log, std::ios::binary);
        raw_output.write(raw_log_bytes.data(), static_cast<std::streamsize>(raw_log_bytes.size()));
    }
    require(read_cockpit_log(raw_log) == raw_log_bytes,
            "raw logs preserve CRLF, NUL, and trailing newline bytes");
    require(read_cockpit_log(base / "missing.log").empty(),
            "missing logs return empty raw text");
    require(std::filesystem::is_regular_file(first.directory / "job.json") &&
                std::filesystem::is_regular_file(first.directory / "command.argv.json") &&
                std::filesystem::is_regular_file(first.directory / "stdout.log") &&
                std::filesystem::is_regular_file(first.directory / "stderr.log"),
            "job metadata and raw log files are persisted");
    std::ifstream command(first.directory / "command.argv.json");
    const std::string command_text((std::istreambuf_iterator<char>(command)), {});
    require(command_text.find("research") != std::string::npos &&
                command_text.find("result.bruce") != std::string::npos,
            "confirmed argv is persisted");
    store.request_cancel(first.id);
    require(store.snapshot().front().state == CockpitJobStateV1::cancel_requested,
            "cancel request is an explicit persisted state");
    store.update_state(first.id, CockpitJobStateV1::cancelled);
    require(store.load_completed().size() == 1 &&
                store.load_completed().front().state == CockpitJobStateV1::cancelled,
            "completed store excludes queued jobs and retains cancellation");
    CockpitJobStoreV1 reloaded(base / "runs" / "cockpit");
    require(reloaded.snapshot().size() == 2 &&
                reloaded.snapshot().front().state == CockpitJobStateV1::cancelled,
            "job status survives store reload");
    require(reloaded.snapshot().front().read_only_inputs.size() == 1 &&
                reloaded.snapshot().front().read_only_inputs.front().digest ==
                    first.read_only_inputs.front().digest,
            "read-only source digest survives store reload");
    store.update_state(second.id, CockpitJobStateV1::running);
    CockpitJobStoreV1 after_restart(base / "runs" / "cockpit");
    require(after_restart.snapshot().back().state ==
                CockpitJobStateV1::unresolved_after_server_exit,
            "persisted running jobs are honest after a server restart");

    const auto metadata_base = base / "metadata";
    auto metadata_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto metadata_state = std::make_shared<CockpitStateStoreV1>(metadata_transport);
    CockpitJobStoreV1 metadata_store(metadata_base / "runs" / "cockpit", metadata_state);
    const auto metadata_job = metadata_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research", "--output", "local-result.bruce"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    const std::string local_stdout("stdout\r\n\0bytes", 14);
    {
        std::ofstream output(metadata_job.directory / "stdout.log", std::ios::binary | std::ios::trunc);
        output.write(local_stdout.data(), static_cast<std::streamsize>(local_stdout.size()));
    }
    metadata_store.update_state(metadata_job.id, CockpitJobStateV1::succeeded, 0);
    CockpitJobStoreV1 metadata_restart(metadata_base / "runs" / "cockpit", metadata_state);
    std::ifstream restored_stdout(metadata_job.directory / "stdout.log", std::ios::binary);
    const std::string restored_bytes((std::istreambuf_iterator<char>(restored_stdout)), {});
    require(metadata_restart.snapshot().front().state == CockpitJobStateV1::succeeded &&
                restored_bytes == local_stdout,
            "metadata mode rehydrates job state without changing local log bytes");

    const auto transaction_base = base / "transaction";
    auto transaction_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto transaction_state = std::make_shared<CockpitStateStoreV1>(transaction_transport);
    CockpitJobStoreV1 transaction_store(transaction_base, transaction_state);
    const auto transaction_job = transaction_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    std::ifstream before_transaction(transaction_job.directory / "job.json");
    const std::string before_transaction_text(
        (std::istreambuf_iterator<char>(before_transaction)), {});
    transaction_transport->fail_next_publication();
    require(rejects([&] {
        transaction_store.update_state(transaction_job.id, CockpitJobStateV1::failed, 9);
    }), "failed state publication is rejected");
    std::ifstream after_transaction(transaction_job.directory / "job.json");
    const std::string after_transaction_text(
        (std::istreambuf_iterator<char>(after_transaction)), {});
    require(transaction_store.snapshot().front().state == CockpitJobStateV1::queued &&
                before_transaction_text == after_transaction_text,
            "failed state publication preserves memory and local metadata state");

    const auto validation_base = base / "validation";
    auto validation_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto validation_state = std::make_shared<CockpitStateStoreV1>(validation_transport);
    CockpitJobStoreV1 validation_store(validation_base, validation_state);
    validation_transport->fail_next_allocation();
    std::string validation_error;
    try {
        static_cast<void>(validation_store.create(CockpitJobRequestV1{
            .executable = "/bin/sh",
            .argv = {"research"},
            .job_directory = {},
            .read_only_inputs = {},
        }));
    } catch (const std::exception& error) {
        validation_error = error.what();
    }
    require(validation_error == "CockpitExecutableRejected",
            "invalid job admission wins over Redis availability failure");
    validation_transport->clear_fail_next_allocation();
    const auto admitted_after_rejection = validation_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {},
    });
    require(admitted_after_rejection.id == "job-000001",
            "invalid job admission does not leak its Redis sequence reservation");

    const auto full_job_base = base / "full-job";
    auto full_job_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto full_job_state = std::make_shared<CockpitStateStoreV1>(full_job_transport, true);
    CockpitJobStoreV1 full_job_store(full_job_base, full_job_state);
    const auto full_job = full_job_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    const std::string full_stdout("full\r\n\0stdout", 12);
    {
        std::ofstream output(full_job.directory / "stdout.log", std::ios::binary | std::ios::trunc);
        output.write(full_stdout.data(), static_cast<std::streamsize>(full_stdout.size()));
    }
    std::ofstream(full_job.directory / "stderr.log", std::ios::binary) << "full stderr";
    std::ofstream(full_job.artifacts_directory / "result.bruce", std::ios::binary)
        << "BRUCE_FULL_RESULT";
    full_job_store.update_state(full_job.id, CockpitJobStateV1::succeeded, 0);
    require(full_job_state->read_blob(full_job.id, "stdout", "").value() == full_stdout &&
                full_job_state->read_blob(full_job.id, "artifact", "result.bruce").value() ==
                    "BRUCE_FULL_RESULT" &&
                !std::filesystem::exists(full_job.directory),
            "redis-full terminal publication preserves blobs and removes the local workspace");
    CockpitJobStoreV1 full_job_restart(full_job_base, full_job_state);
    require(full_job_restart.snapshot().size() == 1 &&
                full_job_restart.snapshot().front().state == CockpitJobStateV1::succeeded &&
                full_job_state->read_blob(full_job.id, "artifact", "result.bruce").value() ==
                "BRUCE_FULL_RESULT",
            "redis-full terminal jobs rehydrate after local workspace removal");

    const auto restart_base = base / "full-restart";
    auto restart_transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto restart_state = std::make_shared<CockpitStateStoreV1>(restart_transport, true);
    CockpitJobStoreV1 restart_store(restart_base, restart_state);
    const auto queued = restart_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    const auto running = restart_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    const auto cancelling = restart_store.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"research"},
        .job_directory = {},
        .read_only_inputs = {input},
    });
    restart_store.update_state(running.id, CockpitJobStateV1::running);
    restart_store.update_state(cancelling.id, CockpitJobStateV1::cancel_requested);
    CockpitJobStoreV1 restarted_jobs(restart_base, restart_state);
    const auto restarted_snapshot = restarted_jobs.snapshot();
    const auto restarted_state = [&](const std::string& id) {
        const auto found = std::find_if(restarted_snapshot.begin(), restarted_snapshot.end(),
            [&](const CockpitJobV1& job) { return job.id == id; });
        return found == restarted_snapshot.end()
            ? CockpitJobStateV1::succeeded : found->state;
    };
    require(restarted_state(queued.id) == CockpitJobStateV1::queued &&
                restarted_state(running.id) == CockpitJobStateV1::unresolved_after_server_exit &&
                restarted_state(cancelling.id) == CockpitJobStateV1::unresolved_after_server_exit,
            "redis-full restart preserves queued jobs and marks active jobs unresolved");
    std::filesystem::remove_all(base, cleanup_error);
}

void test_process_contract(const std::filesystem::path& fixture) {
    const auto base = std::filesystem::temp_directory_path() / "bruce-cockpit-process-contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(base, cleanup_error);
    std::filesystem::create_directories(base);

    const auto stdout_path = base / "success.stdout";
    const auto stderr_path = base / "success.stderr";
    auto success = bruce::CockpitChildProcessV1::start(
        fixture, {"--success", "exact-argv-token"}, stdout_path, stderr_path);
    for (int attempt = 0; attempt != 200 && !success.poll(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(success.exit_status().has_value(), "successful child has an exit status");
    require(success.exit_status()->return_code == 0 && !success.exit_status()->signaled,
            "successful child preserves its zero return code");
    require(std::ifstream(stdout_path).good() && std::ifstream(stderr_path).good(),
            "child stdout and stderr logs are created separately");
    std::ifstream success_stdout(stdout_path);
    const std::string stdout_text((std::istreambuf_iterator<char>(success_stdout)), {});
    std::ifstream success_stderr(stderr_path);
    const std::string stderr_text((std::istreambuf_iterator<char>(success_stderr)), {});
    require(stdout_text.find("exact-argv-token") != std::string::npos,
            "pre-separated argv reaches the child exactly");
    require(stdout_text.find("fixture stderr") == std::string::npos &&
                stderr_text.find("fixture stderr") != std::string::npos,
            "stdout and stderr remain raw and separated");

    const auto failure_out = base / "failure.stdout";
    const auto failure_err = base / "failure.stderr";
    auto failure = bruce::CockpitChildProcessV1::start(
        fixture, {"--failure"}, failure_out, failure_err);
    for (int attempt = 0; attempt != 200 && !failure.poll(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(failure.exit_status().has_value() &&
                failure.exit_status()->return_code == 7 && !failure.exit_status()->signaled,
            "failed child preserves its nonzero return code");

    const auto cancel_out = base / "cancel.stdout";
    const auto cancel_err = base / "cancel.stderr";
    auto cancelled = bruce::CockpitChildProcessV1::start(
        fixture, {"--wait-for-sigint"}, cancel_out, cancel_err);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(!cancelled.poll(), "SIGINT fixture is still running before cancellation");
    cancelled.request_interrupt();
    require(cancelled.interrupt_requested(), "cancellation is recorded before child exit");
    for (int attempt = 0; attempt != 200 && !cancelled.poll(); ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    require(cancelled.exit_status().has_value() &&
                cancelled.exit_status()->return_code == 130,
            "SIGINT cancellation preserves the child's explicit return code");

    std::filesystem::remove_all(base, cleanup_error);
}

void test_import_contract(const std::filesystem::path& executable) {
    using namespace bruce;
    const auto base = std::filesystem::temp_directory_path() / "bruce-cockpit-import-contract";
    std::error_code cleanup_error;
    std::filesystem::remove_all(base, cleanup_error);
    const std::string payload = "BRUCE_TEST_IMPORT_V1\nimported\n";
    CockpitImportStoreV1 store(base / "imports");
    require(rejects([&] { static_cast<void>(store.import_bundle("../escape.bruce", payload)); }),
            "import traversal name is rejected");
    require(rejects([&] { static_cast<void>(store.import_bundle("invalid.txt", payload)); }),
            "import extension is restricted to .bruce");
    require(rejects([&] { static_cast<void>(store.import_bundle("invalid.bruce", "not-bruce")); }),
            "import format is checked before storage");
    const auto imported = store.import_bundle("demo.bruce", payload);
    std::ifstream imported_file(imported.path, std::ios::binary);
    const std::string imported_bytes((std::istreambuf_iterator<char>(imported_file)), {});
    require(imported.id == "import-000001" && imported.name == "demo.bruce" &&
                imported.bytes == payload.size() && imported_bytes == payload &&
                imported.digest.size() == 64 && imported.manifest_version == 1 &&
                std::filesystem::is_regular_file(imported.path.parent_path() / "manifest.v1"),
            "import stores exact bytes, metadata, digest, and a versioned manifest");
    const auto listed = store.snapshot();
    require(listed.size() == 1 && listed.front().id == imported.id,
            "import listing is stable");
    CockpitImportStoreV1 reloaded(base / "imports");
    const auto found = reloaded.find(imported.id);
    require(found.has_value() && found->name == imported.name &&
                found->digest == imported.digest && found->bytes == imported.bytes &&
                found->manifest_version == 1,
            "import manifest metadata survives reload");
    const auto local_snapshot = store.materialize_snapshot_for_job(imported.id);
    std::ifstream local_snapshot_input(local_snapshot, std::ios::binary);
    require(std::string((std::istreambuf_iterator<char>(local_snapshot_input)), {}) == payload,
            "local import action snapshots the manifest-bound bytes");
    {
        std::ofstream changed_file(imported.path, std::ios::binary | std::ios::trunc);
        changed_file << "BRUCE_SNAPSHOT_SOURCE_CHANGED\n";
    }
    std::ifstream retained_snapshot_input(local_snapshot, std::ios::binary);
    require(std::string((std::istreambuf_iterator<char>(retained_snapshot_input)), {}) == payload,
            "changing the original path cannot change the job snapshot");
    std::filesystem::remove_all(local_snapshot.parent_path(), cleanup_error);
    require(rejects([&] { static_cast<void>(store.materialize_snapshot_for_job(imported.id)); }),
            "changed local bytes fail the manifest integrity check before job creation");
    const std::string changed_payload = "BRUCE_TEST_IMPORT_V1\nchanged local bytes\n";
    {
        std::ofstream changed_file(imported.path, std::ios::binary | std::ios::trunc);
        changed_file << changed_payload;
    }
    CockpitImportStoreV1 changed_local_store(base / "imports");
    const auto changed_local = changed_local_store.find(imported.id);
    require(changed_local.has_value() && changed_local->path == imported.path &&
                changed_local->bytes == imported.bytes &&
                changed_local->digest == imported.digest &&
                changed_local->digest != sha256(changed_payload) &&
                changed_local->manifest_version == 1,
            "local reload retains manifest bytes and digest while the path changes");

    const auto metadata_base = base / "metadata";
    auto transport = std::make_shared<CockpitRedisFakeTransportV1>();
    auto state = std::make_shared<CockpitStateStoreV1>(transport);
    CockpitImportStoreV1 metadata_store(metadata_base / "imports", state);
    const auto metadata_import = metadata_store.import_bundle("metadata.bruce", payload);
    transport->fail_next_publication();
    require(rejects([&] {
        static_cast<void>(metadata_store.import_bundle("failed.bruce", payload));
    }), "failed import publication is rejected");
    CockpitImportV1 metadata_retry;
    try {
        metadata_retry = metadata_store.import_bundle("retry.bruce", payload);
    } catch (const std::exception& error) {
        throw;
    }
    require(metadata_retry.id == "import-000002",
            "failed import publication does not leak its sequence reservation");
    std::ifstream metadata_file(metadata_import.path, std::ios::binary);
    const std::string metadata_bytes((std::istreambuf_iterator<char>(metadata_file)), {});
    CockpitImportStoreV1 metadata_restart(metadata_base / "imports", state);
    const auto metadata_found = metadata_restart.find(metadata_import.id);
    require(metadata_found.has_value() && metadata_found->digest == metadata_import.digest &&
                metadata_found->manifest_version == 1 && metadata_bytes == payload,
            "metadata mode rehydrates versioned manifest state while preserving local payload bytes");
    {
        std::ofstream changed_file(metadata_import.path, std::ios::binary | std::ios::trunc);
        changed_file << changed_payload;
    }
    CockpitImportStoreV1 changed_metadata_store(metadata_base / "imports",
                                                 std::make_shared<CockpitStateStoreV1>(transport));
    const auto changed_metadata = changed_metadata_store.find(metadata_import.id);
    std::ifstream changed_metadata_file(metadata_import.path, std::ios::binary);
    const std::string changed_metadata_bytes(
        (std::istreambuf_iterator<char>(changed_metadata_file)), {});
    require(changed_metadata.has_value() && changed_metadata->path == metadata_import.path &&
                changed_metadata->digest == metadata_import.digest &&
                changed_metadata_bytes == changed_payload &&
                changed_metadata_store.materialize_for_job(metadata_import.id) ==
                    metadata_import.path,
            "redis-metadata restart retains manifest metadata while legacy path access remains explicit");
    require(rejects([&] {
        static_cast<void>(changed_metadata_store.materialize_snapshot_for_job(metadata_import.id));
    }), "redis-metadata changed bytes fail manifest integrity before job creation");
    CockpitJobStoreV1 metadata_jobs(metadata_base / "runs" / "cockpit",
                                    std::make_shared<CockpitStateStoreV1>(transport));
    const auto metadata_action = metadata_jobs.create(CockpitJobRequestV1{
        .executable = executable,
        .argv = {"inspect-run", metadata_import.path.string()},
        .job_directory = {},
        .read_only_inputs = {changed_metadata_store.materialize_for_job(metadata_import.id)},
    });
    require(metadata_action.read_only_inputs.size() == 1 &&
                metadata_action.read_only_inputs.front().path == metadata_import.path &&
                metadata_action.read_only_inputs.front().digest == sha256(changed_payload) &&
                metadata_action.read_only_inputs.front().digest != metadata_import.digest,
            "redis-metadata job records the changed path bytes, not persisted import digest");
    std::filesystem::remove_all(base, cleanup_error);
}

} // namespace

int main(int argc, char** argv) {
    test_state_store_contract();
#if BRUCE_BUILD_COCKPIT_REDIS
    test_redis_configuration_and_resp_contract();
#if !defined(_WIN32)
    test_redis_timeout_contract();
#endif
#endif
    test_endpoint_contract();
    (void)argc;
    const auto& manifest = bruce::cockpit_mod_manifest_v1();
    require(manifest.id == "cockpit", "Cockpit manifest has the stable ID");
    require(manifest.version == "cockpit-mod-v1", "Cockpit manifest has the V1 version");
    require(manifest.api_version == bruce::mod_api_version_v1, "Cockpit uses the stable Mod API");
    require(manifest.capabilities.size() == 1 &&
                manifest.capabilities.front() == bruce::ModCapabilityV1::presentation,
            "Cockpit manifest exposes only presentation capability");
    bruce::validate_mod_manifest(manifest);

    const auto& catalog = bruce::builtin_mod_catalog_v1();
    require(catalog.find("cockpit") != nullptr, "Cockpit is discoverable in the enabled catalog");
    const auto& dashboard = bruce::cockpit_dashboard_html();
    require(dashboard.find("BRUCE Cockpit") != std::string::npos,
            "Cockpit dashboard is compiled into the mod");
    require(dashboard.find("/api/v1/health") != std::string::npos &&
                dashboard.find("/api/v1/jobs") != std::string::npos,
            "dashboard consumes the management contracts");
    require(dashboard.find("id=\"health\"") != std::string::npos &&
                dashboard.find("id=\"jobs\"") != std::string::npos &&
                dashboard.find("id=\"job-detail\"") != std::string::npos &&
                dashboard.find("id=\"job-form\"") != std::string::npos,
            "dashboard exposes health, job list, detail, and create regions");
    require(dashboard.find("data-action=\"inspect\"") != std::string::npos &&
                dashboard.find("data-action=\"assess\"") != std::string::npos &&
                dashboard.find("data-action=\"replay\"") != std::string::npos &&
                dashboard.find("data-action=\"gate\"") != std::string::npos &&
                dashboard.find("data-action=\"cancel\"") != std::string::npos,
            "dashboard exposes explicit follow-up action controls");
    require(dashboard.find("id=\"job-command\"") != std::string::npos &&
                dashboard.find("id=\"job-args\"") != std::string::npos,
            "dashboard exposes explicit command and token inputs");
    const auto selection_start = dashboard.find("const selectJob = async");
    const auto selection_end = dashboard.find("const refresh = async", selection_start);
    require(selection_start != std::string::npos && selection_end != std::string::npos,
            "job selection has a bounded independent region");
    const auto selection = dashboard.substr(selection_start, selection_end - selection_start);
    require(dashboard.find("let jobSelectionGeneration = 0;") != std::string::npos &&
                selection.find("const generation = ++jobSelectionGeneration;") != std::string::npos,
            "each job selection supersedes all earlier pending selections");
    const auto job_read = selection.find("const job = await getJson(");
    const auto artifacts_read = selection.find("const artifacts = await getJson(");
    const auto logs_read = selection.find("const [stdout, stderr] = await Promise.all(");
    const auto log_bytes_read = selection.find("const [stdoutText, stderrText] = await Promise.all(");
    const auto guarded = [&](const std::size_t read, const std::size_t write) {
        const auto guard = selection.find("if (generation !== jobSelectionGeneration) return;", read);
        return read != std::string::npos && write != std::string::npos &&
               guard != std::string::npos && read < guard && guard < write;
    };
    require(guarded(job_read, selection.find("state.job = job")) &&
                guarded(artifacts_read, selection.find("renderArtifacts(artifacts.artifacts)")) &&
                guarded(logs_read, log_bytes_read) &&
                guarded(log_bytes_read, selection.find("$(\"stdout\").textContent = stdoutText")) &&
                selection.find("$(\"stderr\").textContent = stderrText") != std::string::npos &&
                selection.find("if (generation === jobSelectionGeneration) showError(error.message)") != std::string::npos,
            "job, artifact, log responses and errors only affect the current selection after asynchronous boundaries");
    require(selection.find("state.source = null") < selection.find("renderArtifacts([]); updateControls();") &&
                selection.find("renderArtifacts([]); updateControls();") < artifacts_read,
            "a newly displayed job clears former artifacts and refreshes controls before loading its artifacts");
    const auto log_status_check = selection.find("for (const response of [stdout, stderr])");
    const auto log_error_start = selection.find("if (!response.ok)", log_status_check);
    const auto log_error_end = selection.find("throw new Error((data && data.error) || \"HTTP \" + response.status)", log_error_start);
    const auto log_error_json = selection.find("data = await response.json()", log_error_start);
    require(guarded(logs_read, log_status_check) &&
                log_status_check < log_error_start && log_error_start < log_error_json &&
                log_error_json < log_error_end && log_error_end < log_bytes_read &&
                selection.find("catch {}", log_error_json) < log_error_end &&
                selection.find("response.json()", log_error_end) == std::string::npos &&
                selection.find("stdout.text()", log_bytes_read) != std::string::npos &&
                selection.find("stderr.text()", log_bytes_read) != std::string::npos,
            "both log responses reject HTTP errors using the JSON error name or status fallback before successful raw text is read");
    const auto refresh_end = dashboard.find("const action = async", selection_end);
    require(refresh_end != std::string::npos,
            "dashboard refresh has a bounded independent region");
    const auto refresh = dashboard.substr(selection_end, refresh_end - selection_end);
    const auto refresh_entry = refresh.find("const selectionGeneration = jobSelectionGeneration;");
    const auto refresh_health = refresh.find("await getJson(\"/api/v1/health\")");
    const auto refresh_lists = refresh.find("renderJobs(jobs); renderImports(imports);");
    const auto refresh_selection = refresh.find("if (selectionGeneration === jobSelectionGeneration && state.job)");
    require(refresh_entry != std::string::npos && refresh_entry < refresh_health &&
                refresh_health < refresh_lists && refresh_lists < refresh_selection &&
                refresh_selection != std::string::npos,
            "refresh reads health and lists but only reloads job details when no selection began since refresh entry");
    const auto refresh_generation = refresh.find("const generation = ++refreshGeneration;");
    const auto refresh_health_write = refresh.find("$(\"health\").textContent =");
    const auto refresh_list_read = refresh.find("const [jobs, imports] = await Promise.all(");
    const auto refresh_guarded = [&](const std::size_t read, const std::size_t write) {
        const auto guard = refresh.find("if (generation !== refreshGeneration) return;", read);
        return read != std::string::npos && write != std::string::npos &&
               guard != std::string::npos && read < guard && guard < write;
    };
    require(dashboard.find("let refreshGeneration = 0;") != std::string::npos &&
                refresh_generation != std::string::npos && refresh_generation < refresh_health &&
                refresh_guarded(refresh_health, refresh_health_write) &&
                refresh_guarded(refresh_list_read, refresh_lists),
            "only the latest refresh renders health and lists");
    const auto refresh_clear_error = refresh.find("if (expectedSelectionGeneration === jobSelectionGeneration) showError(\"\");");
    require(refresh_clear_error != std::string::npos && refresh_clear_error < refresh_health &&
                refresh.find("if (generation === refreshGeneration && expectedSelectionGeneration === jobSelectionGeneration) showError(error.message);") != std::string::npos,
            "superseded action refreshes cannot clear or overwrite the newer selection's error field");
    const auto action_end = dashboard.find("const importAction = async", refresh_end);
    require(action_end != std::string::npos,
            "job actions have a bounded independent region");
    const auto action = dashboard.substr(refresh_end, action_end - refresh_end);
    const auto action_generation = action.find("const selectionGeneration = jobSelectionGeneration;");
    const auto action_target = action.find("const jobId = state.job.id;");
    const auto action_source = action.find("const source = state.source;");
    const auto action_post = action.find("await fetch(");
    const auto action_refresh = action.find("const refreshedSelectionGeneration = await refresh(selectionGeneration);");
    const auto action_select_guard = action.find("if (refreshedSelectionGeneration !== jobSelectionGeneration) return;", action_refresh);
    const auto action_select = action.find("await selectJob(data.id || jobId);", action_select_guard);
    require(action_generation != std::string::npos && action_generation < action_post &&
                action_target != std::string::npos && action_target < action_post &&
                action_source != std::string::npos && action_source < action_post &&
                action.find("encodeURIComponent(jobId)") != std::string::npos &&
                action.find("JSON.stringify({path:source})") != std::string::npos &&
                action_post < action_refresh && action_refresh < action_select_guard &&
                action_select_guard < action_select && action_select != std::string::npos,
            "job actions capture their target before posting and never replace a newer selection after refreshing");
    require(action.find("if (selectionGeneration === jobSelectionGeneration) showError(error.message);") != std::string::npos,
            "asynchronous job action errors cannot overwrite the newer selection's error field");
    const auto refresh_expected_guard = refresh.find("if (expectedSelectionGeneration !== selectionGeneration) return null;");
    const auto refresh_selection_call = refresh.find("const selection = selectJob(state.job.id);", refresh_selection);
    const auto refresh_selection_token = refresh.find("const refreshedSelectionGeneration = jobSelectionGeneration;", refresh_selection_call);
    const auto refresh_selection_wait = refresh.find("await selection;", refresh_selection_token);
    const auto refresh_selection_return = refresh.find("return refreshedSelectionGeneration;", refresh_selection_wait);
    require(refresh.find("async (expectedSelectionGeneration = jobSelectionGeneration)") != std::string::npos &&
                refresh_lists < refresh_expected_guard && refresh_expected_guard < refresh_selection &&
                refresh_selection < refresh_selection_call && refresh_selection_call < refresh_selection_token &&
                refresh_selection_token < refresh_selection_wait && refresh_selection_wait < refresh_selection_return &&
                refresh_selection_return != std::string::npos &&
                dashboard.find("$(\"refresh\").addEventListener(\"click\", () => refresh());") != std::string::npos,
            "action refreshes still update lists but return only their own selection token and cannot reload a superseded target");
    require(dashboard.find("id=\"import-form\"") != std::string::npos &&
                dashboard.find("/api/v1/imports") != std::string::npos &&
                dashboard.find("X-Bruce-Import-Name") != std::string::npos,
            "dashboard exposes the explicit import contract");
    const auto upload_start = dashboard.find("const importFile = async");
    const auto upload_end = dashboard.find("$(\"refresh\").addEventListener", upload_start);
    require(upload_start != std::string::npos && upload_end != std::string::npos,
            "dashboard has an explicit file upload helper");
    const auto upload = dashboard.substr(upload_start, upload_end - upload_start);
    const auto size_guard = upload.find("if (file.size > 8 * 1024 * 1024) { showError(\"CockpitImportTooLarge\"); return; }");
    require(size_guard != std::string::npos && size_guard < upload.find("file.arrayBuffer()") &&
                upload.find("\"Content-Type\":\"application/octet-stream\"") != std::string::npos &&
                upload.find("\"X-Bruce-Import-Name\":file.name") != std::string::npos &&
                upload.find("authHeaders()") != std::string::npos &&
                upload.find("fetch(\"/api/v1/imports\"") != std::string::npos &&
                upload.find("importAction(") == std::string::npos &&
                upload.find("selectJob(") == std::string::npos,
            "upload rejects oversized files before reading bytes and only imports via the authenticated same-origin route");
    const auto upload_generation = upload.find("const selectionGeneration = jobSelectionGeneration;");
    const auto upload_first_await = upload.find("await ");
    const auto upload_refresh = upload.find("await refresh(selectionGeneration);");
    require(upload_generation != std::string::npos && upload_generation < upload_first_await &&
                upload_first_await < upload_refresh && upload_refresh != std::string::npos &&
                upload.find("await refresh();") == std::string::npos,
            "upload captures the job selection before reading bytes and refreshes without replacing a newer selection");
    require(upload.find("if (selectionGeneration === jobSelectionGeneration) showError(error.message);") != std::string::npos,
            "asynchronous upload errors cannot overwrite the newer selection's error field");
    const auto upload_http_guard = upload.find("if (!response.ok) {");
    const auto upload_error_json = upload.find("let data = null; try { data = await response.json(); } catch {}", upload_http_guard);
    const auto upload_error_fallback = upload.find("throw new Error((data && data.error) || \"HTTP \" + response.status);", upload_error_json);
    const auto upload_success_json = upload.find("await response.json();", upload_error_fallback);
    require(upload_http_guard != std::string::npos && upload_http_guard < upload_error_json &&
                upload_error_json < upload_error_fallback && upload_error_fallback < upload_success_json &&
                upload_success_json != std::string::npos && upload_success_json < upload_refresh,
            "upload errors use best-effort JSON with HTTP fallback before strict successful-response parsing");
    const auto upload_file = upload.find("const file = $(\"import-file\").files[0];");
    const auto upload_reset = upload.find("if ($(\"import-file\").files[0] === file) $(\"import-form\").reset();");
    require(upload_file != std::string::npos && upload_file < upload_first_await &&
                upload_reset != std::string::npos && upload_reset < upload_refresh,
            "successful upload resets only its original file selection while refreshing independently");
    require(dashboard.find("label.textContent = imported.name") != std::string::npos,
            "import filenames render as text rather than markup");
    require(dashboard.find("__BRUCE_COCKPIT_TOKEN__") != std::string::npos,
            "dashboard reserves the per-process authentication token marker");
    require(dashboard.find("[[\"inspect\",\"Inspect\"],[\"assess\",\"Assess\"],[\"replay\",\"Replay\"],[\"gate\",\"Gate\"]]") != std::string::npos &&
                dashboard.find("button.dataset.importId = imported.id") != std::string::npos &&
                dashboard.find("button.dataset.importAction = name") != std::string::npos &&
                dashboard.find("importAction(button.dataset.importId, button.dataset.importAction)") != std::string::npos,
            "each imported item exposes four actions bound to its import ID");
    const auto import_action_start = dashboard.find("const importAction = async");
    const auto import_action_end = dashboard.find("const createJob = async", import_action_start);
    require(import_action_start != std::string::npos && import_action_end != std::string::npos,
            "dashboard has an import action transport helper");
    const auto import_action = dashboard.substr(import_action_start, import_action_end - import_action_start);
    require(import_action.find("\"/api/v1/imports/\" + encodeURIComponent(id) + \"/\" + name") != std::string::npos &&
                import_action.find("method:\"POST\"") != std::string::npos &&
                import_action.find("authHeaders()") != std::string::npos &&
                import_action.find("\"Content-Type\":\"application/json\"") != std::string::npos &&
                import_action.find("body:\"\"") != std::string::npos &&
                import_action.find("JSON.stringify") == std::string::npos &&
                import_action.find("state.source") == std::string::npos,
            "import actions use authenticated JSON POSTs with an empty body and no path payload");
    require(import_action.find("response.status !== 201") != std::string::npos &&
                import_action.find("data.error || \"HTTP \" + response.status") != std::string::npos &&
                import_action.find("showError(error.message)") != std::string::npos,
            "import actions require 201 and report the existing error name or status fallback");
    const auto import_action_generation = import_action.find("const selectionGeneration = jobSelectionGeneration;");
    const auto import_action_post = import_action.find("await fetch(");
    const auto import_action_refresh = import_action.find("const refreshedSelectionGeneration = await refresh(selectionGeneration);");
    const auto import_action_select_guard = import_action.find("if (refreshedSelectionGeneration !== jobSelectionGeneration) return;", import_action_refresh);
    const auto import_action_select = import_action.find("await selectJob(data.id);", import_action_select_guard);
    require(import_action_generation != std::string::npos && import_action_generation < import_action_post &&
                import_action_post < import_action_refresh && import_action_refresh < import_action_select_guard &&
                import_action_select_guard < import_action_select && import_action_select != std::string::npos,
            "import actions never replace a newer job selection after their POST or refresh");
    require(import_action.find("if (selectionGeneration === jobSelectionGeneration) showError(error.message);") != std::string::npos,
            "asynchronous import action errors cannot overwrite the newer selection's error field");

    const auto create_job_start = dashboard.find("const createJob = async");
    const auto create_job_end = dashboard.find("const importFile = async", create_job_start);
    require(create_job_start != std::string::npos && create_job_end != std::string::npos,
            "dashboard has a bounded explicit job creation helper");
    const auto create_job = dashboard.substr(create_job_start, create_job_end - create_job_start);
    const auto create_job_generation = create_job.find("const selectionGeneration = jobSelectionGeneration;");
    const auto create_job_post = create_job.find("await fetch(\"/api/v1/jobs\"");
    const auto create_job_refresh = create_job.find("const refreshedSelectionGeneration = await refresh(selectionGeneration);");
    const auto create_job_select_guard = create_job.find("if (refreshedSelectionGeneration !== jobSelectionGeneration) return;", create_job_refresh);
    const auto create_job_select = create_job.find("await selectJob(data.id);", create_job_select_guard);
    require(create_job_generation != std::string::npos && create_job_generation < create_job_post &&
                create_job_post < create_job_refresh && create_job_refresh < create_job_select_guard &&
                create_job_select_guard < create_job_select && create_job_select != std::string::npos,
            "job creation never replaces a newer selection after its POST or refresh");
    require(create_job.find("if (selectionGeneration === jobSelectionGeneration) showError(error.message);") != std::string::npos,
            "asynchronous job creation errors cannot overwrite the newer selection's error field");
    require(create_job.find("method:\"POST\"") != std::string::npos &&
                create_job.find("authHeaders()") != std::string::npos &&
                create_job.find("JSON.stringify({argv:[command,...args]})") != std::string::npos &&
                create_job.find("value.split(/\\r?\\n/).map(token => token.trim()).filter(Boolean)") != std::string::npos &&
                create_job.find("if (!response.ok) throw new Error(data.error || \"HTTP \" + response.status)") != std::string::npos,
            "job creation preserves authenticated POST, separate argv tokens, and existing response errors");

    require(dashboard.find("Natural language → draft") != std::string::npos &&
                dashboard.find("data-language-form") != std::string::npos &&
                dashboard.find("data-language-text") != std::string::npos &&
                dashboard.find("data-language-alternatives") != std::string::npos,
            "dashboard exposes a separate language draft section");
    for (const auto marker : {"data-language-alias=\"1\"", "data-language-alias=\"2\"",
                              "data-language-bytes=\"1\"", "data-language-bytes=\"2\""}) {
        require(dashboard.find(marker) != std::string::npos,
                "language draft form exposes exactly the two input pairs");
    }
    const auto language_start = dashboard.find("const languageJson = async");
    const auto language_end = dashboard.find("const selectJob = async", language_start);
    require(language_start != std::string::npos && language_end != std::string::npos,
            "language draft helpers have a bounded independent region");
    const auto language = dashboard.substr(language_start, language_end - language_start);
    require(dashboard.find("data-language-draft-controls") != std::string::npos &&
                dashboard.find("data-language-load-drafts") != std::string::npos &&
                dashboard.find("Load drafts") != std::string::npos &&
                dashboard.find("data-language-drafts") != std::string::npos,
            "language panel offers explicit process-local draft loading");
    const auto load_start = language.find("const renderLanguageDraftList =");
    const auto load_end = language.find("const dispatchLanguageDraft = async", load_start);
    require(load_start != std::string::npos && load_end != std::string::npos,
            "draft loading has a bounded independent region");
    const auto loading = language.substr(load_start, load_end - load_start);
    require(loading.find("await getJson(\"/api/v1/language/drafts\")") != std::string::npos &&
                loading.find("await getJson(\"/api/v1/language/drafts/\" + encodeURIComponent(id))") != std::string::npos &&
                loading.find("button.dataset.languageDraft = draft.id") != std::string::npos &&
                loading.find("loadLanguageDraft(button.dataset.languageDraft)") != std::string::npos &&
                loading.find("renderLanguageDraft(await getJson") != std::string::npos &&
                loading.find("if (!languageEnabled || languagePending) return") != std::string::npos &&
                loading.find("label.textContent") != std::string::npos &&
                loading.find("showError(error.message)") != std::string::npos,
            "explicit list and detail loading use authenticated GETs, pending guards, and safe display");
    for (const auto forbidden : {"languageJson(", "dispatchLanguageDraft(", "exportLanguageBundle(",
                                 "selectLanguageAlternative(", "innerHTML", "method:\"POST\""}) {
        require(loading.find(forbidden) == std::string::npos,
                std::string("draft loading is read-only and does not select or execute: ") + forbidden);
    }
    for (const auto field : {"raw_text", "normalized_text", "grammar_version", "status",
                             "selected_id", "alternatives", "inputs", "budget", "max_bits",
                             "max_denominator", "max_trace_bytes"}) {
        require(language.find(field) != std::string::npos,
                std::string("language renderer exposes server field: ") + field);
    }
    require(language.find("crypto.subtle.digest(\"SHA-256\"") != std::string::npos &&
                language.find("new TextEncoder()") != std::string::npos &&
                language.find("/api/v1/language/drafts") != std::string::npos &&
                language.find("method:\"POST\"") != std::string::npos &&
                language.find("authHeaders()") != std::string::npos &&
                language.find("\"Content-Type\":\"application/json\"") != std::string::npos,
            "language creation hashes input bytes locally and uses the authenticated JSON contract");
    require(language.find("draft.status === \"AMBIGUOUS\" && !draft.selected_id") != std::string::npos &&
                language.find("button.dataset.languageAlternative = alternative.id") != std::string::npos &&
                language.find("alternative.rule") != std::string::npos &&
                language.find("encodeURIComponent(id) + \"/select\"") != std::string::npos &&
                language.find("{alternative:alternative}") != std::string::npos &&
                language.find("renderLanguageDraft(await languageJson") != std::string::npos,
            "only an unselected ambiguous draft offers explicit server alternatives and renders selection response");
    require(language.find("const languageDispatchEnabled = ") != std::string::npos &&
                language.find("languageDispatchEnabled && (draft.status === \"UNDERSTOOD\" ||") != std::string::npos &&
                language.find("(draft.status === \"AMBIGUOUS\" && Boolean(draft.selected_id)))") != std::string::npos &&
                language.find("button.dataset.languageDispatch = draft.id") != std::string::npos &&
                language.find("button.textContent = \"Run CORE\"") != std::string::npos &&
                language.find("dispatchLanguageDraft(button.dataset.languageDispatch)") != std::string::npos,
            "only understood or explicitly selected ambiguous drafts in a dispatch-enabled build offer CORE execution");
    const auto dispatch_start = language.find("const dispatchLanguageDraft = async");
    const auto dispatch_end = language.find("const selectLanguageAlternative = async", dispatch_start);
    require(dispatch_start != std::string::npos && dispatch_end != std::string::npos,
            "language CORE dispatch has its own transport helper");
    const auto dispatch = language.substr(dispatch_start, dispatch_end - dispatch_start);
    require(dispatch.find("if (!languageDispatchEnabled || languagePending) return") != std::string::npos &&
                dispatch.find("encodeURIComponent(id) + \"/dispatch\"") != std::string::npos &&
                dispatch.find("method:\"POST\"") != std::string::npos &&
                dispatch.find("authHeaders()") != std::string::npos &&
                dispatch.find("\"Content-Type\":\"application/json\"") != std::string::npos &&
                dispatch.find("body:\"\"") != std::string::npos &&
                dispatch.find("textContent = JSON.stringify(data, null, 2)") != std::string::npos &&
                dispatch.find("showError(error.message)") != std::string::npos,
            "CORE dispatch uses an authenticated empty JSON POST and renders the response or existing error surface");
    for (const auto forbidden : {"/run", "/api/v1/jobs", "data-action",
                                 "\"Run\"", "\"Dispatch\"", "\"Job starten\"",
                                 "selectJob(", "createJob(", "refresh(", "state.job", "draft.inputs.bytes", "innerHTML"}) {
        require(language.find(forbidden) == std::string::npos,
                std::string("language draft and CORE execution stay independent of jobs and unsafe output: ") + forbidden);
    }
    require(language.find("data.error || \"HTTP \" + response.status") != std::string::npos &&
                language.find("showError(error.message)") != std::string::npos &&
                dashboard.find("$(\"error\").textContent = message || \"\"") != std::string::npos,
            "language server errors reach the existing error area unchanged");
    require(dashboard.find("data-language-bundle-controls") != std::string::npos &&
                dashboard.find("data-language-bundle-status") != std::string::npos &&
                dashboard.find("data-language-replay-file") != std::string::npos &&
                dashboard.find("data-language-replay-result") != std::string::npos &&
                language.find("exportButton.dataset.languageBundle = draft.id") != std::string::npos &&
                language.find("encodeURIComponent(id) + \"/dispatch/bundle\"") != std::string::npos &&
                language.find("URL.createObjectURL(bundle)") != std::string::npos &&
                language.find("URL.revokeObjectURL(languageBundleUrl)") != std::string::npos,
            "language panel exposes explicit bundle export, download, and separate replay output");
    require(language.find("/api/v1/language/dispatch/replay") != std::string::npos &&
                language.find("\"Content-Type\":\"application/octet-stream\"") != std::string::npos &&
                language.find("file.size > languageReplayMaxBytes") != std::string::npos &&
                language.find("ApplicationDispatchPayloadTooLarge") != std::string::npos &&
                language.find("[data-language-replay-result]\").textContent = JSON.stringify(data, null, 2)") != std::string::npos &&
                language.find("!languageDispatchEnabled || pending") != std::string::npos,
            "explicit replay checks file size before reading and preserves build availability during pending changes");
    require(dashboard.find("__BRUCE_LANGUAGE_") == std::string::npos,
            "language build availability markers are resolved");
    const bool language_enabled = dashboard.find("const languageEnabled = true;") != std::string::npos;
    require((language_enabled || dashboard.find("const languageEnabled = false;") != std::string::npos) &&
                dashboard.find("if (languageEnabled) {") != std::string::npos &&
                dashboard.find("[data-language-load-drafts]\").addEventListener(\"click\", loadLanguageDrafts)") != std::string::npos &&
                language.find("[data-language-draft-controls]\").disabled = !languageEnabled || pending") != std::string::npos,
            "language create and load listeners and pending guards retain build availability");
    require(language_enabled
                ? dashboard.find("Language draft management is available.") != std::string::npos &&
                      dashboard.find("<fieldset disabled>") == std::string::npos
                : dashboard.find("Language draft management is unavailable in this build.") != std::string::npos &&
                      dashboard.find("<fieldset disabled>") != std::string::npos,
            "language-off builds expose an unavailable, disabled draft form");
    require(language_enabled
                ? dashboard.find("<fieldset data-language-draft-controls >") != std::string::npos
                : dashboard.find("<fieldset data-language-draft-controls disabled>") != std::string::npos,
            "language-off builds keep draft loading disabled");
    const bool dispatch_enabled = language.find("const languageDispatchEnabled = true;") != std::string::npos;
    require(dispatch_enabled || language.find("const languageDispatchEnabled = false;") != std::string::npos,
            "language CORE dispatch availability is resolved to a build-time guard");
    require(dashboard.find("data-language-dispatch-availability") != std::string::npos &&
                dashboard.find("data-language-dispatch-actions") != std::string::npos &&
                (dispatch_enabled
                     ? language_enabled && dashboard.find("CORE execution is available.") != std::string::npos
                     : dashboard.find("CORE execution is unavailable in this build.") != std::string::npos),
            "dispatch-off builds clearly report unavailable CORE execution without enabling a handler");
    require(dispatch_enabled
                ? dashboard.find("<fieldset data-language-bundle-controls >") != std::string::npos &&
                      language.find("const languageReplayMaxBytes = 33554572;") != std::string::npos
                : dashboard.find("<fieldset data-language-bundle-controls disabled>") != std::string::npos,
            "bundle replay controls retain build availability and use the native CORE envelope receive limit");

    char help[] = "--help";
    char* help_argv[] = {argv[0], const_cast<char*>("cockpit"), help};
    require(bruce::cockpit_cli(3, help_argv) == 0, "Cockpit help is available");

    char invalid_port[] = "65536";
    char* invalid_argv[] = {argv[0], const_cast<char*>("cockpit"), const_cast<char*>("--port"), invalid_port};
    require(rejects([&] { static_cast<void>(bruce::cockpit_cli(4, invalid_argv)); }),
            "invalid Cockpit ports are rejected");
    const auto bruce_binary = std::filesystem::absolute(argv[0]).parent_path() / "bruce";
    test_job_contract(bruce_binary);
    test_import_contract(bruce_binary);
    require(argc >= 2, "controlled child fixture path is supplied by CTest");
    test_process_contract(std::filesystem::absolute(argv[1]));
    return 0;
}
