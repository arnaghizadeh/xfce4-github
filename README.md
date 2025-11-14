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

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/xfce4-github.git
cd xfce4-github
```

### 2. Build and Install Thunar

```bash
cd thunar
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install
```

### 3. Build and Install XFCE Components

The xfce4 directory contains multiple components. You'll need to build them in the correct order:

#### a. libxfce4util
```bash
cd ../xfce4/libxfce4util
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install
```

#### b. xfconf
```bash
cd ../../xfconf
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install
```

#### c. libxfce4ui
```bash
cd ../../libxfce4ui
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install
```

#### d. exo
```bash
cd ../../exo
./configure --prefix=/usr
make
sudo make install
```

#### e. xfce4-panel
```bash
cd ../xfce4-panel
meson setup build --prefix=/usr
cd build
ninja
sudo ninja install
```

### 4. Update Library Cache

After installation, update the library cache:

```bash
sudo ldconfig
```

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

```bash
# For each component you installed, go to its build directory and run:
cd build
sudo ninja uninstall

# Then reinstall the distribution's XFCE packages:
sudo apt install --reinstall thunar xfce4-panel
```

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
