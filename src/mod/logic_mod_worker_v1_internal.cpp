#include "logic_mod_worker_v1_internal.hpp"

#include "bruce/core/types.hpp"

#include <cstring>
#include <elf.h>

namespace bruce::logic_mod_worker_v1_internal {
namespace {
void static_elf(std::string_view bytes) {
    if (bytes.size() < sizeof(Elf64_Ehdr)) throw Error("LogicWorkerRequiresStaticELF");
    Elf64_Ehdr header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_phentsize != sizeof(Elf64_Phdr) ||
        header.e_phnum > 128 || header.e_phoff > bytes.size() ||
        header.e_phnum > (bytes.size() - header.e_phoff) / sizeof(Elf64_Phdr))
        throw Error("LogicWorkerRequiresStaticELF");
    for (unsigned index{}; index < header.e_phnum; ++index) {
        Elf64_Phdr program{};
        std::memcpy(&program, bytes.data() + header.e_phoff + index * sizeof(program), sizeof(program));
        if (program.p_type == PT_INTERP) throw Error("LogicWorkerRequiresStaticELF");
    }
}
} // namespace

void preflight_static_worker_v1(std::string_view worker_bytes) { static_elf(worker_bytes); }

} // namespace bruce::logic_mod_worker_v1_internal
