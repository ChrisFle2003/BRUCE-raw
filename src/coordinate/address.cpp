#include "bruce/coordinate/address.hpp"
#include <limits>

namespace bruce {
namespace {
using Wide=__uint128_t;
Wide triangular(std::uint64_t n) { return Wide(n)*(Wide(n)+1)/2; }
}
std::uint64_t pair(std::uint64_t x, std::uint64_t y) {
    const Wide sum=Wide(x)+y;
    // An address fitting uint64 cannot have a larger diagonal. This also
    // prevents multiplication overflow even for two maximum input values.
    if (sum>6074000999ULL) throw Error("CoordinateOverflow");
    const Wide result=sum*(sum+1)/2+y;
    if (result>std::numeric_limits<std::uint64_t>::max()) throw Error("CoordinateOverflow");
    return static_cast<std::uint64_t>(result);
}
Coordinate unpair(std::uint64_t z) {
    std::uint64_t low=0, high=6074001000ULL;
    while (low+1<high) {
        const auto mid=low+(high-low)/2;
        if (triangular(mid)<=z) low=mid; else high=mid;
    }
    const auto y=z-static_cast<std::uint64_t>(triangular(low));
    return {low-y,y};
}
std::uint8_t local_pair(std::uint64_t x, std::uint64_t y) {
    if (x>7 || y>7) throw Error("InvalidLocalCoordinate");
    return static_cast<std::uint8_t>(8*y+x);
}
Coordinate local_unpair(std::uint64_t z) {
    if (z>63) throw Error("InvalidLocalAddress");
    return {z%8,z/8};
}
GridPath parse_path(std::string_view text) {
    GridPath path;
    while (true) {
        const auto slash=text.find('/');
        const auto step=text.substr(0,slash);
        const auto comma=step.find(',');
        if (comma==std::string_view::npos) throw Error("InvalidGridPath: expected x,y/x,y");
        const Coordinate coordinate{integer<std::uint64_t>(step.substr(0,comma)),
                                    integer<std::uint64_t>(step.substr(comma+1))};
        (void)local_pair(coordinate.x,coordinate.y);
        path.steps.push_back(coordinate);
        if (path.steps.size()>64) throw Error("DepthBudgetExceeded");
        if (slash==std::string_view::npos) break;
        text.remove_prefix(slash+1);
    }
    return path;
}
}
