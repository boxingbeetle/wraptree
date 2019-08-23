# Test case 1: using wraptree: kill dispatcher and verify termination of children 
# -------------------------------------------------------------------------------

function TestCase1 {
	Param($wraptree)

	$ok = $true

	Write-Host
	Write-Host "Test case 1: start"

	Write-Host "GIVEN the process sequence 'WrapTree, Dispatcher, Launcher, Notepad' is started"
	Write-Host "WHEN the Dispatcher is killed"
	Write-Host "THEN the Launcher and Notepad are terminated"

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

	# Verify that all orphan processes have been terminated
	Write-Host "Verifying"
	
	$dead = Expect-Dead -AppPid $laun_pid -Application "Launcher"
	if (-Not $dead) {
		$ok = $false;
	}
	$dead = Expect-Dead -AppPid $note_pid -Application "Notepad"
	if (-Not $dead) {
		$ok = $false;
	}

	Write-Host "Test case 1: return $ok"
	return $ok
}