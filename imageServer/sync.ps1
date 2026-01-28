param (
    [string]$Increment = "patch"  # Default to patch
)
$File = Join-Path -Path $PSScriptRoot -ChildPath "keys.py"  # Change this to your actual file path
$localPath = Join-Path -Path $PSScriptRoot -ChildPath "*"
$SyncHost = "joshgraff@panel.lan"
$syncDir = "/home/joshgraff/dev/imageServer/"

# Read the file and extract the version number
$Content = Get-Content $File
$VersionLine = $Content | Where-Object { $_ -match '^version = "(\d+\.\d+\.\d+)"' }

if ($Matches.Count -eq 0) {
    Write-Host "Error: Could not find version number in file."
    exit 1
}

# Extract and split version number
$Version = $Matches[1]
$Parts = $Version -split '\.'
$Major = [int]$Parts[0]
$Minor = [int]$Parts[1]
$Patch = [int]$Parts[2]

# Increment the specified part
switch ($Increment) {
    "major" { $Major++; $Minor = 0; $Patch = 0 }
    "minor" { $Minor++; $Patch = 0 }
    "patch" { $Patch++ }
    default { Write-Host "Invalid option. Use: major, minor, or patch"; exit 1 }
}

# Construct the new version string
$NewVersion = "$Major.$Minor.$Patch"

# Update the file content
$UpdatedContent = $Content -replace "VERSION = `"$Version`"", "VERSION = `"$NewVersion`""
$UpdatedContent | Set-Content $File

Write-Host "Version updated to $NewVersion"

scp -r ${localPath} ${SyncHost}:${syncDir}