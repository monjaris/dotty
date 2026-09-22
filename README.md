# dotty

`dotty` is a simple-to-use config and dotfiles manager with profile support, written in C++23

## Features

- **Simple Configuration**: Use a straightforward syntax to map source files to their destinations.
- **Profile Management**: Supports multiple configuration profiles (e.g., `main`, `wm`, `terminal`).
- **GitHub Integration**: Creates & synces a GitHub repository for your dotfiles

## Prerequisites

Before building and using `dotty`, ensure you have the following installed:

- **Dependencies**:
  - [CLI11](https://github.com/CLIUtils/CLI11) For command line parsing
  - [github-cli](https://cli.github.com/) For repository management

- **Optional**:
  - [bat](https://github.com/sharkdp/bat) For file logging (optional)


## Installation

### Available in AUR
```bash
yay -S dotty
# paru -S dotty   # if you use paru
```


### Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/Monjaris/dotty
   cd dotty
   ```

2. You can build dotty with xmake:
   ```bash
   ./build.sh   # calls xmake under the hood
   ```

3. Install the binary:
   ```bash
   xmake install
   ```



## Usage


### 1. Create a profile

There is no separate initialization step. Creating or importing a profile creates
Dotty's local directories and master configuration automatically.

Create a new GitHub-backed profile on this machine:

```bash
dotty profile new --name main --repo my-dotfiles --commit-msg "Initial dotfiles"
```

On another machine, import the existing repository and apply all of its mappings
in one command (GitHub CLI authentication is not required for a public repo):

```bash
dotty profile import --name main --url https://github.com/you/my-dotfiles
```

Imported profiles never delete their remote repository. The command validates the
remote before it changes local Dotty configuration, then downloads the repository,
restores its profile config, and applies it.

### 2. Configuration

`dotty` looks for configuration in `~/.config/dotty/<active-profile>/config`. 

The configuration file uses a simple mapping syntax:
```dotty
# copy file
"/path/to/source/file" >> "relative/path/in/repo"
# copy directory
"/path/to/source/dir" >>* "relative/path/in/repo"
# link file
"/path/to/source/file" -> "relative/path/in/repo"
# link directory
"/path/to/source/dir" ->* "relative/path/in/repo"
```


Example:
```
"~/.bashrc" >> "shell/.bashrc"
"~/.config/hyprland" >>* "wm/hyprland"
"~/.config/nvim/init.lua" -> "nvim/.."  # '..' expands to "init.lua"
```


### 3. Applying configs

To sync your mapped files into the repo locally:
```bash
dotty update # dotty u
```

Push repo to github repository
```bash
dotty push
```

Pull your configs and apply so we can acquire them on another machine or to rollback
```bash
dotty pull
```

Dotty supports multiple profiles. To create another one:
```bash
dotty profile new --name terminal-configs --repo terminal-configs --commit-msg "Initial configs" # dotty p n
```

You can switch between them
```bash
dotty profile switch main # dotty p s main
```

Or you can delete them
```bash
dotty profile delete terminal-configs # dotty p d terminal-configs
```

You can also directly open configuration instead of the manual way
```bash
dotty config # dotty c
dotty config -e vim # use vim to edit the config
```


### My Development Preferences and C++ conventions

- **Variables/Constants/Members**: `snake_case`
- **Macros / Constexprs**: `UPPER_SNAKE_CASE`
- **Functions/Methods**: `camelCase`
- **Classes/Typedefs**: `PascalCase`

Also .h is for C headers, C++ headers are *.hpp only.

### Primary project files

- `src/main.cpp`: Entry point.
- `core/include/core.hpp`: Common utility functions and std-lib wrappers.
- `include/common.hpp`: Global definitions, types, and includes, it's compiled to a PCH via xmake.


### License

This project is licensed under the GNU General Public License v3.0. For the full text of the license and terms of use, please refer to the [LICENSE](LICENSE) file.
