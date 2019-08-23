README for wraptree-test

Preparation:
- wraptree is in $PATH

Manual execution:
- start PowerShell
- cd <test dir>
- execute wraptree-test by './wraptree-test <location of SUT>', e.g.
  > .\wraptree-test ..\Release\WrapTree.exe
- 'echo $LastExitCode' returns 0 on success and 1 otherwise

TEST CASES
==========

TC1:
- Start dispatcher by using 'wraptree'
- kill dispatcher
- verify that all child processes did terminate too

TC2:
- Start dispatcher directly (NOT using 'wraptree')
- kill dispatcher
- verify that all orphan processes are still running
- after verification clean up by killing the orphans

TC3:
- Start dispatcher by using 'wraptree'
- Notice that even if the launcher is terminated before notepad
  that the latter will be aborted when wraptree-test terminates

EXTRA tests using wraptree (NOT yet implemented)
------------------------------------------------

TC4:
- Notice that even, after making a change in notepad without saving this, 
  the process is terminated, so actually notepad is aborted.
