DisPG
==========================

This is proof-of-concept code to encourage security researchers to examine 
PatchGuard more by showing actual code that disables PatchGuard at runtime.

It does following things:

- disarms PatchGuard on certain patch versions of XP SP2, Vista SP2, 7 SP1 and 8.1 at run-time.
- disables Driver Signing Enforcement and allows you to install an arbitrary unsigned driver so that you can examine the x64 kernel using kernel patch techniques if you need.

See [NOTE.md](NOTE.md) for implementation details.

Demo
------------

This is how it is supposed to work. 

[Runtime Disabling PatchGuard on Win8.1(Youtube)](https://www.youtube.com/watch?v=jO0o3XEuDrk)



Installation
---------------

Get an archive file for compiled files form this link:

    https://github.com/tandasat/RemoteWriteMonitor/releases/latest

### Configuring x64 Win8.1

1. Install x64 Win8.1 (editions should not matter). Using a virtual machine is strongly recommended.
2. Enable test signing.
 1. Launch a command prompt with Administrator privilege.
 2. Execute following commands.

            > bcdedit /copy {current} /d "Test Signing Mode"
            The entry was successfully copied to {xxxx}.
            > bcdedit /set {xxxx} TESTSIGNING ON

3. Copy the \x64\Release folder to the test box (a location should not matter).
4. Shutdown Windows.
5. (Optional) Take a snapshot if you are using a VM.

### Getting Ready for Execution
1. Boot Windows in "Test Signing Mode" mode.
2. Execute [Dbgview](http://technet.microsoft.com/en-ca/sysinternals/bb896647.aspx)
   with Administrator privilege and enable Capture Kernel.

### Executing and Monitoring
Run DisPGLoader.exe with Administrator privilege and internet connection so 
that it can download debug symbols. You should see following messages.

	FFFFF8030A2F8D10 : ntoskrnl!ExAcquireResourceSharedLite
	...
	Loading the driver succeeded.
	Press any key to continue . . .

And also should see following messages in DebugView.

	[   4:  58] Initialize : Starting DisPG.
	[   4:  58] Initialize : PatchGuard has been disarmed.
	[   4:  58] Initialize : Driver Signing Enforcement has been disabled.
	[   4:  58] Initialize : Enjoy freedom ;)
	[   4: 10c] PatchGuard xxxxxxxxxxxxxxxx : blahblahblah.
	[   4: 10c] PatchGuard yyyyyyyyyyyyyyyy : blahblahblah.

Each output with 'PatchGuard' shows execution of validation by
PatchGuard, yet none of them should cause BSOD because it has been disarmed.
xxxxxxxxxxxxxxxx and yyyyyyyyyyyyyyyy are addresses of PatchGuard contexts.
It may or may not change each time, but after rebooting Windows, you will
see different patterns as most of random factors are decided at the boot
time.

Note that you will see different output when you run the code on Windows 7,
Vista and XP because an implementation of disarming code for them is completely different.


When you reboot Windows, DisPG will not be reloaded automatically.

Uninstallation
---------------
It cannot be stopped and removed at runtime as it is just concept code. In order
to uninstall DIsPG, you can reboot Windows and simply delete all files you copied.


Tested Platforms
-----------------
- Windows 8.1 x64 (ntoskrnl.exe versions: 17085, 17041, 16452)
- Windows 7 SP1 x64
- Windows Vista SP2 x64
- Windows XP SP2 x64

License
-----------------
This software is released under the MIT License, see LICENSE.


