#include "../../apps/runtime_mod_cli.hpp"
#include <iostream>
#include <limits>

int main() {
    try {
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
        const bruce::RuntimeModActiveViewV1 view{{}, bruce::core_tree_registry_v1().mount(fragment, "large-test")};
        try {
            (void)bruce::runtime_mod_cli_detail::render_route(view, "x");
        } catch (const bruce::Error& error) {
            if (std::string_view(error.what()) == "RouteReportTooLarge") return 0;
            throw;
        }
        throw bruce::Error("OversizedRouteReportWasExportable");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
