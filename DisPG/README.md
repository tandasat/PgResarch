DisPG
======

This is proof-of-concept code disabling PatchGuard on XP SP2, Vista SP2, 7 SP1
and certain build version of 8.1 at run-time. If you are targeting Windows 8.1,
use [meow](https://github.com/tandasat/meow) over this as DisPG does not
support recent builds of 8.1.

See [NOTE.md](NOTE.md) for implementation details.


Demo
-----

Runtime Disabling PatchGuard on [Win8.1](https://www.youtube.com/watch?v=jO0o3XEuDrk)
and [Win7 SP1](https://www.youtube.com/watch?v=WTHXiRTID9g)(Youtube)

Note that the rootkit function in those demo is not included.


Important
----------

This program is not going to work forever since PatchGuard is a moving target.
I will notify you with updating this note when it happened. I am probably
not going to update code for fix, however.


Installation
-------------

Get an archive file for compiled files form this link:

    https://github.com/tandasat/PgResarch/releases/latest

On the x64 platform, you have to enable test signing to install the driver.
To do that, open the command prompt with the administrator privilege and type
the following command, and then restart the system to activate the change:

    bcdedit /set {current} testsigning on

To install the driver, extract the archive file and make sure that Internet
connection is available since this program needs to download symbol files unless
your system already has right symbol files.

Optionally, you may want to use DebugView in order to see logs from the driver.

Then, execute DisPGLoader.exe with the Administrator privilege. It addresses
symbols and loads the driver, which disables PatchGuard. On successful
installation, you should see messages like this:

    FFFFF8030A2F8D10 : ntoskrnl!ExAcquireResourceSharedLite
    ...
    Loading the driver succeeded.
    Press any key to continue . .


Also, you should see messages like below in DebugView.

On Windows 8.1:

    [    4:   58] Initialize : Starting DisPG.
    [    4:   58] Initialize : PatchGuard has been disarmed.
    [    4:   58] Initialize : Enjoy freedom ;)
    [    4:  10c] PatchGuard xxxxxxxxxxxxxxxx : blahblahblah.
    [    4:  10c] PatchGuard yyyyyyyyyyyyyyyy : blahblahblah.

Each output with 'PatchGuard' shows execution of validation by PatchGuard, yet
none of them should cause BSOD because it has been disarmed. xxxxxxxxxxxxxxxx
and yyyyyyyyyyyyyyyy are addresses of PatchGuard contexts. It may or may not
change each time, but after rebooting Windows, you will see different patterns
as most of random factors are decided at the boot time.

On Windows 7 and older:

    [    4:   52] Initialize : Starting DisPG.
    [    4:   52] PatchGuard FFFFFA800195914C : XorKey 0000000000000000
    [    4:   52] PatchGuard FFFFFA8003BBF11D : XorKey 63E62F12D1DEC502
    [    4:   52] Initialize : PatchGuard has been disarmed.
    [    4:   52] Initialize : Enjoy freedom ;)

On those platforms, DisPG locates and disables all PatchGuard contexts at once.
In this example, there are two contexts at FFFFFA800195914C and FFFFFA8003BBF11D
encoded with XOR keys 0 and 63E62F12D1DEC502, respectively.

Note that DisPG is not loaded automatically after system reboot.


Uninstallation
---------------
It cannot be stopped and removed at runtime as it is just concept code. In order
to uninstall DisPG, you can reboot Windows and simply delete all files you copied.


Usage
------

Once you started and disabled PatchGuard, you are free to install your own tools
using hooks. [RemoteWriteMonitor](https://github.com/tandasat/RemoteWriteMonitor)
is an example of this type of tools.


Tested Platforms
-----------------
- Windows 8.1 x64 (ntoskrnl.exe versions: 17085, 17041, 16452)
- Windows 7 SP1 x64
- Windows Vista SP2 x64 (See the note below)
- Windows XP SP2 x64 (See the note below)

Note that due to the fact that recent WDK and Visual Studio no longer support
drivers targeting Windows Vista and older, the compiled driver file through
Visual Studio may not work properly. Try the revision 6f35952 as the last
verified version for those platforms. Building of the revision will require old
Visual Studio and WDK and is not supported by the author anymore, however.


License
-----------------
This software is released under the MIT License, see LICENSE.
