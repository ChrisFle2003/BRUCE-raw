#include "bruce/evidence/bundle.hpp"
#include <filesystem>
#include <iostream>
#include <unistd.h>
int main(){
    using namespace bruce;
    char directory[]="/tmp/bruce-export-control-XXXXXX";
    if(!mkdtemp(directory))return 1;
    const auto path=std::string(directory)+"/output";
    try {
        for(const auto size:{0u,1u,65536u,65537u,196609u}){
            const std::string data(size,'x');std::size_t points=0;
            export_exclusive(path,data,[&]{++points;if(std::filesystem::exists(path))throw Error("Checkpoint after publication");});
            if(read_bounded(path)!=data || points<3+(size+65535)/65536)throw Error("Export coverage mismatch");
            std::filesystem::remove(path);
            for(std::size_t stop=1;stop<=points;++stop){
                std::size_t seen=0;bool stopped=false;
                try {export_exclusive(path,data,[&]{if(++seen==stop)throw Error("ExportStop");});}
                catch(const Error& e){stopped=std::string(e.what())=="ExportStop";}
                if(!stopped || seen!=stop || !std::filesystem::is_empty(directory))throw Error("Export cancellation leaked output or temporary file");
            }
            export_exclusive(path,"original");
            bool refused=false;
            try {export_exclusive(path,data,[]{});}
            catch(const Error& e){refused=std::string(e.what()).starts_with("ExportExistsOrUnavailable");}
            if(!refused || read_bounded(path)!="original")throw Error("Export overwrote destination");
            std::filesystem::remove(path);
            if(!std::filesystem::is_empty(directory))throw Error("Collision leaked temporary file");
        }
        std::filesystem::remove(directory);
        std::cout<<"Controlled export boundaries and cleanup passed\n";
    }catch(const std::exception& e){std::filesystem::remove_all(directory);std::cerr<<e.what()<<'\n';return 1;}
}
