# Test case 3: using wraptree: kill dispatcher after first killing the launcher
# and verify termination of children 
# -------------------------------------------------------------------------------

function TestCase3 {
    Param($wraptree)

    $ok = $true

    Write-Host
    Write-Host "Test case 3: start"

    Write-Host "GIVEN the process sequence 'WrapTree, Dispatcher, Launcher, Notepad' is started"
    Write-Host "WHEN the Launcher is killed"
    Write-Host "THEN the Dispatcher is still running"
    Write-Host "AND Notepad is still running"
    Write-Host "WHEN the Dispatcher is killed"
    Write-Host "THEN Notepad is terminated"
    Write-Host "---"

    # Start dispatcher
    Write-Host "Starting $disp"
    Start-Process -FilePath "$wraptree" -ArgumentList "powershell .\$disp"

    # Wait a while to give system time to start all processes
    Start-Sleep -Seconds 10

    # Gather all PID's of started processes
    $disp_pid = Get-Pid("dispatcher.pid")
    $laun_pid = Get-Pid("launcher.pid")
    $note_pid = Get-Pid("notepad.pid")

    Write-Host "PIDs: dispatcher=$disp_pid; launcher=$laun_pid; notepad=$note_pid"

    # Kill launcher
    Write-Host "Killing launcher.ps1"
    try {
        Stop-Process -Id $laun_pid -ErrorAction stop
    } catch {
        Write-Host "Test execution failed: could not terminate Launcher"
        $ok = $false
    }

    # Wait a while to give system time to get stable
    Start-Sleep -Seconds 10

    # Verify that the dispatcher is still alive and keep it alive
    $alive = Expect-Alive -AppPid $disp_pid -Application "Dispatcher" -Cleanup $false
    if (-Not $alive) {
        $ok = $false;
    }

    # Verify that notepad is still alive and keep it alive
    $alive = Expect-Alive -AppPid $note_pid -Application "Notepad" -Cleanup $false
    if (-Not $alive) {
        $ok = $false;
    }

    # Kill dispatcher
    Write-Host "Killing $disp"
    try {
        Stop-Process -Id $disp_pid -ErrorAction stop
    } catch {
        Write-Host "Test execution failed: could not terminate $disp"
        $ok = $false
    }

    # Wait a while to give system time to get rid of all processes
    Start-Sleep -Seconds 10

    # Verify that other orphan processes have been terminated
    Write-Host "Verifying"
    
    $dead = Expect-Dead -AppPid $note_pid -Application "Notepad"
    if (-Not $dead) {
        $ok = $false
    }

    # Clean up
    Remove-Item "dispatcher.pid"
    Remove-Item "launcher.pid"
    Remove-Item "notepad.pid"

    Write-Host "Test case 3: return $ok"
    return $ok
}