# A Workaround For BlackArch Linux Pacman Conflict Resolver

A workaround automated tool to resolve package conflicts in BlackArch Linux by recursively detecting, removing, and reinstalling conflicting packages.
**Far from perfect but worked for a fresh full offline ISO installation of BlackArch Linux.**

## 🎯 What It Does

This tool automatically handles common pacman conflicts that can occur during system updates, particularly after fresh BlackArch installations. It:

- **Detects package conflicts** using regex pattern matching on pacman output
- **Resolves dependency chains** recursively to find and remove conflicting packages
- **Tracks removed packages** and automatically reinstalls them after conflicts are resolved
- **Handles multiple conflict types**:
  - Package vs package conflicts
  - Required-by dependency issues
  - File system conflicts
  - Target-not-found errors

## 🚀 Why Use This?

Manual conflict resolution in pacman can be tedious and error-prone, especially when:
- Fresh BlackArch installations fail to update
- Dependency chains cause cascading conflicts
- You need to remove and reinstall multiple packages in the correct order

Background:
* After installing BlackArch full from the ISO (in VirtualBox, VMware, and virt-manager) and trying to update, it ended failing.
* After installing BlackArch full from ISO but choosing online type installation (in VirtualBox, VMware, and virt-manager), it also ended failing having a unresponsive desktop, that only allows to manage fluxbox features.

This workaround automates the process, saving hours of manual troubleshooting.

## 📋 Requirements

- **OS**: Blackarch Linux
- **Compiler**: g++ with C++17 support
- **Permissions**: sudo access (the tool runs pacman commands)
- **Dependencies**: Standard C++ libraries (regex, set, algorithm)

## 🔨 Compilation

```bash
g++ -std=c++17 -g fixConflicts.v1arch.cpp -o fixConflicts
```

## 💻 Usage

### Basic usage (auto-fix all conflicts):
```bash
sudo ./fixConflicts --fix
```

### Help:
```bash
./fixConflicts --help
```

### Output:
The tool provides detailed logging:
- `[CONFLICT BETWEEN]` - Shows detected package conflicts
- `[REQUIRED BY]` - Shows dependency chains
- `[REMOVING]` - Packages being removed to resolve conflicts
- `[REINSTALLING]` - Packages being reinstalled after resolution
- `[DONE]` - Conflict resolution complete

## ⚠️ Important Warnings

**Use at your own risk!** This tool:
- First, create your **VM snapshot**. It might be needed for multiple attemps
- Runs with `sudo` privileges
- Removes and reinstalls packages automatically
- May cause system instability if interrupted
- Is designed primarily for **fresh installations** or testing environments

**Recommendations:**
- ✅ Use on fresh **full offline BlackArch ISO installations** (recommended scenario)
- ✅ Backup important data before running
- ✅ Run in a VM or test environment first
- ❌ Avoid interrupting the process (Ctrl+C)
- ❌ Not recommended for production systems with critical data

## 🛠️ How It Works

1. **Detection Phase**: Runs `pacman -Syuv` to detect conflicts
2. **Analysis Phase**: Uses regex patterns to identify conflict types
3. **Resolution Phase**: Recursively removes conflicting packages (tracks them in a set)
4. **Reinstallation Phase**: Reinstalls all removed packages after conflicts are resolved
5. **Repeat**: Loops until no conflicts remain

### Key Features:
- **Cycle Detection**: Uses `std::set<std::string>` to track processed packages and avoid infinite loops
- **Order Management**: Reverses dependency chains to remove dependents before dependencies
- **Safe Reinstall**: Checks package availability before reinstalling (skips packages not in repos)

## 📁 Structure

```
.
├── fixConflicts.v1arch.cpp  # Main source code
├── README.md                # This file
└── LICENSE                  # MIT License
```

## 🐛 Known Issues & Limitations

- **File conflicts**: Currently commented out; may need manual intervention
- **Non-standard repos**: May not work with custom/AUR packages
- **Large dependency chains**: Can take significant time to resolve
- **Network dependency**: Requires active internet connection for pacman operations
- **Far from perfect it just a workaround**

## 🤝 Contributions are welcome!

## 📜 License

MIT License - See [LICENSE](LICENSE) file for details.

Copyright (c) 2026 Daniel Arango

## 👨‍💻 Author

**Daniel Arango** ([@cshdev110](https://github.com/cshdev110))

Created to solve persistent update conflicts in BlackArch installations.

## 🙏 Acknowledgments

- BlackArch communities
- Pacman development team

---

**Disclaimer**: This is an automated tool that modifies system packages. Always maintain backups and use in testing environments when possible.
