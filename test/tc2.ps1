# Test case 2: NOT using wraptree: kill dispatcher and verify living orphans 
# --------------------------------------------------------------------------

function TestCase2 {
	$ok = $true

	Write-Host
	Write-Host "Test case 2: start"

	Write-Host "GIVEN the process sequence 'Dispatcher, Launcher, Notepad' is started"
	Write-Host "WHEN the Dispatcher is killed"
	Write-Host "THEN the Launcher and Notepad are still running"

	# Start dispatcher (wraptree NOT used)
	Write-Host "Starting $disp"
	Start-Process -FilePath "powershell" -ArgumentList ".\$disp"

	# Wait a while to give system time to start all processes
	Start-Sleep -Seconds 10

	# Gather all PID's of started processes
	$disp_pid = Get-Pid("dispatcher.pid")
	$laun_pid = Get-Pid("launcher.pid")
	$note_pid = Get-Pid("notepad.pid")

	Write-Host "PIDs: dispatcher=$disp_pid; launcher=$laun_pid; notepad=$note_pid"

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

	# Verify that all orphan processes survived and clean up
	Write-Host "Verifying"

	$alive = Expect-Alive -AppPid $laun_pid -Application "Launcher" -Cleanup $true
	if (-Not $alive) {
		$ok = $false;
	}

	$alive = Expect-Alive -AppPid $note_pid -Application "Notepad" -Cleanup $true
	if (-Not $alive) {
		$ok = $false;
	}

	Write-Host "Test case 2: return $ok"
	return $ok
}