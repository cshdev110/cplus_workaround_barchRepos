#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>

// Print whitespace
void printWS(int sp){
    do {
        std::cout << " ";
        --sp;
    } while(sp != 0);
}

void lookupDependencies(FILE **listPkgsLookUp, std::string packageName) {

    std::string depends;
    std::vector<std::string> pkges;

    // std::string clicommand = "apt-cache depends ";
    std::string clicommand = "dpkg -s ";
    clicommand += packageName;
    // clicommand += " | grep '^Depends:\s' | cut -d : -f 2";
    clicommand += " | grep -w '^Depends:'";

    *listPkgsLookUp = popen(clicommand.c_str(), "r");
    if (!listPkgsLookUp) {
        std::cerr << "Failed to run command\n";
        return;
    }

    char data[128];
    while(fgets(data, sizeof(data), *listPkgsLookUp) != nullptr) {
        depends += (std::string)data;
    }

    if (depends.find("Depends: ") != std::string::npos){
        depends.replace(depends.find("Depends: "), std::string("Depends: ").length(), "");

        // std::cout << "\nDependencies 1: " << *depends << "\n";
        
        // Remove parentheses and their content
        std::regex rgx("\\([^)]*\\)");
        depends = std::regex_replace(depends, rgx, "");
        // Remove white spaces
        std::regex rgx2(" ?");
        depends = std::regex_replace(depends, rgx2, "");
        // Substitute "|" for ","
        if (depends.find("|") != std::string::npos){
            depends.replace(depends.find("|"), std::string("|").length(), ",");    
        }

        // Divide "depends" string by "," and store each word into pkges
        auto pos_ = depends.find(",");
        while (pos_ != std::string::npos) {
            pkges.push_back(depends.substr(0, pos_));
            depends.erase(0, ++pos_);
            pos_ = depends.find(",");
            if (pos_ == std::string::npos){
                pkges.push_back(depends.substr(0, pos_));
                break;
            }
        }

        // Printing out a tree made of dependences with a given format
        std::cout << "\nDependencies:" << std::endl;
        int top = 1;
        std::cout << top << " " << packageName << std::endl;

        for (std::string pkge : pkges){
            printWS(top);
            std::cout << ++top << " " + pkge << "\n";
            lookupDependencies(listPkgsLookUp, pkge);
        }
    }
}

int main(int argc, char *argv[]) {
    
    // std::cout << argc << std::endl;
    // std::cout << argv[0] << std::endl;
    // std::cout << (std::string)argv[1] << std::endl;
    // std::cout << argv[2] << std::endl;
    // std::cout << sizeof(*argv[0]) << std::endl;
    // std::cout << sizeof(*argv[1]) << std::endl;
    // std::cout << sizeof(*argv[2]) << std::endl;
    // std::cout << sizeof(*argv)/sizeof(*argv[0]) << std::endl;
    // std::cout << sizeof(argv)/sizeof(char);
    // return 0;

    FILE *listPkgs;

    lookupDependencies(&listPkgs, (std::string)argv[1]);

    pclose(listPkgs);
    return 0;
}