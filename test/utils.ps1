function Write-PidFile {
  Param($app_base)

  $pid_ext = ".pid"
 
  $app_pidfile = $app_base + $pid_ext
  $PID > $app_pidfile
  Write-Host "PID $PID written in $app_pidfile"
}

function Get-Pid {
    Param(
        [string]$pid_file = ""
    )

    $app_pid = Get-Content -Path $pid_file
    return $app_pid
}

function Expect-Dead {
    Param(
        [string]$AppPid = "0",
        [string]$Application = "<none>"
    )
    
    if (Get-Process -Id $AppPid -ErrorAction silentlycontinue) {
        Write-Host "Error: $Application not terminated"
        
        # Clean up by killing it
        Write-Host "Clean up: now killing $Application"
        try {
            Stop-Process -Id $AppPid -ErrorAction stop
        } catch {
            Write-Host "Test execution failed: could not terminate $Application"
        }

        return $false
    }
    
    Write-Host "Expected: $Application terminated"
    return $true
}

function Expect-Alive {
    Param(
        [string]$AppPid = "0",
        [string]$Application = "<none>",
        [bool]$Cleanup = $false
    )
    
    $ok = $true
    
    if (Get-Process -Id $AppPid -ErrorAction silentlycontinue) {
        Write-Host "Expected: $Application still running"
        
        # If required clean it up by killing it
        if ($Cleanup) {
            Write-Host "Clean up: now killing $Application"
            try {
                Stop-Process -Id $AppPid -ErrorAction stop
            } catch {
                $ok = $false
                Write-Host "Test execution failed: could not terminate $Application"
            }
        }
    } else {
        $ok = $false
        Write-Host "Error: $Application already terminated"
    }
    
    return $ok
}

