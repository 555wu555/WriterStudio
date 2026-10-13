param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$QtBin
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stage = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist\app'))
if (-not $stage.StartsWith($repositoryRoot + [System.IO.Path]::DirectorySeparatorChar,
                          [System.StringComparison]::OrdinalIgnoreCase)) {
    throw '暂存目录不在当前项目中。'
}

$build = (Resolve-Path -LiteralPath $BuildDir).Path
$cache = Join-Path $build 'CMakeCache.txt'
$exe = Join-Path $build 'write.exe'
$deploy = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path -LiteralPath $cache -PathType Leaf) -or
    -not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    throw '构建目录缺少 CMakeCache.txt 或 write.exe。'
}
if (-not (Test-Path -LiteralPath $deploy -PathType Leaf)) {
    throw '未找到 windeployqt.exe，请提供 Qt 的 bin 目录。'
}
$cacheLines = Get-Content -LiteralPath $cache
if ($cacheLines -notcontains 'CMAKE_BUILD_TYPE:STRING=Release') {
    throw '只允许打包 Release 构建。'
}
$compilerLine = $cacheLines | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:(?:FILEPATH|STRING)=' } | Select-Object -First 1
if (-not $compilerLine) { throw '无法从 CMakeCache.txt 找到 C++ 编译器。' }
$compilerBin = Split-Path -Parent ($compilerLine -replace '^CMAKE_CXX_COMPILER:(?:FILEPATH|STRING)=', '')
$objdump = Join-Path $compilerBin 'objdump.exe'

if (Test-Path -LiteralPath $stage) {
    $stageItem = Get-Item -LiteralPath $stage -Force
    if (($stageItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw '暂存目录是链接，拒绝递归清理。'
    }
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $stage 'write.exe')

$env:PATH = "$QtBin;$compilerBin;$env:PATH"
& $deploy --release --dir $stage (Join-Path $stage 'write.exe')
if ($LASTEXITCODE -ne 0) {
    Write-Warning 'windeployqt 无法运行，改为扫描本项目的 Qt/MinGW DLL 依赖。'
    if (-not (Test-Path -LiteralPath $objdump -PathType Leaf)) {
        throw '找不到 objdump.exe，无法执行备用部署。'
    }

    foreach ($plugin in @(
        'platforms\qwindows.dll',
        'tls\qschannelbackend.dll',
        'imageformats\qjpeg.dll'
    )) {
        $source = Join-Path (Join-Path $QtBin '..\plugins') $plugin
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Qt 插件不存在：$source"
        }
        $target = Join-Path $stage $plugin
        New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $target -Force
    }

    $queue = [System.Collections.Generic.Queue[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
        $queue.Enqueue($_.FullName)
    }
    while ($queue.Count -gt 0) {
        $binary = $queue.Dequeue()
        $dependencies = & $objdump -p $binary
        if ($LASTEXITCODE -ne 0) { throw "无法扫描 DLL 依赖：$binary" }
        foreach ($line in $dependencies) {
            if ($line -notmatch '^\s*DLL Name:\s*(\S+)') { continue }
            $name = $Matches[1]
            if (-not $seen.Add($name)) { continue }
            if ($name -match '^(api-ms-win-|ext-ms-)') { continue }
            $target = Join-Path $stage $name
            if (Test-Path -LiteralPath $target -PathType Leaf) { continue }

            $searchDirs = if ($name -match '^(libgcc_|libstdc\+\+|libwinpthread)') {
                @($compilerBin, $QtBin)
            } else {
                @($QtBin, $compilerBin)
            }
            $source = $null
            foreach ($directory in $searchDirs) {
                $candidate = Join-Path $directory $name
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $source = $candidate
                    break
                }
            }
            if ($source) {
                Copy-Item -LiteralPath $source -Destination $target -Force
                $queue.Enqueue($target)
            } elseif (-not (Test-Path -LiteralPath (Join-Path $env:WINDIR "System32\$name") -PathType Leaf)) {
                throw "依赖 DLL 未找到：$name（来自 $binary）"
            }
        }
    }
}

$required = @(
    (Join-Path $stage 'write.exe'),
    (Join-Path $stage 'Qt6Core.dll'),
    (Join-Path $stage 'Qt6Gui.dll'),
    (Join-Path $stage 'Qt6Widgets.dll'),
    (Join-Path $stage 'Qt6Network.dll')
)
foreach ($file in $required) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "部署目录缺少：$file"
    }
}
foreach ($plugin in @('qwindows.dll', 'qschannelbackend.dll')) {
    if (-not (Get-ChildItem -LiteralPath $stage -Recurse -File -Filter $plugin |
               Select-Object -First 1)) {
        throw "部署目录缺少插件：$plugin"
    }
}
$pluginRoot = if (Test-Path -LiteralPath (Join-Path $stage 'platforms\qwindows.dll')) {
    '.'
} elseif (Test-Path -LiteralPath (Join-Path $stage 'plugins\platforms\qwindows.dll')) {
    'plugins'
} else {
    throw '平台插件不在可识别的部署位置。'
}
[System.IO.File]::WriteAllText(
    (Join-Path $stage 'qt.conf'),
    "[Paths]`r`nPrefix=.`r`nPlugins=$pluginRoot`r`n",
    [System.Text.Encoding]::ASCII)
Write-Host "部署文件已准备：$stage"
Write-Host '请检查此目录只有程序及其运行依赖，再在 Inno Setup 中将它作为唯一的程序文件来源。'
