# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Benchmark for eden-cli on a Switch 2 title.
# Runs eden-cli N times for a fixed time, kills it, keeps each log and prints per-run and overall
# numbers from the GPU profiler (EDEN_NX2_GPU_PROFILE, one line per thread every 10 s) and the
# nx2:vi2 present log (one line per 600 presents -> mean frame time per 600-frame window).
#
#   .\bench.ps1 -Game "D:\...\exefs\main" [-Runs 3] [-Seconds 120] [-Warmup 40] [-Tag base]
#               [-KeyScript "50:4,50.5:-4"] [-Env @{EDEN_NX2_SPLIT='0'}]
#   .\bench.ps1 -ParseOnly C:\dev\bench\base-*.log
# -WarmupRun adds a first run that is not counted (shader/pipeline caches). -Config @{key=value}
# runs with a copy of user\config\sdl2-config.ini that has those settings ([key\default=false, key=value]);
# -Preset row128 is NX2_NOTES row 128 (gpu_accuracy Low, astc_recompression Bc3, reactive flushing off,
# async shaders on).
param(
    [string]$Game,
    [int]$Runs = 3,
    [switch]$WarmupRun,
    [hashtable]$Config = @{},
    [string]$Preset = "",
    [int]$Seconds = 120,
    [double]$Warmup = 40,
    [string]$Tag = "base",
    [string]$KeyScript = "",
    [hashtable]$Env = @{},
    [string]$Exe = "C:\dev\eden\build\bin\eden-cli.exe",
    [string]$OutDir = "C:\dev\bench",
    [string[]]$ParseOnly
)
$ErrorActionPreference = "Stop"
$inv = [Globalization.CultureInfo]::InvariantCulture

function Parse-Log([string]$path, [double]$warmup) {
    $presents = @()   # (seconds, present number)
    $zones = @{}      # "thread|zone" -> list of (ms, calls)
    foreach ($line in [IO.File]::ReadLines($path)) {
        if ($line -match '^\[\s*(\d+)\.(\d+)\]') {
            $t = [double]$Matches[1] + [double]$Matches[2] / 1e6
        } else { continue }
        if ($line -match 'nx2:vi2 229 present #(\d+)') {
            $presents += , @($t, [double]$Matches[1])
        } elseif ($t -ge $warmup -and $line -match '(GPU (?:back )?thread) per second:(.*)$') {
            $thread = $Matches[1]
            foreach ($m in [regex]::Matches($Matches[2], '\| ([a-z/ ]+?) ([\d.]+) ms \((\d+)/s\)')) {
                $key = "$thread|$($m.Groups[1].Value)"
                if (-not $zones.ContainsKey($key)) { $zones[$key] = New-Object System.Collections.ArrayList }
                [void]$zones[$key].Add(@([double]::Parse($m.Groups[2].Value, $inv), [double]$m.Groups[3].Value))
            }
        }
    }
    # Frame time per window between consecutive logged presents (600 apart after the first 4).
    $frames = @()
    for ($i = 1; $i -lt $presents.Count; $i++) {
        $dn = $presents[$i][1] - $presents[$i - 1][1]
        if ($dn -ge 100 -and $presents[$i - 1][0] -ge $warmup) {
            $frames += ($presents[$i][0] - $presents[$i - 1][0]) * 1000 / $dn
        }
    }
    $sorted = $frames | Sort-Object
    $summary = [ordered]@{
        log            = $path
        presents       = if ($presents.Count) { $presents[-1][1] } else { 0 }
        frame_windows  = $frames.Count
        frame_ms_mean  = if ($frames.Count) { ($frames | Measure-Object -Average).Average } else { [double]::NaN }
        frame_ms_p95w  = if ($frames.Count) { $sorted[[math]::Min($sorted.Count - 1, [math]::Floor(0.95 * $sorted.Count))] } else { [double]::NaN }
        zones          = [ordered]@{}
    }
    foreach ($key in ($zones.Keys | Sort-Object)) {
        $list = $zones[$key]
        $summary.zones[$key] = @(
            ($list | % { $_[0] } | Measure-Object -Average).Average,
            ($list | % { $_[1] } | Measure-Object -Average).Average,
            $list.Count)
    }
    $summary
}

function Show([object[]]$results) {
    $names = $results | % { $_.zones.Keys } | Sort-Object -Unique
    "{0,-40} {1}" -f "", (($results | % { "{0,22}" -f (Split-Path $_.log -Leaf) }) -join "")
    "{0,-40} {1}" -f "frame ms (mean of 600-frame windows)", (($results | % { "{0,22:N2}" -f $_.frame_ms_mean }) -join "")
    "{0,-40} {1}" -f "frame ms (p95 window)", (($results | % { "{0,22:N2}" -f $_.frame_ms_p95w }) -join "")
    "{0,-40} {1}" -f "frame windows / presents", (($results | % { "{0,22}" -f "$($_.frame_windows) / $($_.presents)" }) -join "")
    foreach ($n in $names) {
        "{0,-40} {1}" -f $n, (($results | % {
            $z = $_.zones[$n]; if ($z) { "{0,22}" -f ("{0:N1} ms {1,8:N0}/s" -f $z[0], $z[1]) } else { "{0,22}" -f "-" } }) -join "")
    }
    $draws = $results | % { $z = $_.zones["GPU back thread|draw"]; if (-not $z) { $z = $_.zones["GPU thread|draw"] }; if ($z) { $z[1] } }
    $fms = $results | % { $_.frame_ms_mean } | ? { -not [double]::IsNaN($_) }
    foreach ($pair in @(@("draws/s", $draws), @("frame ms", $fms))) {
        $v = @($pair[1]); if ($v.Count -lt 1) { continue }
        $avg = ($v | Measure-Object -Average).Average
        $sd = if ($v.Count -gt 1) { [math]::Sqrt((($v | % { ($_ - $avg) * ($_ - $avg) } | Measure-Object -Sum).Sum) / ($v.Count - 1)) } else { 0 }
        "{0}: mean {1:N2}, min {2:N2}, max {3:N2}, stdev {4:N2} ({5:N1}%)" -f $pair[0], $avg,
            ($v | Measure-Object -Minimum).Minimum, ($v | Measure-Object -Maximum).Maximum, $sd, (100 * $sd / $avg)
    }
}

if ($ParseOnly) {
    Show (@(Get-ChildItem $ParseOnly | % { Parse-Log $_.FullName $Warmup }))
    return
}

if (-not $Game -or -not (Test-Path $Game)) { throw "Game main not found: $Game" }
if (-not (Test-Path $Exe)) { throw "eden-cli not found: $Exe" }
if (Get-Process eden-cli, eden -ErrorAction SilentlyContinue) { throw "eden is already running" }
if (Get-Process nvidia-smi -ErrorAction SilentlyContinue) { throw "nvidia-smi is running (see NX2_NOTES row 130)" }
$bin = Split-Path $Exe
New-Item -ItemType Directory -Force (Join-Path $bin "user"), $OutDir | Out-Null
$log = Join-Path $bin "user\log\eden_log.txt"

$vars = @{
    EDEN_NX2_AUTO_STUB = "1"; EDEN_NX2_GPU_PROFILE = "1"; DISABLE_VULKAN_MEDAL_OBS_CAPTURE = "1"
}
if ($KeyScript) { $vars.EDEN_NX2_KEY_SCRIPT = $KeyScript }
foreach ($k in $Env.Keys) { $vars[$k] = $Env[$k] }

if ($Preset -eq "row128") {
    $Config = @{ gpu_accuracy = "0"; astc_recompression = "2"; use_reactive_flushing = "false";
                 use_asynchronous_shaders = "true" } + $Config
} elseif ($Preset) { throw "Unknown preset: $Preset" }
$extraArgs = @()
if ($Config.Count) {
    $baseIni = Join-Path $bin "user\config\sdl2-config.ini"
    if (-not (Test-Path $baseIni)) { throw "No $baseIni yet: run once without -Config first" }
    $lines = [Collections.Generic.List[string]](Get-Content $baseIni)
    foreach ($key in $Config.Keys) {
        $at = -1
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -eq "$key\default=true" -or $lines[$i] -eq "$key\default=false") { $lines[$i] = "$key\default=false" }
            elseif ($lines[$i] -like "$key=*") { $lines[$i] = "$key=$($Config[$key])"; $at = $i }
        }
        if ($at -lt 0) { throw "Setting $key not found in $baseIni" }
    }
    $variantIni = Join-Path $OutDir "$Tag-config.ini"
    Set-Content -Path $variantIni -Value $lines -Encoding ascii
    $extraArgs = @("-c", "`"$variantIni`"")
    Write-Host "config: $variantIni ($(($Config.Keys | % { "$_=$($Config[$_])" }) -join ', '))"
}

$results = @()
$first = if ($WarmupRun) { 0 } else { 1 }
for ($r = $first; $r -le $Runs; $r++) {
    $busy = Get-Process | ? { $_.CPU -ne $null } | Sort-Object CPU -Descending | Select -First 1
    $cpu1 = (Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -Filter "Name='_Total'").PercentProcessorTime
    Write-Host "run $r/$Runs ($Tag): host CPU $cpu1% before start"
    $saved = @{}
    foreach ($k in $vars.Keys) { $saved[$k] = [Environment]::GetEnvironmentVariable($k); [Environment]::SetEnvironmentVariable($k, $vars[$k]) }
    try {
        $p = Start-Process -FilePath $Exe -ArgumentList (@("-g", "`"$Game`"") + $extraArgs) -WorkingDirectory $bin -PassThru
    } finally {
        foreach ($k in $saved.Keys) { [Environment]::SetEnvironmentVariable($k, $saved[$k]) }
    }
    $exited = $p.WaitForExit($Seconds * 1000)
    if (-not $exited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() } else { Write-Warning "eden-cli exited early (code $($p.ExitCode))" }
    Start-Sleep -Milliseconds 500
    $dest = Join-Path $OutDir ("{0}-{1}-{2}.log" -f $Tag, (Get-Date -Format "MMdd-HHmmss"), $(if ($r) { $r } else { "warmup" }))
    Copy-Item $log $dest
    if ($r -eq 0) { Write-Host "  warm-up run kept as $dest, not counted"; continue }
    $res = Parse-Log $dest $Warmup
    $results += $res
    Write-Host ("  {0}: {1} presents, frame {2:N2} ms" -f (Split-Path $dest -Leaf), $res.presents, $res.frame_ms_mean)
}
Show $results
