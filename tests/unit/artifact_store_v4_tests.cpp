#include "bruce/artifact/artifact_store_v4.hpp"
#include "bruce/evidence/bundle.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

using namespace bruce;
using namespace bruce::artifact_v1;
using namespace bruce::artifact_v4;
namespace fs = std::filesystem;
namespace {
void require(bool yes, const char* message) { if (!yes) throw Error(message); }
template<class F> void rejects(F&& action) {
    try { action(); } catch (const Error& e) {
        require(std::string_view(e.what()) == "PROTOCOL_ERROR", "diagnostic leaked"); return;
    }
    throw Error("invalid transfer accepted");
}
struct Fixture {
    fs::path base;
    Fixture() { char pattern[] = "/tmp/bruce-v4-tests-XXXXXX"; base = ::mkdtemp(pattern); }
    ~Fixture() { fs::remove_all(base); }
    fs::path store() const { return base / "store"; }
    ArtifactManifestV1 ready() {
        auto local = ArtifactStoreV1::open(store());
        ArtifactManifestV1 m{"run", "node", "attempt", "READY", "OK", {}};
        m.objects.push_back({local.put_bytes("payload"), "application/octet-stream", "local"});
        local.publish_manifest("example", m); return m;
    }
};
std::map<std::string, std::string> snapshot(const fs::path& root) {
    std::map<std::string, std::string> result;
    if (!fs::exists(root)) return result;
    for (const auto& item : fs::recursive_directory_iterator(root)) if (item.is_regular_file()) {
        std::ifstream input(item.path(), std::ios::binary);
        result[item.path().lexically_relative(root).string()] = std::string(std::istreambuf_iterator<char>(input), {});
    }
    return result;
}
struct Remote {
    std::map<std::string, std::string> files;
    unsigned calls{};
    HttpsTransportV4 transport() {
        return [&](const HttpsRequestV4& req, const auto& headers, const auto& body) {
            ++calls;
            require(req.endpoint.host == "example.org" && req.endpoint.port == 8443, "wrong authority");
            const auto now = std::chrono::steady_clock::now();
            require(req.endpoint.connect_deadline > now && req.endpoint.deadline > now &&
                    req.endpoint.connect_deadline <= now + std::chrono::seconds(5) &&
                    req.endpoint.deadline <= now + std::chrono::seconds(30), "missing bounded deadline");
            if (req.method == "PUT") {
                files[req.endpoint.path] = req.upload_bytes; headers(201, 0);
            } else {
                require(req.method == "GET" && req.upload_bytes.empty(), "bad GET");
                const auto& bytes = files.at(req.endpoint.path); headers(200, bytes.size()); body(bytes);
            }
        };
    }
};
constexpr auto remote = "https://example.org:8443/bruce";
// Missing object validation, wrong routes/order or a local write during mirror breaks this.
void verified_ready_roundtrip() {
    Fixture source, target; const auto manifest = source.ready(); Remote peer;
    const auto before = snapshot(source.store());
    const auto result = mirror_to_remote_v4("example", source.store(), remote, peer.transport());
    require(result.manifest_id == "example" && result.object_count == 1 && result.total_object_bytes == 7,
            "wrong transfer metadata");
    require(result.manifest_sha256 == sha256(encode_manifest_v1(manifest)) &&
            result.manifest_size_bytes == encode_manifest_v1(manifest).size(), "wrong manifest identity");
    require(peer.calls == 2 && peer.files.count("/bruce/manifests/example.mirror") == 1 &&
            peer.files.count("/bruce/objects/23/239f59ed55e737c77147cf55ad0c1b030b6d7ee748a7426952f9b852d5a935e5.blob") == 1,
            "wrong fixed paths");
    require(snapshot(source.store()) == before, "mirror changed local store");
    const auto restored = restore_from_remote_v4("example", target.store(), remote, peer.transport());
    require(restored.manifest_sha256 == result.manifest_sha256, "restore identity changed");
    auto local = ArtifactStoreV1::open_existing(target.store());
    require(local.read_manifest("example") == manifest, "manifest not restored");
    local.verify(manifest.objects.front().id);
    const auto calls = peer.calls; const auto retained = snapshot(target.store());
    rejects([&] { restore_from_remote_v4("example", target.store(), remote, peer.transport()); });
    require(peer.calls == calls && snapshot(target.store()) == retained, "existing manifest touched");
}
// Network/manifest corruption must never publish even a partial local restore.
void corrupt_restore_leaves_no_store() {
    Fixture source; source.ready(); Remote good;
    mirror_to_remote_v4("example", source.store(), remote, good.transport());
    for (int kind = 0; kind < 7; ++kind) {
        Fixture target; auto peer = good;
        auto& envelope = peer.files.at("/bruce/manifests/example.mirror");
        if (kind == 0) envelope.back() ^= 1;
        if (kind == 1) envelope.replace(envelope.find("example"), 7, "another");
        if (kind == 2) envelope += "extra";
        if (kind == 3) envelope = "not a manifest\n";
        if (kind == 4) for (auto& [path, bytes] : peer.files) if (path.ends_with(".blob")) bytes = "corrupt";
        if (kind == 5) for (auto& [path, bytes] : peer.files) if (path.ends_with(".blob")) bytes = "short";
        if (kind == 6) envelope.resize(0);
        rejects([&] { restore_from_remote_v4("example", target.store(), remote, peer.transport()); });
        require(!fs::exists(target.store()), "bad restore created local store");
    }
}
void errors_and_validation_preserve_local() {
    Fixture source; const auto manifest = source.ready(); const auto before = snapshot(source.store());
    for (const auto message : {"TLS certificate failed", "hostname mismatch", "timeout"}) {
        HttpsTransportV4 failure = [&](const auto&, const auto&, const auto&) { throw std::runtime_error(message); };
        rejects([&] { mirror_to_remote_v4("example", source.store(), remote, failure); });
        Fixture target;
        rejects([&] { restore_from_remote_v4("example", target.store(), remote, failure); });
        require(!fs::exists(target.store()), "transport failure created store");
    }
    for (int status : {200, 201, 204, 301, 404, 500}) {
        HttpsTransportV4 bad = [=](const auto&, const auto& h, const auto& b) { h(status, 1); b("x"); };
        Fixture target;
        rejects([&] { restore_from_remote_v4("example", target.store(), remote, bad); });
    }
    unsigned calls = 0;
    HttpsTransportV4 never = [&](const auto&, const auto&, const auto&) { ++calls; };
    for (auto origin : {"http://example.org", "https://user:password@example.org", "https://example.org/?token=x"})
        rejects([&] { mirror_to_remote_v4("example", source.store(), origin, never); });
    rejects([&] { mirror_to_remote_v4("../bad", source.store(), remote, never); });
    require(calls == 0 && snapshot(source.store()) == before, "invalid request changed store");
    auto local = ArtifactStoreV1::open_existing(source.store());
    auto failed = manifest; failed.status = "FAILED"; local.publish_manifest("failed", failed);
    rejects([&] { mirror_to_remote_v4("failed", source.store(), remote, never); });
    std::ofstream(source.store() / "objects" / manifest.objects[0].id.sha256.substr(0,2) /
                  (manifest.objects[0].id.sha256 + ".blob"), std::ios::binary) << "corrupt";
    rejects([&] { mirror_to_remote_v4("example", source.store(), remote, never); });
    require(calls == 0, "unverified local data uploaded");
}
// Removing the aggregate byte cap or accepting invalid response event ordering
// would allow unbounded work or publish malformed responses.
void bounded_transfers() {
    Fixture source; auto manifest = source.ready();
    auto local = ArtifactStoreV1::open_existing(source.store());
    manifest.objects.push_back({local.put_bytes(std::string(max_file_bytes, 'b')), "", ""});
    local.publish_manifest("over-budget", manifest);
    unsigned calls = 0;
    HttpsTransportV4 never = [&](const auto&, const auto&, const auto&) { ++calls; };
    rejects([&] { mirror_to_remote_v4("over-budget", source.store(), remote, never); });
    require(calls == 0, "aggregate cap checked after upload");
    for (int kind = 0; kind < 5; ++kind) {
        Fixture target;
        HttpsTransportV4 bad = [=](const auto&, const auto& h, const auto& b) {
            if (kind == 0) { h(200, mirror_manifest_limit_v4 + 1); }
            if (kind == 1) { b("body before headers"); }
            if (kind == 2) { h(200, 0); h(200, 0); }
            if (kind == 3) { h(200, 1); }
            if (kind == 4) { h(200, 0); b("extra"); }
        };
        rejects([&] { restore_from_remote_v4("example", target.store(), remote, bad); });
        require(!fs::exists(target.store()), "malformed stream published store");
    }
}
// A legal V1 manifest ID with consecutive dots is a filename, not traversal.
void dotted_manifest_ids() {
    Fixture source, target; const auto manifest = source.ready(); Remote peer;
    ArtifactStoreV1::open_existing(source.store()).publish_manifest("a..b", manifest);
    mirror_to_remote_v4("a..b", source.store(), remote, peer.transport());
    restore_from_remote_v4("a..b", target.store(), remote, peer.transport());
    require(ArtifactStoreV1::open_existing(target.store()).read_manifest("a..b") == manifest,
            "legal dotted ID rejected");
}
// Existing metadata remains present even when opening its bytes returns EACCES.
// Restore must reject before contacting any remote.
void unreadable_existing_manifest_blocks_network() {
    Fixture source; source.ready();
    const auto path = source.store() / "manifests" / "example.manifest";
    require(::chmod(path.c_str(), 0000) == 0, "chmod failed");
    unsigned calls = 0;
    HttpsTransportV4 transport = [&](const auto&, const auto&, const auto&) { ++calls; };
    rejects([&] { restore_from_remote_v4("example", source.store(), remote, transport); });
    require(::chmod(path.c_str(), 0600) == 0, "restore mode failed");
    require(calls == 0, "existing unreadable manifest reached network");
}
void existing_object_never_replaced() {
    Fixture source, target; const auto manifest = source.ready(); Remote peer;
    mirror_to_remote_v4("example", source.store(), remote, peer.transport());
    auto local = ArtifactStoreV1::open(target.store()); local.put_bytes("payload");
    const auto object = target.store() / "objects" / manifest.objects[0].id.sha256.substr(0,2) /
                        (manifest.objects[0].id.sha256 + ".blob");
    std::ofstream(object, std::ios::binary) << "corrupt";
    const auto before = snapshot(target.store());
    rejects([&] { restore_from_remote_v4("example", target.store(), remote, peer.transport()); });
    require(snapshot(target.store()) == before, "existing corrupt object replaced");
}
}
int main() {
    const auto run = [](const char* name, auto test) {
        try { test(); }
        catch (const std::exception& e) { throw Error(std::string(name) + ": " + e.what()); }
    };
    try {
        run("verified ready roundtrip", verified_ready_roundtrip);
        run("corrupt restore preserves absence", corrupt_restore_leaves_no_store);
        run("validation preserves local", errors_and_validation_preserve_local);
        run("existing object preserved", existing_object_never_replaced);
        run("dotted manifest IDs", dotted_manifest_ids);
        run("bounded transfers", bounded_transfers);
        run("unreadable manifest blocks network", unreadable_existing_manifest_blocks_network);
        std::cout << "artifact store V4 contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
