#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>


void lookupDpendencies(FILE **listPkgsLookUp, char *packageName[], std::string *depends) {

    std::vector<std::string> pkges;

    // std::string clicommand = "apt-cache depends ";
    std::string clicommand = "dpkg -s ";
    clicommand += packageName[1];
    // clicommand += " | grep '^Depends:\s' | cut -d : -f 2";
    clicommand += " | grep -w '^Depends:'";

    *listPkgsLookUp = popen(clicommand.c_str(), "r");
    if (!listPkgsLookUp) {
        std::cerr << "Failed to run command\n";
        return;
    }

    char data[128];
    while(fgets(data, sizeof(data), *listPkgsLookUp) != nullptr) {
        *depends += (std::string)data;
    }

    if (depends->find("Depends: ") != std::string::npos){
        depends->replace(depends->find("Depends: "), std::string("Depends: ").length(), "");

        // std::cout << "\nDependencies 1: " << *depends << "\n";
        
        std::regex rgx("\\([^)]*\\)");
        *depends = std::regex_replace(*depends, rgx, "");
        std::regex rgx2(" ?");
        *depends = std::regex_replace(*depends, rgx2, "");
        if (depends->find("|") != std::string::npos){
            depends->replace(depends->find("|"), std::string("|").length(), ",");    
        }

        auto pos_ = depends->find(",");
        while (pos_ != std::string::npos) {
            pkges.push_back(depends->substr(0, pos_));
            depends->erase(0, ++pos_);
            pos_ = depends->find(",");
            if (pos_ == std::string::npos){
                pkges.push_back(depends->substr(0, pos_));
                break;
            }
        }

        std::cout << "\nDependencies:" << std::endl;

        for (int i = 0; i < pkges.size(); i++){
            std::cout << i + 1 << " " << pkges[i] << "\n";
        }
    }
}

int main(int argc, char *argv[]) {
    
    // std::cout << argc << std::endl;
    // std::cout << argv[0] << std::endl;
    // std::cout << argv[1] << std::endl;
    // std::cout << argv[2] << std::endl;
    // std::cout << sizeof(*argv[0]) << std::endl;
    // std::cout << sizeof(*argv[1]) << std::endl;
    // std::cout << sizeof(*argv[2]) << std::endl;
    // std::cout << sizeof(*argv)/sizeof(*argv[0]) << std::endl;
    // std::cout << sizeof(argv)/sizeof(char);
    // return 0;

    FILE *listPkgs;
    std::string dependencies;
    lookupDpendencies(&listPkgs, argv, &dependencies);
    pclose(listPkgs);
    return 0;
}