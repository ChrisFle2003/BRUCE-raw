#include "bruce/evidence/bundle.hpp"
#include <fstream>
#include <iostream>
#include <unistd.h>

int main() {
    using namespace bruce;
    char path[]="/tmp/bruce-read-control-XXXXXX";
    const int fd=mkstemp(path);
    if(fd<0)return 1;
    close(fd);
    try {
        for(const auto size:{0u,1u,8192u,8193u,24577u}) {
            const std::string data(size,'x');
            {std::ofstream out(path,std::ios::binary);out<<data;}
            std::size_t calls=0;
            const auto result=read_bounded(path,size,[&]{++calls;});
            if(result!=data || read_bounded(path,size)!=data || calls!=2+(size+8191)/8192)
                throw Error("Read checkpoint coverage mismatch");
            for(std::size_t stop=1;stop<=calls;++stop) {
                std::size_t seen=0;bool stopped=false;
                try {read_bounded(path,size,[&]{if(++seen==stop)throw Error("ReadStop");});}
                catch(const Error& e){stopped=std::string(e.what())=="ReadStop";}
                if(!stopped || seen!=stop)throw Error("Read callback swallowed");
            }
            if(size) {
                bool limited=false;
                try {read_bounded(path,size-1,[]{});}
                catch(const Error& e){limited=std::string(e.what())=="InputByteLimit";}
                if(!limited)throw Error("Read byte limit changed");
            }
        }
        unlink(path);
        bool stopped=false;
        try {read_bounded(path,0,[]{throw Error("BeforeOpen");});}
        catch(const Error& e){stopped=std::string(e.what())=="BeforeOpen";}
        if(!stopped)throw Error("Read opened before checkpoint");
        std::cout<<"Bounded read checkpoints passed\n";
    } catch(const std::exception& e){unlink(path);std::cerr<<e.what()<<'\n';return 1;}
}
