# Windows 11 Manual Verification

Use the installer, its `.sha256` file, and `windows-smoke.ps1` from the supplied VM folder.

1. Verify the installer SHA-256 against the adjacent `.sha256` file.
2. Run the installer interactively. Confirm the GPLv3 license page, install-directory selection, and Start Menu shortcut.
3. Expect an unsigned-publisher SmartScreen warning; note it, but do not treat it as a failure.
4. Launch from the shortcut. Confirm the main window renders, then test:
   - Offline sign-in and the account listing.
   - Minecraft version loading and instance creation/installation.
   - Launching an instance through to the Minecraft main menu.
   - Server creation, EULA prompt, start, console, stop, and deletion.
5. Check for missing-DLL errors, console flashes, incorrect app name, or a wrong icon.
6. Uninstall through **Settings > Apps**. Confirm the app directory and Start Menu shortcut are removed.
7. Record the Windows edition/build, VM type, each checklist result, and any unexpected behavior.

Automated smoke test, from PowerShell:

```powershell
.\windows-smoke.ps1 -InstallerPath .\stratalauncher-<version>-windows-x86_64.exe -ExpectedVersion <version>
```

The automated test uses a separate `C:\StrataLauncherSmoke-*` directory and does not replace an existing install or Start Menu shortcut.
