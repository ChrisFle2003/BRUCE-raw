#include "bruce/evidence/bundle.hpp"
#include <openssl/evp.h>
#include <array>
#include <algorithm>
#include <memory>
#include "bruce/io/local_files.hpp"

namespace bruce {
std::string sha256(std::string_view value,const ReplayCheckpoint& checkpoint) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};
    unsigned size=0;
    if(!checkpoint) {
        if(EVP_Digest(value.data(),value.size(),digest.data(),&size,EVP_sha256(),nullptr)!=1)
            throw Error("DigestFailure");
    } else {
        std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),EVP_MD_CTX_free);
        if(!context || EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1) throw Error("DigestFailure");
        constexpr std::size_t block_bytes=65536;
        for(std::size_t offset=0;offset<value.size();) {
            checkpoint();
            const auto count=std::min(block_bytes,value.size()-offset);
            if(EVP_DigestUpdate(context.get(),value.data()+offset,count)!=1) throw Error("DigestFailure");
            offset+=count;
        }
        checkpoint();
        if(EVP_DigestFinal_ex(context.get(),digest.data(),&size)!=1) throw Error("DigestFailure");
    }
    constexpr char hex[]="0123456789abcdef";
    std::string result;
    for(unsigned i=0;i<size;++i) {
        result+=hex[digest[i]>>4]; result+=hex[digest[i]&15];
    }
    return result;
}
std::string read_bounded(const std::string& path,std::size_t limit,const ReplayCheckpoint& checkpoint) {
    return LocalFiles::read(path,limit,checkpoint);
}
void export_exclusive(const std::string& path,std::string_view content,const ReplayCheckpoint& checkpoint) {
    LocalFiles::publish_new(path,content,checkpoint);
}
}
