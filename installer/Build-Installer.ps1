param(
    [Parameter(Mandatory = $true)]
    [string] $ModZip,
    [string] $Output = (Join-Path $PSScriptRoot 'UMVC3TrainingToolsInt-Setup.exe'),
    [string] $LoaderZip,
    [string] $LoaderLicense
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$modArchivePath = (Resolve-Path -LiteralPath $ModZip).Path
$outputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$work = Join-Path ([IO.Path]::GetTempPath()) ('umvc3-installer-' + [Guid]::NewGuid().ToString('N'))
$licensePrefix = 'Licenses/UMVC3TrainingToolsInt/'
$loaderVersion = 'v10.0.4'

function Add-ArchiveResource($archive, [string] $entryName, [string] $resourceName) {
    $entry = $archive.GetEntry($entryName)
    if ($null -eq $entry -or $entry.Length -eq 0) {
        throw "Falta $entryName en el paquete de instalación."
    }
    $file = Join-Path $work ([Guid]::NewGuid().ToString('N'))
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $file)
    "/resource:$file,$resourceName"
}

try {
    [void] [IO.Directory]::CreateDirectory($work)
    if (!$LoaderZip) {
        $LoaderZip = Join-Path $work 'loader.zip'
        $url = "https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/$loaderVersion/Ultimate-ASI-Loader-NoPDB_x64.zip"
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $LoaderZip
    }
    if (!$LoaderLicense) {
        $LoaderLicense = Join-Path $work 'license.txt'
        $url = "https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/$loaderVersion/license"
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $LoaderLicense
    }
    $LoaderZip = (Resolve-Path -LiteralPath $LoaderZip).Path
    $LoaderLicense = (Resolve-Path -LiteralPath $LoaderLicense).Path
    if ((Get-FileHash -LiteralPath $LoaderZip -Algorithm SHA256).Hash -ne 'aed4a9ed3fe76cbd5ae7e62a25eccafdd05e0b5b585ff1995e51455ff387a4fb' -or
        (Get-FileHash -LiteralPath $LoaderLicense -Algorithm SHA256).Hash -ne 'ceca73c504e39084b0b80d2a437aea9988313d1915418d3c629c3e4a8313f5a1') {
        throw "El cargador ASI y su licencia deben coincidir con la versión oficial $loaderVersion."
    }

    $archive = [IO.Compression.ZipFile]::OpenRead($modArchivePath)
    try {
        $resources = @(
            Add-ArchiveResource $archive 'UMVC3TrainingToolsInt.asi' 'UMVC3TrainingToolsInt.asi'
            Add-ArchiveResource $archive 'Resources/3TTAppText.csv' 'Resources/3TTAppText.csv'
            Add-ArchiveResource $archive 'Resources/FontToUse.ttf' 'Resources/FontToUse.ttf'
            Add-ArchiveResource $archive 'Resources/Apache License.txt' 'Resources/Apache License.txt'
            Add-ArchiveResource $archive 'LICENSE.txt' ($licensePrefix + 'UMVC3TrainingToolsInt.txt')
            foreach ($license in @('ImGui.txt', 'Utils.txt', 'MinHook.txt', 'HDE.txt')) {
                Add-ArchiveResource $archive ("Licenses/" + $license) ($licensePrefix + $license)
            }
        )
    }
    finally { $archive.Dispose() }

    $loader = [IO.Compression.ZipFile]::OpenRead($LoaderZip)
    try { $resources += Add-ArchiveResource $loader 'dinput8.dll' 'dinput8.dll' }
    finally { $loader.Dispose() }
    $resources += ('/resource:{0},{1}Ultimate ASI Loader.txt' -f $LoaderLicense, $licensePrefix)

    [void] [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath))
    $sources = @((Join-Path $PSScriptRoot 'Installer.cs'), (Join-Path $PSScriptRoot 'UpdateInstaller.cs'))
    & $compiler /nologo /target:winexe /platform:x64 /optimize+ /codepage:65001 "/out:$outputPath" @resources @sources
    if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del instalador." }
    Get-Item -LiteralPath $outputPath
}
finally {
    if ((Test-Path -LiteralPath $work) -and
        (Split-Path -Parent (Resolve-Path -LiteralPath $work).Path) -eq [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') -and
        (Split-Path -Leaf $work) -match '^umvc3-installer-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $work -Recurse
    }
}
