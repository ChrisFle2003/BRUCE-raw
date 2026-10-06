#include "bruce/core/json.hpp"
namespace bruce {
std::string json_string(std::string_view text) {
    constexpr char hex[]="0123456789abcdef";
    std::string value="\"";
    for(std::size_t i=0;i<text.size();++i) {
        const auto c=static_cast<unsigned char>(text[i]);
        if(c>=128) {
            // Keep valid UTF-8; diagnostic strings can also contain arbitrary
            // filename/input bytes, which must not invalidate the JSON envelope.
            const std::size_t length=c>=0xc2 && c<=0xdf ? 2 : c>=0xe0 && c<=0xef ? 3 : c>=0xf0 && c<=0xf4 ? 4 : 0;
            bool valid=length!=0 && length<=text.size()-i;
            if(valid) {
                for(std::size_t j=1;j<length;++j) {
                    const auto next=static_cast<unsigned char>(text[i+j]);
                    valid=valid && next>=0x80 && next<=0xbf;
                }
                const auto second=static_cast<unsigned char>(text[i+1]);
                valid=valid && !(c==0xe0 && second<0xa0) && !(c==0xed && second>=0xa0)
                            && !(c==0xf0 && second<0x90) && !(c==0xf4 && second>=0x90);
            }
            if(valid) { value.append(text.substr(i,length)); i+=length-1; }
            else { value+="\\u00"; value+=hex[c>>4]; value+=hex[c&15]; }
            continue;
        }
        if(c=='"' || c=='\\') { value+='\\'; value+=static_cast<char>(c); }
        else if(c<32) { value+="\\u00"; value+=hex[c>>4]; value+=hex[c&15]; }
        else value+=static_cast<char>(c);
    }
    return value+'"';
}
}
