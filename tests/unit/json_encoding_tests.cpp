#include "bruce/core/json.hpp"
#include <string>
int main() {
    using bruce::json_string;
    if(json_string("")!="\"\"" || json_string("\"\\")!="\"\\\"\\\\\"")return 1;
    const std::string utf8="Grüße 😀";
    if(json_string(utf8)!="\""+utf8+"\"")return 2;
    const char* hex="0123456789abcdef";
    for(unsigned c=0;c<32;++c){
        std::string expected="\"\\u00";expected+=hex[c>>4];expected+=hex[c&15];expected+='"';
        if(json_string(std::string(1,static_cast<char>(c)))!=expected)return 3;
    }
    if(json_string(std::string("\xc0\x80",2))!="\"\\u00c0\\u0080\"")return 4;
    if(json_string(std::string("\xf0\x9f",2))!="\"\\u00f0\\u009f\"")return 5;
    if(json_string(std::string("\xed\xa0\x80",3))!="\"\\u00ed\\u00a0\\u0080\"")return 6;
}
