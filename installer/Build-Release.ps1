param(
    [string] $Output,
    [string] $Tag,
    [string] $PlatformToolset,
    [string] $LoaderZip,
    [string] $LoaderLicense
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $root 'UMVC3TrainingToolsInt'
$versionHeader = Get-Content -Raw -LiteralPath (Join-Path $project 'code/ReleaseUpdate.h')
$version = [regex]::Match($versionHeader, 'VersionTag\[\]\s*=\s*"(v\d+\.\d+\.\d+)"').Groups[1].Value
if (!$version) { throw 'VersionTag debe tener el formato vX.Y.Z.' }
if ($Tag -and $Tag -cne $version) { throw "La etiqueta $Tag no coincide con VersionTag ($version)." }
if (!(Test-Path -LiteralPath (Join-Path $root "changelog/$version.md") -PathType Leaf)) {
    throw "Falta el changelog de $version."
}
if (!$Output) { $Output = Join-Path $PSScriptRoot "UMVC3TrainingToolsInt-Setup-$version.exe" }
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe' |
    Select-Object -First 1
if (!$msbuild) { throw 'No se encontró MSBuild de Visual Studio.' }

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
$work = Join-Path $tempRoot ('umvc3-release-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

try {
    $bin = Join-Path $work 'bin'
    $obj = Join-Path $work 'obj'
    $buildArgs = @(
        (Join-Path $root 'UMVC3TrainingToolsInt.sln'), '/m', '/nologo', '/v:minimal',
        '/p:Configuration=Release', '/p:Platform=x64', "/p:OutDir=$bin\", "/p:IntDir=$obj\"
    )
    if ($PlatformToolset) { $buildArgs += "/p:PlatformToolset=$PlatformToolset" }
    & $msbuild @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación Release x64.' }

    $asi = Join-Path $bin 'UMVC3TrainingToolsInt.asi'
    $files = @{
        'UMVC3TrainingToolsInt.asi'      = $asi
        'Resources/3TTAppText.csv'     = Join-Path $project 'Resources/3TTAppText.csv'
        'Resources/FontToUse.ttf'      = Join-Path $project 'Resources/FontToUse.ttf'
        'Resources/Apache License.txt' = Join-Path $project 'Resources/Apache License.txt'
        'LICENSE.txt'                  = Join-Path $root 'LICENSE'
        'Licenses/ImGui.txt'           = Join-Path $project 'gui/imgui/LICENSE.txt'
        'Licenses/Utils.txt'           = Join-Path $project 'utils/LICENSE'
        'Licenses/MinHook.txt'         = Join-Path $PSScriptRoot 'licenses/MinHook.txt'
        'Licenses/HDE.txt'             = Join-Path $PSScriptRoot 'licenses/HDE.txt'
    }
    $modZip = Join-Path $work 'mod.zip'
    $zip = [IO.Compression.ZipFile]::Open($modZip, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($entry in ($files.Keys | Sort-Object)) {
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $files[$entry], $entry)
        }
    }
    finally { $zip.Dispose() }

    $stagedSetup = Join-Path $work 'UMVC3TrainingToolsInt-Setup.exe'
    $installerArgs = @{ ModZip = $modZip; Output = $stagedSetup }
    if ($LoaderZip) { $installerArgs.LoaderZip = $LoaderZip }
    if ($LoaderLicense) { $installerArgs.LoaderLicense = $LoaderLicense }
    & (Join-Path $PSScriptRoot 'Build-Installer.ps1') @installerArgs | Out-Null

    $setupBytes = [IO.File]::ReadAllBytes($stagedSetup)
    $machine = [BitConverter]::ToUInt16($setupBytes, [BitConverter]::ToInt32($setupBytes, 0x3c) + 4)
    if ($machine -ne 0x8664) { throw 'El instalador generado no es x64.' }
    $assembly = [Reflection.Assembly]::Load($setupBytes)
    $embeddedAsi = $assembly.GetManifestResourceStream('UMVC3TrainingToolsInt.asi')
    if (!$embeddedAsi) { throw 'El instalador no incluye el archivo ASI.' }
    try { $embeddedHash = (Get-FileHash -InputStream $embeddedAsi -Algorithm SHA256).Hash }
    finally { $embeddedAsi.Dispose() }
    if ($embeddedHash -ne (Get-FileHash -LiteralPath $asi -Algorithm SHA256).Hash) {
        throw 'El SHA-256 del ASI embebido no coincide con el de la compilación.'
    }

    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath))
    Copy-Item -LiteralPath $stagedSetup -Destination $outputPath -Force
    if ((Get-FileHash -LiteralPath $outputPath).Hash -ne (Get-FileHash -LiteralPath $stagedSetup).Hash) {
        throw 'La copia del instalador no coincide con el ejecutable verificado.'
    }
    Write-Host "Instalador x64 $version verificado. SHA-256 del ASI: $embeddedHash"
    Get-Item -LiteralPath $outputPath
}
finally {
    $resolvedWork = (Resolve-Path -LiteralPath $work).Path
    if ((Split-Path -Parent $resolvedWork) -eq $tempRoot -and (Split-Path -Leaf $resolvedWork) -match '^umvc3-release-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolvedWork -Recurse
    }
}
