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

void lookupDependencies(std::string packageName, int column) {

// Variables 
    FILE *listPkgsLookUp;
    std::string depends;
    std::vector<std::string> pkges;


// Putting together the string command line
    // std::string clicommand = "apt-cache depends ";
    std::string clicommand = "dpkg -s ";
    clicommand += packageName;
    clicommand += " 2> /dev/null | grep -w '^Depends:'";

// Executing the string command line
    // std::cout << "testing - cmdline: " << clicommand.c_str() << "\n";
    listPkgsLookUp = popen(clicommand.c_str(), "r");
    if (!listPkgsLookUp) {
        std::cerr << "Failed to run command\n";
        return;
    }

// Moving the CLI output to a string to be easly managed
    char data[128];
    while(fgets(data, sizeof(data), listPkgsLookUp) != nullptr) {
        depends += (std::string)data;
    }

    // std::cout << "Testing - output: " << depends << "\n";

// Capture exit status. Capture exit code 2 ignoring code 1
    int output_status = pclose(listPkgsLookUp);
    if (output_status == -1) {
        std::cerr << "pclose faild\n";
        return;
    }

    int output_exit_code = WEXITSTATUS(output_status);
    if (output_exit_code != 0) {
        // std::cerr << "Command faild with exit code: " << output_exit_code << std::endl;
        if (output_exit_code == 2) {
            std::cerr <<"dpkg error: Pakage not found ro similar.\n";
        }
        return;
    }

// 
    if (depends.find("Depends: ") != std::string::npos){
        depends.replace(depends.find("Depends: "), std::string("Depends: ").length(), "");
        
        // Remove parentheses and their content
        std::regex rgx("\\([^)]*\\)");
        depends = std::regex_replace(depends, rgx, "");
        // Remove white spaces [ \t\s\r\f\v]
        std::regex rgx2("[\\s]*");
        depends = std::regex_replace(depends, rgx2, "");
        // Substitute "|" for ","
        if (depends.find("|") != std::string::npos){
            depends.replace(depends.find("|"), std::string("|").length(), ",");    
        }

        // Divide "depends" string by "," and store each word into pkges
        auto pos_ = depends.find(",");
        if (pos_ != std::string::npos) {
            do {
                pkges.push_back(depends.substr(0, pos_));
                depends.erase(0, ++pos_);
                pos_ = depends.find(",");
                if (pos_ == std::string::npos){
                    pkges.push_back(depends.substr(0, pos_));
                    break;
                }
            } while (pos_ != std::string::npos);
        }
        else
            pkges.push_back(depends);
        // std::cout << "\nTesting Dependencies 1: " << depends + " - " + pkges[0] << "\n";
        
        // Printing out a tree made of dependences with a given format
        for (std::string pkge : pkges){
            printWS(column);
            std::cout << column + 1 << " " + pkge << "\n";
            lookupDependencies(pkge, column + 1);
        }
    }
    else {
        std::cout << "No dependencies found." << std::endl;
        pclose(listPkgsLookUp);
    }

    // pclose(listPkgs);
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

    std::cout << "\nDependencies:" << std::endl;
    int column = 1;
    std::cout << column << " " << (std::string)argv[1] << std::endl;
    lookupDependencies((std::string)argv[1], column);

    return 0;
}