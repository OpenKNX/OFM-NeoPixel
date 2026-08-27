#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
Open ■
┬────┴  Test-SerialTiming
■ KNX   2026 OpenKNX - Erkan Çolak

FILEPATH: scripts/Tests/Test-SerialTiming.ps1

.SYNOPSIS
    Regression test for the 1-wire bit-timing solver in src/SerialTimingProfile.h.

.DESCRIPTION
    Mirrors the integer math of SerialTiming::solvePio() and SerialTiming::toTicks() and
    checks every supported protocol against its reference profile.

    Checks per protocol and system clock:
      - a solution exists and every segment fits the PIO delay field (1..16 cycles)
      - the clock divider is an integer, so the state machine cannot dither
      - realized T0H / T1H / bit period stay within tolerance of the profile
      - the RMT tick split gives the 0-bit and the 1-bit the same period

    Reference values follow NeoPixelBus (the timing WLED ships), cross-checked against the
    chip datasheets in doc/Datasheets.

.PARAMETER SysClockHz
    System clocks to test. Defaults to RP2040 (125 MHz) and RP2350 (150 MHz).

.PARAMETER ToleranceNs
    Maximum allowed deviation per edge. Default 25 ns.

.PARAMETER RmtTickNs
    RMT tick length in ns. Default 25 (40 MHz), matching the ESP32 backend.

.EXAMPLE
    .\Test-SerialTiming.ps1

.EXAMPLE
    .\Test-SerialTiming.ps1 -SysClockHz 133000000 -ToleranceNs 30

.NOTES
    Author: Erkan Çolak
    Part of OFM-NeoPixel. PowerShell 5.1 compatible.
    The protocol table is parsed from SerialTimingProfile.h, never duplicated here.
#>

[CmdletBinding()]
param(
    [int[]] $SysClockHz = @(125000000, 150000000),
    [int]   $ToleranceNs = 25,
    [int]   $RmtTickNs = 25
)

function OpenKNX_ShowLogo($AddCustomText = $null) {
    Write-Host ""
    Write-Host "Open " -NoNewline
    Write-Host "$( [char]::ConvertFromUtf32(0x25A0) )" -ForegroundColor Green
    $bar = "$( [char]::ConvertFromUtf32(0x252C) )$( [char]::ConvertFromUtf32(0x2500) )$( [char]::ConvertFromUtf32(0x2500) )$( [char]::ConvertFromUtf32(0x2500) )$( [char]::ConvertFromUtf32(0x2500) )$( [char]::ConvertFromUtf32(0x2534) ) "
    if ($AddCustomText) { Write-Host "$bar $AddCustomText" -ForegroundColor Green }
    else { Write-Host $bar -ForegroundColor Green }
    Write-Host "$( [char]::ConvertFromUtf32(0x25A0) )" -NoNewline -ForegroundColor Green
    Write-Host " KNX"
    Write-Host ""
}

# Largest delay a PIO instruction carries with one side-set bit and no side-set enable.
$script:MaxSegmentCycles = 16

function Get-IntDiv {
    <#
        .SYNOPSIS
            Truncating integer division.
        .DESCRIPTION
            PowerShell's "/" yields a double and a [long] cast rounds it, so a direct port of
            the C++ integer math would drift. Floor matches the C++ for non-negative operands.
    #>
    param([long] $Dividend, [long] $Divisor)
    if ($Divisor -eq 0) { return [long]0 }
    return [long][Math]::Floor([double]$Dividend / [double]$Divisor)
}

function Get-BitPeriodNs {
    param([hashtable] $Profile)
    $zero = $Profile.T0H + $Profile.T0L
    $one  = $Profile.T1H + $Profile.T1L
    return [int](Get-IntDiv -Dividend ($zero + $one + 1) -Divisor 2)
}

function Resolve-PioTiming {
    <#
        .SYNOPSIS
            Integer port of SerialTiming::solvePio().
    #>
    param(
        [hashtable] $Profile,
        [int]       $SysClock
    )

    $bitNs = Get-BitPeriodNs -Profile $Profile
    if ($bitNs -le 0) { return $null }

    $best = $null
    $bestScore = [long]::MaxValue

    for ($div = 1; $div -le 65535; $div++) {
        $cycPs = Get-IntDiv -Dividend ([long]$div * 1000000000000) -Divisor $SysClock
        if ($cycPs -le 0) { continue }

        $total = Get-IntDiv -Dividend (([long]$bitNs * 1000) + (Get-IntDiv -Dividend $cycPs -Divisor 2)) -Divisor $cycPs
        if ($total -lt 3) { break }
        if ($total -gt (3 * $script:MaxSegmentCycles)) { continue }

        $a  = Get-IntDiv -Dividend (([long]$Profile.T0H * 1000) + (Get-IntDiv -Dividend $cycPs -Divisor 2)) -Divisor $cycPs
        $ab = Get-IntDiv -Dividend (([long]$Profile.T1H * 1000) + (Get-IntDiv -Dividend $cycPs -Divisor 2)) -Divisor $cycPs
        if ($a -lt 1) { $a = 1 }
        if ($ab -le $a) { $ab = $a + 1 }
        $b = $ab - $a
        if ($total -le $ab) { continue }
        $c = $total - $ab

        if ($a -gt $script:MaxSegmentCycles -or $b -gt $script:MaxSegmentCycles -or $c -gt $script:MaxSegmentCycles) { continue }

        $gotT0h = Get-IntDiv -Dividend ($a * $cycPs) -Divisor 1000
        $gotT1h = Get-IntDiv -Dividend ($ab * $cycPs) -Divisor 1000
        $gotBit = Get-IntDiv -Dividend ($total * $cycPs) -Divisor 1000

        $e0 = [Math]::Abs($gotT0h - $Profile.T0H)
        $e1 = [Math]::Abs($gotT1h - $Profile.T1H)
        $eb = [Math]::Abs($gotBit - $bitNs)
        $score = ($e0 * 2) + ($e1 * 2) + ($eb * 3)

        if ($score -lt $bestScore) {
            $bestScore = $score
            $best = @{
                A = $a; B = $b; C = $c
                ClkDiv = $div; CyclesPerBit = $total
                T0H = $gotT0h; T1H = $gotT1h; Bit = $gotBit
                T0L = Get-IntDiv -Dividend (($b + $c) * $cycPs) -Divisor 1000
                T1L = Get-IntDiv -Dividend ($c * $cycPs) -Divisor 1000
            }
        }
    }
    return $best
}

function Resolve-RmtTicks {
    <#
        .SYNOPSIS
            Integer port of SerialTiming::toTicks().
    #>
    param(
        [hashtable] $Profile,
        [int]       $TickNs
    )

    $bit = [int](Get-IntDiv -Dividend ((Get-BitPeriodNs -Profile $Profile) + (Get-IntDiv -Dividend $TickNs -Divisor 2)) -Divisor $TickNs)
    if ($bit -lt 2) { $bit = 2 }

    $t0h = [int](Get-IntDiv -Dividend ($Profile.T0H + (Get-IntDiv -Dividend $TickNs -Divisor 2)) -Divisor $TickNs)
    $t1h = [int](Get-IntDiv -Dividend ($Profile.T1H + (Get-IntDiv -Dividend $TickNs -Divisor 2)) -Divisor $TickNs)
    if ($t0h -lt 1) { $t0h = 1 }
    if ($t1h -lt 1) { $t1h = 1 }
    if ($t0h -gt ($bit - 1)) { $t0h = $bit - 1 }
    if ($t1h -gt ($bit - 1)) { $t1h = $bit - 1 }

    return @{ T0H = $t0h; T0L = $bit - $t0h; T1H = $t1h; T1L = $bit - $t1h; Bit = $bit }
}

function Get-ProfilesFromHeader {
    <#
        .SYNOPSIS
            Read the protocol table out of SerialTiming::profileFor().
        .DESCRIPTION
            The table is parsed instead of duplicated here: a mirror kept in step by hand
            passes just as happily after the header changed, which is the one failure a
            regression test must not have.
    #>
    param([string] $HeaderPath)

    if (-not (Test-Path $HeaderPath)) {
        throw "Header not found: $HeaderPath - run this from the OFM-NeoPixel root."
    }

    $profiles = @()
    $pending = @()
    $inSwitch = $false

    foreach ($line in (Get-Content $HeaderPath -Encoding UTF8)) {
        if ($line -match 'inline\s+Profile\s+profileFor') { $inSwitch = $true; continue }
        if (-not $inSwitch) { continue }

        if ($line -match 'case\s+LedProtocol::(\w+)\s*:') {
            $pending += $Matches[1]
            continue
        }

        if ($line -match 'return\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(true|false)\s*\}') {
            if ($pending.Count -gt 0) {
                $profiles += @{
                    Name     = ($pending -join '/')
                    T0H      = [int]$Matches[1]
                    T0L      = [int]$Matches[2]
                    T1H      = [int]$Matches[3]
                    T1L      = [int]$Matches[4]
                    ResetUs  = [int]$Matches[5]
                    Inverted = ($Matches[6] -eq 'true')
                }
                $pending = @()
            }
            continue
        }

        # The default branch closes the table; everything after it is not a profile.
        if ($line -match '^\s*\}\s*$' -and $profiles.Count -gt 0 -and $pending.Count -eq 0) { break }
    }

    if ($profiles.Count -eq 0) { throw "No profiles parsed from $HeaderPath - has profileFor() changed shape?" }
    return $profiles
}

$HeaderPath = Join-Path $PSScriptRoot "..\..\src\SerialTimingProfile.h"
$Profiles = Get-ProfilesFromHeader -HeaderPath $HeaderPath

OpenKNX_ShowLogo "Serial timing solver test"

$failures = 0
$checks = 0

foreach ($sys in $SysClockHz) {
    Write-Host ("PIO solver, system clock {0:N0} Hz" -f $sys) -ForegroundColor Yellow
    Write-Host ("  {0,-20} {1,4} {2,4} {3,3} {4,3} {5,3}  {6,-13} {7,-13} {8,-13} {9}" -f `
        'protocol', 'div', 'cyc', 'a', 'b', 'c', 'T0H want/got', 'T1H want/got', 'bit want/got', 'result')

    foreach ($p in $Profiles) {
        $checks++
        $sol = Resolve-PioTiming -Profile $p -SysClock $sys
        $wantBit = Get-BitPeriodNs -Profile $p

        if ($null -eq $sol) {
            Write-Host ("  {0,-20} no solution" -f $p.Name) -ForegroundColor Red
            $failures++
            continue
        }

        $e0 = [Math]::Abs($sol.T0H - $p.T0H)
        $e1 = [Math]::Abs($sol.T1H - $p.T1H)
        $eb = [Math]::Abs($sol.Bit - $wantBit)
        $worst = ($e0, $e1, $eb | Measure-Object -Maximum).Maximum

        $segmentsOk = ($sol.A -ge 1 -and $sol.A -le $script:MaxSegmentCycles -and
                       $sol.B -ge 1 -and $sol.B -le $script:MaxSegmentCycles -and
                       $sol.C -ge 1 -and $sol.C -le $script:MaxSegmentCycles)
        $sumOk = (($sol.A + $sol.B + $sol.C) -eq $sol.CyclesPerBit)
        $ok = ($worst -le $ToleranceNs) -and $segmentsOk -and $sumOk

        $verdict = if ($ok) { 'OK' } else { 'FAIL' }
        $colour  = if ($ok) { 'Green' } else { 'Red' }
        if (-not $ok) { $failures++ }

        Write-Host ("  {0,-20} {1,4} {2,4} {3,3} {4,3} {5,3}  {6,5}/{7,-7} {8,5}/{9,-7} {10,5}/{11,-7} {12} ({13} ns)" -f `
            $p.Name, $sol.ClkDiv, $sol.CyclesPerBit, $sol.A, $sol.B, $sol.C,
            $p.T0H, $sol.T0H, $p.T1H, $sol.T1H, $wantBit, $sol.Bit, $verdict, $worst) -ForegroundColor $colour
    }
    Write-Host ""
}

Write-Host ("RMT tick split, {0} ns per tick" -f $RmtTickNs) -ForegroundColor Yellow
foreach ($p in $Profiles) {
    $checks++
    $t = Resolve-RmtTicks -Profile $p -TickNs $RmtTickNs
    $zero = $t.T0H + $t.T0L
    $one  = $t.T1H + $t.T1L
    $ok = ($zero -eq $one) -and ($t.T0H -ge 1) -and ($t.T1H -ge 1) -and ($t.T0L -ge 1) -and ($t.T1L -ge 1)
    if (-not $ok) { $failures++ }

    $verdict = if ($ok) { 'OK' } else { 'FAIL' }
    $colour  = if ($ok) { 'Green' } else { 'Red' }
    Write-Host ("  {0,-20} T0 {1,5}/{2,-5} T1 {3,5}/{4,-5} bit {5,5} ns  equal={6}  {7}" -f `
        $p.Name, ($t.T0H * $RmtTickNs), ($t.T0L * $RmtTickNs), ($t.T1H * $RmtTickNs), ($t.T1L * $RmtTickNs),
        ($t.Bit * $RmtTickNs), $(if ($zero -eq $one) { 'yes' } else { 'NO' }), $verdict) -ForegroundColor $colour
}

Write-Host ""
if ($failures -eq 0) {
    Write-Host ("All {0} checks passed." -f $checks) -ForegroundColor Green
    exit 0
} else {
    Write-Host ("{0} of {1} checks FAILED." -f $failures, $checks) -ForegroundColor Red
    exit 1
}
