# XFCE4 Fork

This repository contains forks of the core XFCE4 desktop environment components with all source code included.

## Components

- **xfce4-panel** - The XFCE panel
- **xfwm4** - The XFCE window manager
- **xfce4-settings** - Settings manager for XFCE
- **xfce4-session** - Session manager
- **xfdesktop** - Desktop manager
- **xfconf** - Configuration storage system
- **libxfce4ui** - UI library for XFCE
- **libxfce4util** - Utility library for XFCE
- **exo** - Extension library for XFCE
- **xfce4-appfinder** - Application finder
- **xfce4-power-manager** - Power management daemon

## Thunar

The Thunar file manager is maintained in a separate repository: [arnaghizadeh/thunar](https://github.com/arnaghizadeh/thunar)

## Building

Each component has its own build instructions. Generally, XFCE components use:

```bash
./autogen.sh
make
sudo make install
```

Refer to each component's documentation for specific build requirements.
