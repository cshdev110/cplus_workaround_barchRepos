// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Arango (github: cshdev110)

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <set>
#include <algorithm>

/* 
    * This program is designed to automatically resolve package conflicts for a full offline installation of BlackArch Linux.
    * Issue this program in a fresh installation. (Recomended)
    * Background:
    * After installing BlackArch full from the ISO (in VirtualBox, VMware, and virt-manager) and trying to update, it ended failing.
    * After installing BlackArch full from ISO choosing online type installation (in VirtualBox, VMware, and virt-manager), it also ended failing having a unresponsive desktop,
    * that only allows to manage fluxbox features.
    * This program uses pacman to identify conflicts and required by packages, and used recursion to resolve them.
    * It goes recursively updating/removing/reinstalling packages until all conflicts are resolved.
    * When packages are needed to be removed to resolve conflicts, they are stored in a set and reinstalled later.
    * 
    * Author: Daniel Arango (github: cshdev110)
    * Date: Jan 2026
    * 
 */

// Global variables
std::set<std::string> pkge_processed; // To keep track of processed packages. It avoids infinite loops and helps removing necessary packages in order.
std::set<std::string> removed_pkges; // To keep track of removed packages for reinstallation later.
std::string current_pkge_to_remove = ""; // To keep track of the current package to remove when dependencies are found.
bool remove_pkge = false; // Flag to indicate if a package needs to be removed.

// Logging tracking structures
std::set<std::string> log_removed_reinstalled; // Packages removed and reinstalled
std::set<std::string> log_removed_not_reinstalled; // Packages removed but not reinstalled
std::set<std::string> log_conflicts_resolved; // Packages in conflict that were resolved
std::set<std::string> log_requiredby_resolved; // Packages required-by that were resolved
std::set<std::string> log_not_found_in_repos; // Packages not found in repos
std::set<std::string> log_dependency_unsatisfy_removed; // Packages removed due to unsatisfied dependencies

// Regex patterns
/*
 * Pattern explanations:
 * 1. Conflict between two packages: "packageA and packageB are in conflict"
 * 2. Package required by another: "packageA required by packageB"
 * 3. File conflict: "path/to/file exists in filesystem (owned by packageA)"
 * 4. Package up to date: "is up to date --- reinstalling"
 * 5. Target not found: "target not found: packageA"
 * 6. Package was not found: "package 'packageA' was not found"
 * 7. Nothing to fix: "there is nothing to do". When no conflicts or issues are found. 
 */
std::regex pattern_rgx_conflict(R"((?!.*\[y/N\])(\S+)\s+and\s+(\S+) are in conflict)");
std::regex pattern_rgx_requiredby(R"((\S+)\s+required by\s+(\S+))");
//std::regex pattern_rgx_conflict_files(R"((\S+):\s(\S+)\s+exists in filesystem \(owned by)");
std::regex pattern_rgx_up_to_date(R"(\s*is up to date\s*-+\s*reinstalling)");
std::regex pattern_rgx_target_not_found(R"(\s*target not found:\s+(\S+))");
std::regex pattern_rgx_was_not_found(R"(\s+package '(\S+)' was not found)");
std::regex pattern_rgx_unable_to_satisfy_depen(R"(unable to satisfy dependency '(\S+)' required by\s+(\S+))");
std::regex pattern_rgx_nothing_to_fix(R"(.*there is nothing to do.*)");
// std::regex pattern_rgx_unknown_issue(R"(error:)");

// Enum for issue types
enum class IssueType {
    CONFLICT,
    REQUIRED_BY,
    // CONFLICT_FILES,
    TARGET_NOT_FOUND,
    DEPENDENCY_UNSATISFY,
    NOTHING_TO_FIX,
    UNKNOWN
};

// Enum for proceedure status
enum ProceedureStatus {
    NOTHING_TO_DO,
    CONFLICTS_RESOLVED,
    REQUIREDBY_RESOLVED,
    // FILE_CONFLICTS_RESOLVED,
    TARGET_NOT_FOUND_RESOLVED,
    DEPENDENCY_UNSATISFY_RESOLVED,
    INSTALLED_PACKAGE,
    PKGES_REQUIRED_TO_REMOVE,
    CONTINUE_PROCESSING,
    DONE,
    ERROR_OCCURRED 
};


// Function declarations
ProceedureStatus inspect_and_resolve_packages(std::string packageName); // Main function to inspect and resolve packages
std::string popen_exec(const std::string* clicommand); // Function to execute a command and return its output
void inspect_regex_and_resolve(std::string *depends, std::regex *pattern_rgx, IssueType isstype); // Function to inspect regex matches and resolve issues
std::string remove_package(std::string packageName); // Function to remove a package and its dependents
void write_log_file(const std::string& filename); // Function to write log file with all tracked actions


// Main function
int main(int argc, char *argv[]) {

    ProceedureStatus status;
    std::string commandline_input;

    // Sanitizing input
    if (argc > 2 || argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        std::cerr << "\nUsage: " << argv[0] << " [optional: package_name]" << "   :   Fix conflicts for a specific package" << std::endl;
        std::cerr << "Usage: " << argv[0] << " --fix" << "  :   Fix all conflicts automatically" << "\n\n";
        return EXIT_FAILURE;
    }

    // Checking if --fix flag is used, otherwise using the package name provided
    if (std::string(argv[1]) == "--fix") {
        commandline_input = "--fix";
    } else {
        commandline_input = std::string(argv[1]);
    }

    printf("\nRunning pacman to see packages in conflict...:\n\n");


    // Main loop to inspect and resolve packages and reinstall removed packages
    // It continues until there are no more conflicts or an error occurs
    do {
        // Reinstalling removed packages. If any package was removed, it will be reinstalled here.
        // Some packages might need to be re-removed if they are still causing conflicts.
        // This is done before inspecting packages again to ensure all dependencies are met.
        // Some other packages might not be possible to reinstall if they were removed due to being not found in the repositories.
        if (!removed_pkges.empty()) {
            printf("\nReinstalling removed packages...\n");

            std::string get_pgkes_info;
            std::set<std::string> pkges_to_skip;

            // Checking if any removed package was not found in the repositories
            // to avoid reinstalling it and causing errors
            for (const auto& pkge : removed_pkges) {
                get_pgkes_info = "pacman -Si " + pkge + " 2>&1";
                if (std::regex_search(popen_exec(&get_pgkes_info), pattern_rgx_was_not_found)) {
                    printf("[PACKAGE NOT FOUND] >> %s was not found in the repositories. Skipping reinstall.\n", pkge.c_str());
                    pkges_to_skip.insert(pkge);
                    log_not_found_in_repos.insert(pkge);
                }
            }

            for (const auto& pkge : pkges_to_skip) {
                removed_pkges.erase(pkge);
                log_removed_not_reinstalled.insert(pkge);
            }

            std::string reinstall_cmd = "sudo pacman -Sy --noconfirm ";
            for (const auto& pkge : removed_pkges) {
                reinstall_cmd += pkge + " ";
                log_removed_reinstalled.insert(pkge);
            }

            printf("\n[REINSTALLING] >> %s\n\n", reinstall_cmd.c_str());
            popen_exec(&reinstall_cmd);
            removed_pkges.clear();
            printf("\n[REINSTALLATION DONE]\n\n");

        }

        // Update Write log file
        write_log_file("fixConflicts.log");
        
        status = inspect_and_resolve_packages("--fix");

    } while (status != NOTHING_TO_DO && status != ERROR_OCCURRED);

    printf("\n[FINISHED]. All conflicts and required packages processed.\n\n");
    printf("If any package was removed, it has been reinstalled.\n");
    printf("Execute the program again if there are still conflicts.\n\n");
    printf("[YOU MIGHT WANT TO EXECUTE pacman -Syu --needed --overwrite=/*]\n");
    printf("[OR pacman -Syu --needed blackarch --overwrite=/* to install all tools]\n\n");

    return 0;
}


// Function to inspect and resolve packages based on the provided packagename
ProceedureStatus inspect_and_resolve_packages(std::string packageName) {

    // Variables 
    std::string depends; // To store the output of the pacman command
    std::string clicommand; // To store the command to be executed
    std::vector<std::string> pkges; // To store packages found in the output
    std::vector<std::string> general_or_package = {"-Syv", "-Syuv"}; // General command or package specific command

    // Removing package if already processed
    if (packageName == "--fix") {
        pkge_processed.clear();

    } else if (pkge_processed.count(packageName) > 0) {
        printf("\n[PKGE(S) REQUIRE(S) TO BE REMOVED] >> %s\nPrevious PKGES might need to be removed first.\n", packageName.c_str());

        current_pkge_to_remove = packageName; // Setting current package to remove

        remove_pkge = true;

        return PKGES_REQUIRED_TO_REMOVE;

    } else {
        // Marking package as processed to avoid infinite loops and removing them later if needed
        pkge_processed.insert(packageName); 
    }

    // Putting together the string command line
    if (packageName.find("--fix") != std::string::npos) {
        printf("\n[RESOLVING ALL CONFLICTS AUTOMATICALLY]\n\n");
        clicommand = "sudo pacman ";
        clicommand += general_or_package[1]; // "Syuv"
        clicommand += " ";
        clicommand += "--needed --noconfirm --overwrite=/*"; // To overwrite all files causing conflicts

    } else {
        printf("\n[RESOLVING FOR] >> %s\n\n", packageName.c_str());
        clicommand = "yes | sudo pacman ";
        clicommand += general_or_package[0]; // "Syv"
        clicommand += " ";
        clicommand += packageName;
        clicommand += " 2>&1";
    }
    
    depends = popen_exec(&clicommand);

    // Analyzing the output for conflicts or issues
	if (!depends.empty()){

        // Checking for different issues using regex patterns
        // Conflict between packages
        if (std::regex_search(depends, pattern_rgx_conflict)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_conflict, IssueType::CONFLICT);
            pkge_processed.clear();
            return CONFLICTS_RESOLVED;

        } 
        // Package required by another
        else if (std::regex_search(depends, pattern_rgx_requiredby) && !std::regex_search(depends, pattern_rgx_unable_to_satisfy_depen)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_requiredby, IssueType::REQUIRED_BY);
            if (remove_pkge) {
                return CONTINUE_PROCESSING;
            }
            pkge_processed.clear();
            return REQUIREDBY_RESOLVED;

        } 
        /* // File conflicts
        else if (std::regex_search(depends, pattern_rgx_conflict_files)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_conflict_files, IssueType::CONFLICT_FILES);
            pkge_processed.clear();
            return FILE_CONFLICTS_RESOLVED;

        }  */
        // Target not found. They might need to be removed.
        else if (std::regex_search(depends, pattern_rgx_target_not_found)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_target_not_found, IssueType::TARGET_NOT_FOUND);
            return TARGET_NOT_FOUND_RESOLVED;

        } 
        // Unable to satisfy dependency
        else if (std::regex_search(depends, pattern_rgx_unable_to_satisfy_depen)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_unable_to_satisfy_depen, IssueType::DEPENDENCY_UNSATISFY);
            pkge_processed.clear();
            return REQUIREDBY_RESOLVED;

        }
        // Nothing to fix
        else if (std::regex_search(depends, pattern_rgx_nothing_to_fix)) {
            inspect_regex_and_resolve(&depends, &pattern_rgx_nothing_to_fix, IssueType::NOTHING_TO_FIX);
            pkge_processed.clear();
            return NOTHING_TO_DO;

        } 
        // Package is already installed and up to date
        else if (std::regex_search(depends, pattern_rgx_up_to_date)) {
            printf("\n[UP TO DATE] >> %s is already installed and up to date.\n", packageName.c_str());
            pkge_processed.clear();
            return INSTALLED_PACKAGE;

        }
    }
    else {
        printf("[EMPTY OUTPUT].\n");
    }   
    
    // Final done message. If reached here, means no issues were found.
    printf("\n[DONE]\n\n");
    pkge_processed.clear();
    return DONE;
}


// Function to execute a command and return its output as a string
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
        printf("%s", data);
        output_cli += (std::string)data;
    }

    // Capture exit status. Capture exit code 2 ignoring code 1
    int output_status = pclose(listPkgsLookUp);
    if (output_status == -1) {
        std::cerr << "pclose faild\n";
        return "";
    }

    // Getting exit code from status
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


// Function to inspect regex matches and resolve issues based on issue type
void inspect_regex_and_resolve(std::string *depends, std::regex *pattern_rgx, IssueType isstype) {

    std::string remove_pkge_output; // To store output of remove package function
    ProceedureStatus proceedure_status; // To store status of the proceedure
    ProceedureStatus status;
    std::sregex_iterator findingMatches(depends->begin(), depends->end(), *pattern_rgx); // Iterator to find regex matches
    std::sregex_iterator end; // End iterator

    // Looping through all matches found.
    // Each match is handled based on the issue type.
    // As conflicts might have multiple required by packages, all matches are processed via this loop.
    while (findingMatches != end){
        switch (isstype) {

            // Conflict between packages
            case IssueType::CONFLICT:
                printf("\n[CONFLICT BETWEEN] >> %s and %s\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                
                // This loop continues until the conflict is resolved or a package is installed or target not found is resolved
                do {
                    status = inspect_and_resolve_packages(findingMatches->str(1));
                    
                } while ((status != DONE) && (status != INSTALLED_PACKAGE) && (status != TARGET_NOT_FOUND_RESOLVED));

                log_conflicts_resolved.insert(findingMatches->str(1));
                log_conflicts_resolved.insert(findingMatches->str(2));
                
                break;

            // Package required by another
            case IssueType::REQUIRED_BY:
                printf("\n[REQUIRED BY] >> %s required by %s\n\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                
                // Trying to resolve the required by issue.
                // It attempts to inspect and resolve the required package by updating, removing, or reinstalling it.
                // The loop means: first attempt to resolve the required package normally.
                // if there is a problem, it will solve it in the recursion before coming back and check if it is resolved.
                for (int attempt = 0; attempt < 2; ++attempt) {

                    //
                    if (removed_pkges.count(findingMatches->str(2)) == 0) {
                        status = inspect_and_resolve_packages(findingMatches->str(2));

                        if (status == DONE || status == TARGET_NOT_FOUND_RESOLVED) {
                            break;
                        }
                        
                        // This status indicates that the package has already processed and needs to be removed.
                        // And because of that, we return in the recursion to handle previous packages first
                        // before reaching this one again.
                        // When a package is already processed, it means that it has been inspected and needs to be removed
                        // before ending in a infinite loop. However, the removed packages are stored in a set and reinstalled later.
                        // This way, we ensure that all dependencies are met and conflicts are resolved in the correct order.
                        // After the main package that generated the conflicts is resolved, all removed packages are reinstalled.
                        if (status == PKGES_REQUIRED_TO_REMOVE) {
                            return;
                        }
                        // When a packages is already processed, it is marked for removal and previous packages are handled first.
                        // This make the remove_pkge flag to be set to true.
                        // So, with remove_pkge being true and making sure that the package exists in the pkge_processed set,
                        // we proceed to remove the package.
                        if (remove_pkge && (pkge_processed.count(findingMatches->str(2)) > 0)) {

                            remove_pkge_output = remove_package(findingMatches->str(2));

                            if (remove_pkge_output == "OK") {
                                status = DONE;
                                pkge_processed.erase(findingMatches->str(2));

                            } else if (remove_pkge_output == "ERROR") {
                                printf("\n[FAILED REMOVING PACKAGE] >> %s\n", current_pkge_to_remove.c_str());
                                exit(EXIT_FAILURE);
                            }

                            // Here, the first processed package that triggers the removal is handled
                            // and the flag is reset to false to avoid removing other packages unintentionally.
                            // This allows the main package to that generated the conflicts to be resolved,
                            // and then the removed packages are reinstalled later without any issues.
                            if (findingMatches->str(2) == current_pkge_to_remove) {
                                remove_pkge = false;
                                current_pkge_to_remove = "";
                            }
                        }

                    } else {
                        status = DONE;
                        break;
                    }
                } 
                switch (status) {
                    case DONE:
                        printf("\n[REQUIRED BY RESOLVED] >> %s required by %s has been resolved.\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                        break;

                    case INSTALLED_PACKAGE:
                        printf("\n[INSTALLED PACKAGE] >> %s is already installed.\n", findingMatches->str(2).c_str());
                        break;

                    case TARGET_NOT_FOUND_RESOLVED:
                        printf("\n[TARGET NOT FOUND RESOLVED] >> %s was not found and has been handled.\n", findingMatches->str(2).c_str());
                        break;

                    default:
                        break;
                }

                log_requiredby_resolved.insert(findingMatches->str(1));
                log_requiredby_resolved.insert(findingMatches->str(2));

                break;
            
            // Dependency unable to be satisfied because not found in repositories
            // It will be removed
            case IssueType::DEPENDENCY_UNSATISFY:
                printf("\n[DEPENDENCY UNSATISFIED] >> %s required by %s\n\n", findingMatches->str(1).c_str(), findingMatches->str(2).c_str());
                printf("[REMOVING PACKAGE] >> %s to resolve the unsatisfied dependency.\n", findingMatches->str(2).c_str());
                
                // Trying to remove the package that has the unsatisfied dependency
                remove_pkge_output = remove_package(findingMatches->str(2));
                if (remove_pkge_output == "OK") {
                    printf("\n[DEPENDENCY UNSATISFY RESOLVED] >> %s has been removed to resolve the unsatisfied dependency.\n", findingMatches->str(2).c_str());
                    removed_pkges.erase(findingMatches->str(2)); // Removing from removed packages set to avoid reinstalling it later
                    log_dependency_unsatisfy_removed.insert(findingMatches->str(2));

                } else if (remove_pkge_output == "ERROR") {
                    printf("\n[FAILED REMOVING PACKAGE] >> %s\n", findingMatches->str(2).c_str());
                    exit(EXIT_FAILURE);
                }
                break;

            /* case IssueType::CONFLICT_FILES:
                // str(2) is the file path
                printf("\n[CONFLICT FILES] >> %s exists in filesystem\n", findingMatches->str(2).c_str());
                printf("[REMOVING FILE] >> %s\n", findingMatches->str(1).c_str());

                {
                    // Scoped using {} to avoid bypassing variable initialization error
                    std::string removeFileCmd = "sudo rm -f " + findingMatches->str(2);
                    popen_exec(&removeFileCmd);
                }

                break; */

            // If target not found in repositories, remove the package
            case IssueType::TARGET_NOT_FOUND:
                {
                    printf("\n[TARGET NOT FOUND] >> %s - Desinstalling...\n", findingMatches->str(1).c_str());
                    std::string rm_output;
                    do {
                        rm_output = remove_package(findingMatches->str(1));
                    } while (rm_output != "OK" && rm_output != "NOT_INSTALLED");

                    log_not_found_in_repos.insert(findingMatches->str(1));
                    
                    break;
                }

            // When the full update finishes without issues
            case IssueType::NOTHING_TO_FIX:
                printf("\n[DONE]\n");
                break;

            default:
                break;
        }
        ++findingMatches;
    }
}


// Function to remove a package and its dependents
std::string remove_package(std::string packageName) {
    std::regex pattern_rgx_removing(R"(Required By\s+:\s+(.+))"); // To capture packages that require the target package
    std::smatch match;
    std::string clicommand = "pacman -Qi " + packageName + " 2>&1"; // Command to get package info
    std::vector<std::string> removed_pkges_requiredby; // To store 
    std::string rm_pkge_output;

    removed_pkges_requiredby.push_back(packageName);

    printf("\n[CHECKING DEPENDENCIES FOR] >> %s\n\n", packageName.c_str());

    std::string required_by_output = popen_exec(&clicommand); // Getting package info

    // Checking if package is installed, if not, return NOT_INSTALLED
    if (!std::regex_search(required_by_output, match, pattern_rgx_was_not_found)) {

        // Checking for packages that require the target package being removed
        if (std::regex_search(required_by_output, match, pattern_rgx_removing)) {

            std::string required_by = match.str(1); // Getting the required by packages

            // If there are packages depending on the target package, add them to the list for removal
            // These packages will be removed first before removing the target package
            // although they will be reinstalled later
            if (required_by != "None") {
                std::istringstream iss(required_by);
                std::string word;
                // Splitting the required by string into individual package names
                while (iss >> word) {
                    removed_pkges_requiredby.push_back(word);
                    printf("**** Marking package for removal: %s\n\n", word.c_str());
                }
            } else {
                printf("[REMOVING] >> No packages depending on: %s\n\n", packageName.c_str());
                std::string rm_pkge = "sudo pacman -R --noconfirm " + packageName + " 2>&1";

                removed_pkges.insert(packageName); // Adding package to removed packages set for reinstallation later

                for (int attempt = 0; attempt < 2; ++attempt) {
                    rm_pkge_output = popen_exec(&rm_pkge);
                }
                if (std::regex_search(rm_pkge_output, match, pattern_rgx_target_not_found)) {
                    printf("\n[PACKAGE UNINSTALLED] >> %s \n\n", packageName.c_str());
                    return "OK";
                } else {
                    return "ERROR";
                }
            }
        }

        // Reversing the order of packages to remove dependents first
        if (removed_pkges_requiredby.size() > 1) {
            // Reverse the order to remove dependents first before the target package
            // Using std::reverse from <algorithm>
            std::reverse(removed_pkges_requiredby.begin(), removed_pkges_requiredby.end());
        }
        
        // Removing packages that require the target package first
        for (const auto& pkge : removed_pkges_requiredby) {
            remove_package(pkge);
        }
        printf("[PACKAGE REMOVED] >> %s and its dependents were removed successfully.\n", packageName.c_str());
        return "OK";

    } else {
        printf("[PACKAGE NOT INSTALLED] >> %s was not found in the system.\n", packageName.c_str());
        return "NOT_INSTALLED";
    }
}


// Function to write all logged actions to a file
void write_log_file(const std::string& filename) {
    FILE *logFile = fopen(filename.c_str(), "w");
    if (!logFile) {
        std::cerr << "Failed to create log file: " << filename << "\n";
        return;
    }

    fprintf(logFile, "=== Package Conflict Resolution Log ===\n");
    fprintf(logFile, "Date: %s\n\n", __DATE__);

    // Packages removed and reinstalled
    fprintf(logFile, "[PACKAGES REMOVED AND REINSTALLED]\n");
    if (!log_removed_reinstalled.empty()) {
        for (const auto& pkg : log_removed_reinstalled) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    // Packages removed but not reinstalled
    fprintf(logFile, "[PACKAGES REMOVED BUT NOT REINSTALLED]\n");
    if (!log_removed_not_reinstalled.empty()) {
        for (const auto& pkg : log_removed_not_reinstalled) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    // Packages in conflict and resolved
    fprintf(logFile, "[PACKAGES IN CONFLICT AND RESOLVED]\n");
    if (!log_conflicts_resolved.empty()) {
        for (const auto& pkg : log_conflicts_resolved) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    // Packages required-by and resolved
    fprintf(logFile, "[PACKAGES REQUIRED-BY AND RESOLVED]\n");
    if (!log_requiredby_resolved.empty()) {
        for (const auto& pkg : log_requiredby_resolved) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    // Packages not found in repos
    fprintf(logFile, "[PACKAGES NOT FOUND IN REPOS]\n");
    if (!log_not_found_in_repos.empty()) {
        for (const auto& pkg : log_not_found_in_repos) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    // Dependencies unsatisfied
    fprintf(logFile, "[DEPENDENCIES UNSATISFIED AS NOT FOUND IN REPOS]\n");
    if (!log_dependency_unsatisfy_removed.empty()) {
        for (const auto& pkg : log_dependency_unsatisfy_removed) {
            fprintf(logFile, "  - %s\n", pkg.c_str());
        }
    } else {
        fprintf(logFile, "  (none)\n");
    }
    fprintf(logFile, "\n");

    fprintf(logFile, "=== End of Log ===\n");
    fclose(logFile);
    printf("\n[LOG FILE UPDATED] >> %s\n", filename.c_str());
}