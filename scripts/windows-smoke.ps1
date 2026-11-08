[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallerPath,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$installDirectory = "C:\StrataLauncherSmoke-$PID"
$shortcutPath = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\StrataLauncher.lnk'
$failureCount = 0
$installed = $false

function Invoke-Check {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    try {
        & $Action
        Write-Host "PASS: $Name"
    }
    catch {
        $script:failureCount += 1
        Write-Host "FAIL: $Name - $($_.Exception.Message)"
    }
}

$resolvedInstaller = $null
Invoke-Check 'installer and smoke-test paths are clear' {
    if (-not (Test-Path -LiteralPath $InstallerPath -PathType Leaf)) {
        throw "Installer not found: $InstallerPath"
    }
    if (Test-Path -LiteralPath $installDirectory) {
        throw "Install directory already exists: $installDirectory"
    }
    if (Test-Path -LiteralPath $shortcutPath) {
        throw "Start Menu shortcut already exists: $shortcutPath"
    }
    $script:resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath).Path
}

if ($null -ne $resolvedInstaller) {
    Invoke-Check 'silent installation' {
        $process = Start-Process -FilePath $resolvedInstaller `
            -ArgumentList @('/S', "/D=$installDirectory") -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Installer exited with code $($process.ExitCode)"
        }
        $script:installed = $true
    }
}

if ($installed) {
    Invoke-Check 'installed files and runtime DLLs' {
        $executable = Join-Path $installDirectory 'bin\stratalauncher.exe'
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "Launcher executable missing: $executable"
        }
        if (-not (Get-ChildItem -LiteralPath (Join-Path $installDirectory 'bin') `
                -Filter '*.dll' -File)) {
            throw 'No runtime DLLs were installed beside the launcher'
        }
        foreach ($requiredPath in @('LICENSE', 'LICENSES', 'THIRD_PARTY_NOTICES')) {
            if (-not (Test-Path -LiteralPath (Join-Path $installDirectory $requiredPath))) {
                throw "Required license file or directory missing: $requiredPath"
            }
        }
    }

    Invoke-Check 'Start Menu shortcut' {
        if (-not (Test-Path -LiteralPath $shortcutPath -PathType Leaf)) {
            throw "Shortcut missing: $shortcutPath"
        }
    }

    Invoke-Check 'version output' {
        $executable = Join-Path $installDirectory 'bin\stratalauncher.exe'
        $output = (& $executable --version 2>&1 | Out-String).Trim()
        if ($LASTEXITCODE -ne 0) {
            throw "Launcher exited with code $LASTEXITCODE"
        }
        if ($output -notmatch [regex]::Escape("StrataLauncher $ExpectedVersion")) {
            throw "Unexpected version output: $output"
        }
    }

    $uninstaller = Join-Path $installDirectory 'Uninstall.exe'
    if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        $uninstaller = Join-Path $installDirectory 'uninstall.exe'
    }
    Invoke-Check 'silent uninstall and cleanup' {
        if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
            throw 'Uninstaller executable missing'
        }
        $process = Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Uninstaller exited with code $($process.ExitCode)"
        }
        if (Test-Path -LiteralPath $installDirectory) {
            throw "Install directory remains after uninstall: $installDirectory"
        }
        if (Test-Path -LiteralPath $shortcutPath) {
            throw "Start Menu shortcut remains after uninstall: $shortcutPath"
        }
    }
}
else {
    Write-Host 'SKIP: installation-dependent checks'
}

if ($failureCount -gt 0) {
    Write-Host "Windows smoke test failed: $failureCount check(s)"
    exit 1
}

Write-Host 'Windows smoke test passed'
