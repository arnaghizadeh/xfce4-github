# XFCE Desktop Environment - Enhanced Edition

This repository contains modified versions of the XFCE panel and Thunar file manager with several usability enhancements and modern features.

## Features

### Thunar File Manager Enhancements

1. **Native Archive Support**: Compress and extract archives directly from the context menu without external tools
2. **Breadcrumb-Style Path Bar**: Modern breadcrumb navigation with a toggle button to switch between traditional and breadcrumb views
3. **Click-to-Rename**: Rename files using slow double-click on the filename (like in Windows Explorer)

### XFCE Panel Enhancements

1. **Recent Files Feature**: Access recently used files directly from the panel tasklist

## Prerequisites

Before building, ensure you have the following dependencies installed:

### Build Tools
```bash
sudo apt install build-essential meson ninja-build pkg-config git
```

### XFCE Dependencies
```bash
sudo apt install \
    libgtk-3-dev \
    libglib2.0-dev \
    libxfce4util-dev \
    libxfce4ui-2-dev \
    xfce4-dev-tools \
    libexo-2-dev \
    libgarcon-1-0-dev \
    libwnck-3-dev \
    libdbus-1-dev \
    libdbus-glib-1-dev \
    libstartup-notification0-dev \
    libthunarx-3-dev \
    libnotify-dev \
    libgudev-1.0-dev \
    libxfconf-0-dev
```

### Archive Support (for Thunar compression features)
```bash
sudo apt install \
    libarchive-dev \
    file-roller \
    p7zip-full \
    unzip \
    zip
```

## Building from Source

### How This Replaces Your System Defaults

This installation uses **`~/.local`** as the installation prefix (or `/usr/local` for system-wide installation). Here's how this automatically replaces your system defaults:

1. **PATH Precedence**: `~/.local/bin` comes before `/usr/bin` in your system's PATH
2. **No Conflicts**: Your package manager (apt/dpkg) never touches `~/.local`, so no conflicts
3. **No Sudo Required**: Everything installs to your home directory
4. **Easy Uninstall**: Simply delete files from `~/.local` to revert to system defaults

When you run `thunar` or `xfce4-panel`, your system will automatically find and use the enhanced versions from `~/.local/bin` instead of the system versions in `/usr/bin`.

### Installation Methods

Choose one of the following methods:

#### Method A: User-Only Installation (Recommended - No Sudo)

Install to `~/.local` (used in this repository, works only for your user):

Use `--prefix=$HOME/.local` in all meson/configure commands below.

#### Method B: System-Wide Installation

Install to `/usr/local` (requires sudo, works for all users on the system):

Use `--prefix=/usr/local` in all meson/configure commands below. You'll need `sudo` for the install steps.

### Step-by-Step Build Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/xfce4-github.git
cd xfce4-github
```

### 2. Build and Install Components in Order

**IMPORTANT**: Components must be built in this specific order due to dependencies.

#### a. libxfce4util
```bash
cd xfce4/libxfce4util
meson setup build --prefix=$HOME/.local --buildtype=release
cd build
ninja
ninja install  # no sudo needed for ~/.local
cd ../..
```

#### b. xfconf
```bash
cd xfce4/xfconf
meson setup build --prefix=$HOME/.local --buildtype=release
cd build
ninja
ninja install
cd ../..
```

#### c. libxfce4ui
```bash
cd xfce4/libxfce4ui
meson setup build --prefix=$HOME/.local --buildtype=release
cd build
ninja
ninja install
cd ../..
```

#### d. exo
```bash
cd xfce4/exo
./configure --prefix=$HOME/.local
make -j$(nproc)
make install
cd ..
```

#### e. xfce4-panel (with Recent Files feature)
```bash
cd xfce4/xfce4-panel
meson setup build --prefix=$HOME/.local --buildtype=release
cd build
ninja
ninja install
cd ../..
```

#### f. Thunar (with Archive Support, Breadcrumbs, and Click-to-Rename)
```bash
cd thunar
meson setup build --prefix=$HOME/.local --buildtype=release
cd build
ninja
ninja install
cd ..
```

### 3. Verify Installation

Check that the enhanced versions will be used:

```bash
which thunar          # Should show: ~/.local/bin/thunar (or /usr/local/bin/thunar)
which xfce4-panel     # Should show: ~/.local/bin/xfce4-panel (or /usr/local/bin/xfce4-panel)
```

If you used `~/.local` prefix, verify that `~/.local/bin` is in your PATH and comes before `/usr/bin`:

```bash
echo $PATH | tr ':' '\n' | cat -n | grep -E "\.local|/usr/bin"
```

The `.local/bin` line number should be smaller than the `/usr/bin` line number.

### 4. Update Library Cache (Optional)

If you installed to `/usr/local` (system-wide), update the library cache:

```bash
sudo ldconfig
```

For `~/.local` installations, this is not needed as libraries are found via wrapper scripts.

## Post-Installation

### Restart XFCE Components

To use the new features, you'll need to restart the affected components:

```bash
# Restart XFCE panel
xfce4-panel --restart

# Restart Thunar (close all instances first)
thunar -q
```

Alternatively, log out and log back in to restart the entire XFCE session.

## Usage

### Thunar Archive Support

- **Compress files**: Right-click on files or folders and select "Compress" from the context menu
- **Extract archives**: Right-click on archive files (.zip, .tar.gz, .7z, etc.) and select "Extract Here" or "Extract To..."

### Thunar Breadcrumb Navigation

- Look for the toggle button in the location bar to switch between traditional path entry and breadcrumb navigation
- Click on breadcrumb segments to navigate to parent directories

### Thunar Click-to-Rename

- Click once to select a file
- After a short pause, click again on the filename (not the icon) to enter rename mode
- Type the new name and press Enter

### XFCE Panel Recent Files

- Access recent files from the panel tasklist menu
- Recently opened documents will appear in the list for quick access

## Troubleshooting

### Missing Dependencies

If you encounter build errors about missing dependencies, check the error message for the specific library name and install it using:

```bash
sudo apt install lib<name>-dev
```

### Panel or Thunar Not Starting

If the panel or Thunar fails to start after installation:

1. Check for error messages in the system log:
   ```bash
   journalctl -xe
   ```

2. Try running from the terminal to see error output:
   ```bash
   xfce4-panel
   # or
   thunar
   ```

### Features Not Working

If the custom features aren't working:

1. Verify the installation completed without errors
2. Make sure you've restarted the components (see Post-Installation)
3. Check that you're running the correct version:
   ```bash
   thunar --version
   xfce4-panel --version
   ```

## Uninstallation

To uninstall and revert to the system's default XFCE components:

### Method 1: Using Meson/Make Uninstall

For each component you installed, go to its build directory and run:

```bash
# For meson projects (libxfce4util, xfconf, libxfce4ui, xfce4-panel, thunar):
cd build
ninja uninstall  # add sudo if you used /usr/local

# For autotools projects (exo):
cd xfce4/exo
make uninstall  # add sudo if you used /usr/local
```

### Method 2: Manual Removal

If you installed to `~/.local`:

```bash
# Remove binaries and libraries
rm -f ~/.local/bin/thunar ~/.local/bin/thunar-bin ~/.local/bin/xfce4-panel
rm -rf ~/.local/lib/*/libxfce4*
rm -rf ~/.local/lib/*/libthunar*
rm -rf ~/.local/lib/*/xfce4/
rm -rf ~/.local/share/xfce4/
rm -rf ~/.local/share/thunar/
```

If you installed to `/usr/local`:

```bash
# Remove binaries
sudo rm -f /usr/local/bin/thunar /usr/local/bin/xfce4-panel

# Remove libraries
sudo rm -rf /usr/local/lib/libxfce4*
sudo rm -rf /usr/local/lib/thunarx-*
sudo rm -rf /usr/local/lib/xfce4/

# Update library cache
sudo ldconfig
```

### Restart XFCE

After uninstallation, restart XFCE components to use the system defaults:

```bash
xfce4-panel --restart
thunar -q
```

Or log out and log back in.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project maintains the same licenses as the upstream XFCE components:
- Thunar: GNU General Public License v2.0
- XFCE Panel: GNU General Public License v2.0

See individual component directories for specific license information.

## Credits

Based on the official XFCE desktop environment:
- Thunar: https://gitlab.xfce.org/xfce/thunar
- XFCE Panel: https://gitlab.xfce.org/xfce/xfce4-panel

Custom enhancements developed for improved user experience.
