param(
    [Parameter(Position = 0)]
    [string]$ClangFormatArg
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$input = Join-Path $repoRoot 'include/gaia.h'
$output = Join-Path $repoRoot 'single_include/gaia.h'
$includeDir = Join-Path $repoRoot 'include'

function Resolve-ClangFormat {
    param(
        [string]$Arg
    )

    if ($Arg) {
        if (Test-Path -LiteralPath $Arg -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Arg).Path
        }

        $cmd = Get-Command $Arg -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) {
            if ($cmd.Path) {
                return $cmd.Path
            }
            return $cmd.Source
        }

        throw "ERROR: '$Arg' was not found as a file and is not on PATH."
    }

    foreach ($candidate in @('clang-format', 'clang-format-19', 'clang-format-18', 'clang-format-17')) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cmd) {
            if ($cmd.Path) {
                return $cmd.Path
            }
            return $cmd.Source
        }
    }

    return $null
}

function Resolve-IncludePath {
    param(
        [string]$CurrentFileDir,
        [string]$IncludePath
    )

    $candidate = Join-Path $CurrentFileDir $IncludePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $resolved = (Resolve-Path -LiteralPath $candidate).Path
        if ($resolved.StartsWith($includeDir, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $resolved
        }
    }

    $candidate = Join-Path $includeDir $IncludePath
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    $baseName = Split-Path -Leaf $IncludePath
    $match = Get-ChildItem -Path $includeDir -Recurse -File -Filter $baseName | Select-Object -First 1
    if ($match) {
        return $match.FullName
    }

    return $null
}

$clangFormat = Resolve-ClangFormat $ClangFormatArg

Write-Host "Input        : $input"
Write-Host "Output       : $output"
if ($clangFormat) {
    Write-Host "clang-format : $clangFormat"
}
else {
    Write-Host 'clang-format : not found - formatting will be skipped'
}

[System.IO.Directory]::CreateDirectory((Split-Path -Parent $output)) | Out-Null

$visited = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$builder = [System.Text.StringBuilder]::new()

[void]$builder.AppendLine('// Amalgamated single-header build of Gaia-ECS')
[void]$builder.AppendLine('// The file is generated. Do not edit it.')
[void]$builder.AppendLine('#pragma once')
[void]$builder.AppendLine('')

$includeRegex = [regex]'^\s*#\s*include\s*(["<])([^">]+)[">]'
$pragmaOnceRegex = [regex]'^\s*#\s*pragma\s+once\s*$'

function Append-File {
    param(
        [string]$FilePath
    )

    $resolvedFile = (Resolve-Path -LiteralPath $FilePath).Path
    if (-not $visited.Add($resolvedFile)) {
        return
    }

    $fileDir = Split-Path -Parent $resolvedFile

    foreach ($line in [System.IO.File]::ReadLines($resolvedFile)) {
        if ($pragmaOnceRegex.IsMatch($line)) {
            continue
        }

        $match = $includeRegex.Match($line)
        if (-not $match.Success) {
            [void]$builder.AppendLine($line)
            continue
        }

        $delimiter = $match.Groups[1].Value
        if ($delimiter -eq '<') {
            [void]$builder.AppendLine($line)
            continue
        }

        $includePath = $match.Groups[2].Value.Trim()
        if (-not $includePath) {
            [void]$builder.AppendLine($line)
            continue
        }

        $resolvedInclude = Resolve-IncludePath -CurrentFileDir $fileDir -IncludePath $includePath
        if ($resolvedInclude) {
            Append-File $resolvedInclude
        }
        else {
            [void]$builder.AppendLine($line)
        }
    }
}

Append-File $input

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($output, $builder.ToString(), $utf8NoBom)

if ($clangFormat) {
    Write-Host "Formatting   : $output"
    & $clangFormat -i --style=file $output
    if ($LASTEXITCODE -ne 0) {
        throw 'ERROR: clang-format failed.'
    }
}
else {
    Write-Host 'Formatting   : skipped'
}

$lineCount = ([System.IO.File]::ReadLines($output) | Measure-Object -Line).Lines
Write-Host "Done: $output ($lineCount lines)"
