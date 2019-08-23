# WrapTree

WrapTree is a process wrapper for Windows. When WrapTree exits, it cleans up all processes spawned from its child processes. This is useful in automated testing, to make sure that when a test finishes or is aborted, there are no lingering processes that could interfere with future tests.

WrapTree was written by Sergei Zhirikov at Philips, for use in [SoftFab](https://softfab.io), but it is a standalone tool that could be used in other environments as well.
