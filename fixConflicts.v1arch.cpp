#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <set>
#include <algorithm>

// Global variables
std::set<std::string> pkge_processed;
std::vector<std::string> removed_pkges;

// Regex patterns
std::regex pattern_rgx_conflict(R"((?!.*\[y,N\])(\S+)\s+and\s+(\S+) are in conflict)");
std::regex pattern_rgx_requiredby(R"((\S+)\s+required by\s+(\S+))");
std::regex pattern_rgx_conflict_files(R"((\S+):\s(\S+)\s+exists in filesystem \(owned by)");
std::regex pattern_rgx_target_not_found(R"(error: target not found:\s+(\S+))");
std::regex pattern_rgx_nothing_to_fix(R"(there is nothing to do)");
std::regex pattern_rgx_unknown_issue(R"(error:)");

// Enum for issue types
enum class IssueType {
    CONFLICT,
    REQUIRED_BY,
    CONFLICT_FILES,
    TARGET_NOT_FOUND,
    NOTHING_TO_FIX,
    UNKNOWN
};

enum ProceedureStatus {
    NOTHING_TO_DO = 0,
    CONFLICTS_RESOLVED = 1,
    REQUIREDBY_RESOLVED = 2,
    FILE_CONFLICTS_RESOLVED = 3,
    TARGET_NOT_FOUND_RESOLVED = 4,
    INSTALLED_PACKAGE = 5,
    DONE = 8,
    ERROR_OCCURRED = -1
};


// Function declarations
ProceedureStatus inspect_and_resolve_packages(std::string packageName);
std::string popen_exec(const std::string* clicommand);
void inspect_regex_and_resolve(std::string *depends, std::regex *pattern_rgx, IssueType isstype);
std::string remove_package(std::string packageName);


int main(int argc, char *argv[]) {
    
    printf("\nRunning pacman to see packages in conflict...:\n");
    inspect_and_resolve_packages((std::string)argv[1]);

    // Installing removed packages again
    if (!removed_pkges.empty()) {
        printf("\nReinstalling removed packages...\n");
        std::string reinstall_cmd = "sudo pacman -Sy --noconfirm ";
        for (const auto& pkge : removed_pkges) {
            reinstall_cmd += pkge + " ";
        }
        popen_exec(&reinstall_cmd);
    }

    printf("\n[FINISHED]. All conflicts and required packages processed.\n\n");
    printf("If any package was removed, it has been reinstalled.\n");
    printf("You may want to run 'sudo pacman -Syu' to ensure system is up to date.\n");
    printf("Execute the program again if there are still conflicts.\n\n");

    return 0;
}


ProceedureStatus inspect_and_resolve_packages(std::string packageName) {

    // Variables 
    std::string depends;
    std::string clicommand;
    std::vector<std::string> pkges;
    std::vector<std::string> general_or_package = {"-Syv", "-Syuv"}; // first for required by, second for conflicts

    // Removing package if already processed
    if (packageName == "--fix") {
        pkge_processed.clear();

    } else if (pkge_processed.count(packageName) > 0) {
        printf("[ALREADY PROCESSED] >> %s%s\n", packageName.c_str(), ". Desinstalling...");

        remove_package(packageName);
        return DONE;

    } else {
        pkge_processed.insert(packageName);
    }

    // Putting together the string command line
    
    if (packageName.find("--fix") != std::string::npos) {
        clicommand = "sudo pacman ";
        clicommand += general_or_package[1]; // "Syuv"
        clicommand += " ";
        clicommand += "--noconfirm 2>/dev/null";

    } else {
        printf("\n[RESOLVING FOR] >> %s\n\n", packageName.c_str());
        clicommand = "yes | sudo pacman ";
        clicommand += general_or_package[0]; // "Syv"
        clicommand += " ";
        clicommand += packageName;
        // clicommand += " 2>/dev/null";
    }
    
    depends = popen_exec(&clicommand);
 
	if (!depends.empty()){

        // Pattern for pkges that are already installed and up to date
        std::regex pattern_rgx_up_to_date(R"(is up to date)");

        if (std::regex_search(depends, pattern_rgx_conflict)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_conflict, IssueType::CONFLICT);
            return CONFLICTS_RESOLVED;

        } else if (std::regex_search(depends, pattern_rgx_requiredby)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_requiredby, IssueType::REQUIRED_BY);
            return REQUIREDBY_RESOLVED;

        } else if (std::regex_search(depends, pattern_rgx_conflict_files)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_conflict_files, IssueType::CONFLICT_FILES);
            return FILE_CONFLICTS_RESOLVED;

        } else if (std::regex_search(depends, pattern_rgx_target_not_found)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_target_not_found, IssueType::TARGET_NOT_FOUND);
            return TARGET_NOT_FOUND_RESOLVED;

        } else if (std::regex_search(depends, pattern_rgx_nothing_to_fix)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_nothing_to_fix, IssueType::NOTHING_TO_FIX);
            return NOTHING_TO_DO;

        } else if (std::regex_search(depends, pattern_rgx_up_to_date)) {
            printf("\n[UP TO DATE] >> %s is already installed and up to date.\n", packageName.c_str());
            return INSTALLED_PACKAGE;

        }
        else if (std::regex_search(depends, pattern_rgx_unknown_issue)) {
            printf("\n[UNKNOWN ISSUE] >> An unknown issue was detected in pacman output. Please check manually.\n");
            return ERROR_OCCURRED;
        }
    }
    else {
        printf("Empty pacman output.\n");
    }    

    return DONE;
}


std::string popen_exec(const std::string* clicommand) {
    // Executing the string command line
    // std::cout << "\n[COMMAND]: " << clicommand.c_str() << "\n";
    FILE *listPkgsLookUp = popen(clicommand->c_str(), "r");
    if (!listPkgsLookUp) {
        std::cerr << "Failed to run command\n";
        return "";
    }

    // Moving the CLI output to a string to be easly managed
    // This while loop needs to be executed before closing the pipe with pclose
    // otherwise no output is captured
    char data[1024];
    std::string output_cli;
    while(fgets(data, sizeof(data), listPkgsLookUp) != nullptr) {
        output_cli += (std::string)data;
        printf("%s", data);
    }
    printf("\n");

    // Capture exit status. Capture exit code 2 ignoring code 1
    int output_status = pclose(listPkgsLookUp);
    if (output_status == -1) {
        std::cerr << "pclose faild\n";
        return "";
    }

    int output_exit_code = WEXITSTATUS(output_status);
    if (output_exit_code != 0) {
        // std::cerr << "Command faild with exit code: " << output_exit_code << std::endl;
        if (output_exit_code == 2) {
            std::cerr <<"pacman error: Pakage not found or similar.\n";
        }
        // printf("Exit code: %d\n\n", output_exit_code);
    }
    return output_cli;
}

void inspect_regex_and_resolve(std::string *depends, std::regex *pattern_rgx, IssueType isstype) {

    ProceedureStatus proceedure_status;
    std::sregex_iterator findingMatches(depends->begin(), depends->end(), *pattern_rgx);
    std::sregex_iterator end;

    while (findingMatches != end){
        switch (isstype) {
            case IssueType::CONFLICT:
                printf("\n[CONFLICT BETWEEN] >> %s and %s\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                ProceedureStatus status;
                do {
                    status = inspect_and_resolve_packages(findingMatches->str(1));
                    
                } while ((status != DONE) && (status != INSTALLED_PACKAGE) && (status != TARGET_NOT_FOUND_RESOLVED));
                break;

            case IssueType::REQUIRED_BY:
                printf("[REQUIRED BY] >> %s required by %s\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                do {
                    status = inspect_and_resolve_packages(findingMatches->str(2));
                    
                } while ((status != DONE) && (status != INSTALLED_PACKAGE) && (status != TARGET_NOT_FOUND_RESOLVED));
                break;

            case IssueType::CONFLICT_FILES:
                // str(2) is the file path
                printf("\n[CONFLICT FILES] >> %s exists in filesystem\n", findingMatches->str(2).c_str());
                printf("Removing file: %s\n", findingMatches->str(1).c_str());

                {
                    // Scoped using {} to avoid bypassing variable initialization error
                    std::string removeFileCmd = "sudo rm -f " + findingMatches->str(2);
                    popen_exec(&removeFileCmd);
                }

                break;

            case IssueType::TARGET_NOT_FOUND:
                {
                    printf("\n[TARGET NOT FOUND] >> %s - Desinstalling...\n", findingMatches->str(1).c_str());
                    std::string rm_output;
                    do {
                        rm_output = remove_package(findingMatches->str(1));
                    } while (!std::regex_search(rm_output, pattern_rgx_target_not_found));
                    
                    break;
                }

            case IssueType::NOTHING_TO_FIX:
                printf("\n[DONE]\n");
                break;

            default:
                break;
        }
        ++findingMatches;
    }
}


std::string remove_package(std::string packageName) {
    std::regex pattern_rgx_removing(R"(Required By\s+:\s+(.+))");
    std::smatch match;
    std::string clicommand = "pacman -Qi " + packageName;
    std::vector<std::string> removed_pkges_requiredby;
    std::string popen_output;

    removed_pkges_requiredby.push_back(packageName);

    printf("[CHECKING DEPENDENCIES FOR: ] >> %s\n", packageName.c_str());

    std::string required_by_output = popen_exec(&clicommand);
    if (std::regex_search(required_by_output, match, pattern_rgx_removing)) {
        std::string required_by = match.str(1);

        if (required_by != "None") {
            std::istringstream iss(required_by);
            std::string word;
            while (iss >> word) {
                removed_pkges_requiredby.push_back(word);
                printf("Marking package for removal: %s\n", word.c_str());
            }
        } else {
            printf("No packages depend on. Removing... %s\n", packageName.c_str());
            std::string rm_pkge = "sudo pacman -Rc --noconfirm " + packageName;
            removed_pkges.push_back(packageName);
            return popen_exec(&rm_pkge).c_str();
        }
    }

    if (removed_pkges_requiredby.size() > 1) {
        // Reverse the order to remove dependents first
        std::reverse(removed_pkges_requiredby.begin(), removed_pkges_requiredby.end());
    }
    
    for (const auto& pkge : removed_pkges_requiredby) {
        remove_package(pkge);
    }
    return "";
}