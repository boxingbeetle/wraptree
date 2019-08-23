. ($PSScriptRoot + "\utils.ps1")

$app_name = (Get-Item $PSCommandPath).Name
$app_base = (Get-Item $PSCommandPath).Basename

Write-Host "Running $app_name"
Write-PidFile($app_base)

$lnp = "launcher.ps1"
Write-Host "Starting $lnp"

Start-Process -FilePath "powershell" -ArgumentList ".\$lnp"

pause