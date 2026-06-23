$vcvarsPath = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
$tempFile = 'C:\temp\vcvars.txt'

New-Item -ItemType Directory -Path 'C:\temp' -Force | Out-Null

cmd /c "`"$vcvarsPath`" x64 && set > `"$tempFile`""

Get-Content $tempFile -Encoding UTF8 | ForEach-Object {
    $parts = $_.Split('=', 2)
    if ($parts.Count -eq 2) {
        [Environment]::SetEnvironmentVariable($parts[0], $parts[1], "Process")
    }
}

[Environment]::SetEnvironmentVariable('TEMP', 'C:\temp', "Process")
[Environment]::SetEnvironmentVariable('TMP', 'C:\temp', "Process")

Set-Location 'D:\QtCADPlatform\build'

& msbuild HydraulicCADPlatform.vcxproj /p:Configuration=Debug /v:minimal 2>&1
