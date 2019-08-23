. ($PSScriptRoot + "\utils.ps1")

$app_name = (Get-Item $PSCommandPath).Name
$app_base = (Get-Item $PSCommandPath).Basename

Write-Host "Running $app_name"
Write-PidFile($app_base)

Write-Host "Starting notepad"
$Process = [Diagnostics.Process]::Start("notepad")
$npid = $Process.Id
Write-Host "notepad launched with PID $npid"

$np_idfile = "notepad.pid"
$npid > $np_idfile
Write-Host "PID $npid written in $np_idfile"

pause