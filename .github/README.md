# Deskflow Roleswap

**Two-way mouse and keyboard sharing between a Mac and a Windows PC, with clipboard sharing.**
Use the mouse and keyboard of *either* computer on both, without changing any settings.

An unofficial, experimental fork of [Deskflow](https://github.com/deskflow/deskflow) 1.27.0, a software KVM. It isn't affiliated with or supported by the Deskflow project, so please report problems [here](https://github.com/barbo7/deskflow/issues).

**[Türkçe özet](#türkçe)** · **[Download](https://github.com/barbo7/deskflow/releases/tag/roleswap-1)**

## Why

In Deskflow and similar tools, only the *server's* mouse and keyboard can cross to the other computer. To use the other computer's mouse, you have to swap server and client by hand on both machines.

This fork does the swap for you: push a computer's own mouse toward the other screen, and that computer becomes the server.

## Features

| Feature | What it does |
|---|---|
| **Automatic role swap** | Push the client's own mouse against the edge facing the server. The computers swap roles in a few seconds and the cursor moves across |
| **Clipboard sharing** | Copy on one computer, paste on the other, in either role |
| **Arrow hotkeys** | Move to the computer on that side |
| **"In use" indicator** | Shows which computer has the cursor, in the main window and tray |
| **"Switch to" buttons** | Move to the other computer from the main window or tray menu |
| **Escape hotkey** | Brings the cursor back to the server, even if the other computer stops responding |
| **Automatic revert** | If a swap doesn't complete within 45 seconds, both computers go back to their old roles |
| **Game protection (Windows)** | No edge switching or swaps while a game hides or confines the cursor |
| **Elevation prompt handling (Windows)** | The cursor returns to the server when a UAC prompt or the lock screen appears |

## Hotkeys

| | Windows | Mac |
|---|---|---|
| Move left or right | `Ctrl` + `Alt` + `Win` + `←` / `→` | `Control` + `Option` + `Command` + `←` / `→` |
| Escape to the server | `Ctrl` + `Alt` + `Win` + `M` | `Control` + `Option` + `Command` + `M` |

On the server's keyboard, the switch is instant. On the client's keyboard, pressing the arrow toward the server swaps roles first.

## Install

Install the [`roleswap-1` release](https://github.com/barbo7/deskflow/releases/tag/roleswap-1) on **both** computers, and check the downloads against `sums.txt`. Builds are unsigned.

**Windows** (`.msi`)

1. Install the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) if you don't have it.
2. Run the installer. An "unknown publisher" warning is expected.
3. In Deskflow's settings, set the process mode to **Desktop**. Role swaps don't work in Service mode.

**Mac** (`.dmg`, macOS 13 or later on Apple Silicon)

1. Copy the app to `/Applications/Deskflow Roleswap.app`.
2. Prepare it. The new identifier keeps its permissions separate from an official Deskflow install:

   ```bash
   APP="/Applications/Deskflow Roleswap.app"
   /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier org.deskflow.deskflow.roleswap" "$APP/Contents/Info.plist"
   xattr -dr com.apple.quarantine "$APP"
   codesign --force --deep -s - "$APP"
   ```

3. Add the app under System Settings → Privacy & Security → Accessibility.

**Then on both computers**

1. Make one computer the server. In *Configure Server*, place the other computer on the correct side, using its exact name.
2. Make the other a client and enter the server's IP address.
3. Accept the fingerprint prompts on the first connection.

## How it works

The client watches its physical mouse, ignoring input Deskflow injects. When the mouse is pushed toward the server, the client asks to swap. The server sends its layout and agrees, the client restarts as the server, and the old server reconnects to it as a client. This uses protocol 1.9; older Deskflow versions negotiate down to 1.8 and can't swap roles.

See the [full guide (in Turkish)](https://github.com/barbo7/deskflow/blob/feat/active-computer-indicator/docs/AktifBilgisayar-ve-PatronDegisimi.md) for the architecture and a ready-to-paste setup prompt for Claude Code.

## Status and limits

- **Confirmed working:** role swaps in both directions between one Mac built from this source and one Windows x64 PC in Desktop mode.
- **Not tested:** the macOS `.dmg` files, Windows on ARM, and setups with more than two computers.
- **Windows Service mode:** role swaps don't work.
- **Desktop mode on Windows:** the other computer's mouse can't control windows running as administrator.
- **Linux:** the indicator and buttons work, but pushing the mouse doesn't trigger a swap.

## Türkçe

Bir Mac ile bir Windows bilgisayarı **iki yönlü** kullanmak için Deskflow'un gayriresmi sürümü. Hangi bilgisayarın faresini ekranın kenarına itersen o bilgisayar patron olur; böylece iki bilgisayarın faresi ve klavyesi de iki ekranda çalışır. Kopyala-yapıştır da çalışmaya devam eder.

[Kurulum dosyaları](https://github.com/barbo7/deskflow/releases/tag/roleswap-1) · [Nasıl çalışır ve nasıl kurulur](https://github.com/barbo7/deskflow/blob/feat/active-computer-indicator/docs/AktifBilgisayar-ve-PatronDegisimi.md)

## License

[GPL-2.0](https://github.com/barbo7/deskflow/blob/feat/active-computer-indicator/LICENSE) with an [OpenSSL exception](https://github.com/barbo7/deskflow/blob/feat/active-computer-indicator/LICENSES/LicenseRef-OpenSSL-Exception.txt). Based on [Deskflow](https://github.com/deskflow/deskflow) and its contributors' work.
