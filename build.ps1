# Setup environment
$vcvarsPath = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
$tempFile = 'C:\temp\vcvars.txt'

New-Item -ItemType Directory -Path 'C:\temp' -Force | Out-Null

# Run vcvarsall and capture environment
cmd /c "`"$vcvarsPath`" x64 && set > `"$tempFile`""

# Parse and set environment variables
Get-Content $tempFile -Encoding UTF8 | ForEach-Object {
    $parts = $_.Split('=', 2)
    if ($parts.Count -eq 2) {
        [Environment]::SetEnvironmentVariable($parts[0], $parts[1], "Process")
    }
}

# Override TEMP/TMP to avoid Chinese path issues
[Environment]::SetEnvironmentVariable('TEMP', 'C:\temp', "Process")
[Environment]::SetEnvironmentVariable('TMP', 'C:\temp', "Process")

# Go to Qt directory (where project files are)
Set-Location 'D:\Qt'

# Run nmake
& nmake clean 2>&1
& nmake 2>&1