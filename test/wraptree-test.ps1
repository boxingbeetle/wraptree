# command line parameters:
# 1: path to application to test
#    defaults to "wraptree.exe" in current directory
#
param (
    [string]$wraptree = "wraptree.exe"
)

. ($PSScriptRoot + "\utils.ps1")
. ($PSScriptRoot + "\tc1.ps1")
. ($PSScriptRoot + "\tc2.ps1")
. ($PSScriptRoot + "\tc3.ps1")

$app_name = (Get-Item $PSCommandPath).Name
$app_base = (Get-Item $PSCommandPath).Basename

[int]$success = 0
[int]$total = 0

Write-Host "Running $app_name"
Write-Host "Testing target application $wraptree"

$result = Test-Path -Path $wraptree
if (-not $result) {
    Write-Host "Target application doesn't exist"
    exit 1
}

$disp = "dispatcher.ps1"
$ok = $true

# test case 1: using 'wraptree'
$total++
$ret = TestCase1($wraptree)
if ($ret) {
    $success++
} else {
    $ok = $false
}

# test case 2: NOT using 'wraptree'
$total++
$ret = TestCase2
if ($ret) {
    $success++
} else {
    $ok = $false
}

# test case 3: using 'wraptree' and killing launcher before the dispatcher
$total++
$ret = TestCase3($wraptree)
if ($ret) {
    $success++
} else {
    $ok = $false
}

# The SoftFab wrapper executing this script will take the last line written 
# and place it in SF_SUMMARY.
Write-Host
Write-Host "$success/$total test cases succeed"

if ($ok) {
    exit 0
}
exit 1
