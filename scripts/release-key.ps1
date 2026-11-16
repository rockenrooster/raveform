# The release key (docs/RELEASE_PIPELINE.md; the owner's choice of 2026-11-15: a new key). Creates it outside the repository,
# gives it to the GitHub release workflow as its four secrets, and starts a release build. The owner runs it at this PC:
# keytool asks for a new password (twice) and this script once more; nothing writes the password down or passes it on a
# command line. Back the keystore and its password up: a build signed with another key cannot update an installed copy.
#   scripts\release-key.ps1 [-Keystore <path>] [-SecretsOnly] [-NoBuild]
param(
    [string]$Keystore = (Join-Path $env:USERPROFILE '.raveform\raveform-release.keystore'),
    [string]$Alias = 'raveform',
    [string]$Repo = 'rockenrooster/raveform',
    [switch]$SecretsOnly,  # the keystore exists: only set the secrets again
    [switch]$NoBuild       # set the secrets without starting a build
)
. "$PSScriptRoot\env.ps1"
$keytool = Join-Path $RF_JDK 'bin\keytool.exe'
$gh = (Get-Command gh -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source

function Set-GhSecret([string]$Name, [string]$Value) {
    # The value goes to gh's standard input as it is (no newline added, never on a command line).
    $psi = [Diagnostics.ProcessStartInfo]::new($gh)
    foreach ($a in @('secret', 'set', $Name, '-R', $Repo)) { $psi.ArgumentList.Add($a) }
    $psi.RedirectStandardInput = $true
    $psi.UseShellExecute = $false
    $p = [Diagnostics.Process]::Start($psi)
    $p.StandardInput.Write($Value)
    $p.StandardInput.Close()
    $p.WaitForExit()
    if ($p.ExitCode -ne 0) { throw "gh secret set $Name failed ($($p.ExitCode))" }
}

if (-not $SecretsOnly) {
    if (Test-Path $Keystore) { throw "$Keystore exists and may be the key in use: keep it (-SecretsOnly uploads it again)" }
    New-Item -ItemType Directory -Force (Split-Path $Keystore) | Out-Null
    Write-Host "[key] creating $Keystore (keytool asks for a new password, twice)"
    & $keytool -genkeypair -keystore $Keystore -storetype PKCS12 -alias $Alias -keyalg RSA -keysize 4096 -validity 10000 -dname 'CN=RAVEFORM'
    if ($LASTEXITCODE -ne 0) { throw 'keytool failed' }
}
if (-not (Test-Path $Keystore)) { throw "no keystore at $Keystore" }

$secure = Read-Host -AsSecureString "[key] the keystore's password, once more, for GitHub"
$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
try {
    $env:RF_KEY_CHECK = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    $cert = & $keytool -list -v -keystore $Keystore -storepass:env RF_KEY_CHECK -alias $Alias 2>&1
    if ($LASTEXITCODE -ne 0) { throw "that password does not open $Keystore (or it has no key '$Alias')" }
    Set-GhSecret 'RF_KEYSTORE_BASE64' ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Keystore)))
    Set-GhSecret 'RF_KEYSTORE_PASSWORD' $env:RF_KEY_CHECK
    Set-GhSecret 'RF_KEY_PASSWORD' $env:RF_KEY_CHECK  # (a PKCS12 keystore's key has the keystore's password)
    Set-GhSecret 'RF_KEY_ALIAS' $Alias
} finally {
    Remove-Item Env:RF_KEY_CHECK -ErrorAction SilentlyContinue
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
}
$sha = ($cert | Select-String -Pattern 'SHA256:\s*(\S+)' | Select-Object -First 1).Matches.Groups[1].Value
$sha = $sha.Replace(':', '').ToLowerInvariant()  # (as the release notes print it)
Write-Host "[key] the four secrets are set on $Repo; the certificate's SHA-256: $sha"
Write-Host "[key] back up $Keystore and its password (not in the repository): without them no later build can update an installed copy"
if (-not $NoBuild) {
    gh workflow run release.yml -R $Repo --ref main
    if ($LASTEXITCODE -ne 0) { throw 'gh workflow run failed' }
    Write-Host "[key] a release build has started: gh run watch -R $Repo"
}
