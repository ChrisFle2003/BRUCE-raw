#include "mcp_inspector_server.hpp"
#include "mcp_inspector.hpp"
#include "mcp_inspector_assets.hpp"
#include "httplib.h"

namespace bruce {

int run_mcp_inspector_server(const std::uint16_t port) {
    httplib::Server server;
    server.set_payload_max_length(mcp_max_inspect_bytes_v3);
    // cpp-httplib maps HEAD to GET by default. Only the declared methods apply.
    server.set_pre_routing_handler([](const httplib::Request& request, httplib::Response& response) {
        if (request.method != "GET" && request.method != "POST") {
            response.status = 404;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    server.set_error_handler([](const httplib::Request& request, httplib::Response& response) {
        // Unknown method tokens are rejected by the transport parser before routing.
        if (response.status == 400 && !request.method.empty() &&
            request.method != "GET" && request.method != "POST") {
            response.status = 404;
        }
        if (response.status == 413) {
            response.set_content("{\"status\":\"REJECTED\",\"code\":\"REJECTED\"}", "application/json");
        } else if (response.status == 500) {
            response.set_content("{\"status\":\"ERROR\",\"code\":\"ERROR\"}", "application/json");
        }
    });
    server.set_exception_handler([](const httplib::Request&, httplib::Response& response, std::exception_ptr) {
        response.status = 500;
        response.set_content("{\"status\":\"ERROR\",\"code\":\"ERROR\"}", "application/json");
    });
    server.Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.set_content(mcp_inspector_html(), "text/html; charset=utf-8");
    });
    server.Get("/api/v1/health", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("{\"status\":\"OK\"}", "application/json");
    });
    server.Post("/api/v1/inspect", [](const httplib::Request& request, httplib::Response& response) {
        const auto result = inspect_phase_dag_v2_bundle(request.body);
        response.status = result.status == McpInspectionStatusV1::valid ? 200 : 422;
        response.set_content(result.json, "application/json");
    });
    server.Post("/api/v3/inspect", [](const httplib::Request& request, httplib::Response& response) {
        const auto result = inspect_phase_dag_v3_bundle(request.body);
        response.status = result.status == McpInspectionStatusV1::valid ? 200 : 422;
        response.set_content(result.json, "application/json");
    });
    server.Post("/api/dag-scheduler-v4/inspect", [](const httplib::Request& request, httplib::Response& response) {
        const auto result = inspect_dag_scheduler_v4_bundle(request.body);
        response.status = result.status == McpInspectionStatusV1::valid ? 200 : 422;
        response.set_content(result.json, "application/json");
    });
    if (!server.bind_to_port("127.0.0.1", port)) return 1;
    return server.listen_after_bind() ? 0 : 1;
}

} // namespace bruce
