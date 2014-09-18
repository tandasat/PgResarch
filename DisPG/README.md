DisPG
==========================

This is proof-of-concept code to encourage security researchers to examine PatchGuard more by showing actual code that disables PatchGuard at runtime.

It does following things:

- disarms PatchGuard on certain patch versions of XP SP2, Vista SP2, 7 SP1 and 8.1 at run-time.
- disables Driver Signing Enforcement and allows you to install an arbitrary unsigned driver so that you can examine the x64 kernel using kernel patch techniques if you need.
- hide processes whose names start with 'rk' to demonstrate that PatchGuard is being disarmed.


Installation
---------------
### Configuring x64 Win8.1

1. Install x64 Win8.1 (editions should not matter). Using a virtual machine is strongly recommended.
1. Apply all Windows Updates.
1. Enable test signing.
 1. Launch a command prompt with Administrator privilege.
 1. Execute following commands.

            > bcdedit /copy {current} /d "Test Signing Mode"
            The entry was successfully copied to {xxxx}.
            > bcdedit /set {xxxx} TESTSIGNING ON

1. Copy the bin folder to this test box (a location should not matter).
1. Shutdown Windows.
1. (Optional) Take a snapshot if you are using a VM.

### Getting Ready for Execution
1. Boot Windows in "Test Signing Mode" mode.
1. Execute [Dbgview](http://technet.microsoft.com/en-ca/sysinternals/bb896647.aspx) with Administrator privilege and enable Capture Kernel.

### Executing and Monitoring
1. Run DisPGLoader.exe with Administrator privilege and internet connection so that it can download debug symbols. You should see following messages.

        FFFFF8030A2F8D10 : ntoskrnl!ExAcquireResourceSharedLite
        ...
        Loading the driver succeeded.
        Press any key to continue . . .

   And also should see following messages in DebugView.

        [   4:  58] Initialize : Starting DisPG.
        [   4:  58] Initialize : PatchGuard has been disarmed.
        [   4:  58] Initialize : Hiding processes has been enabled.
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

2. (Optional) Start any process whose name starts with 'rk' and confirm
   that they are not listed in Task Manager or something similar tools.

3. (Optional) Keep Windows running at least 30 minutes to confirm PatchGuard was really
   disabled.

When you reboot Windows, DisPG will not be reloaded automatically.

Uninstallation
---------------
It cannot be stopped and removed at runtime as it is just concept code. In order to uninstall DIsPG, you can reboot Windows and simply delete all files you copied.

How DisPG Disarms PatchGuard
---------------

Before you go through these explanations, it is strongly recommended to read Skape's article [[4]](http://www.uninformed.org/?v=3&a=3) to get general ideas about the internals of PatchGuard and some terms.

### On Windows 7 and Older Platforms
DisPG attacks poor encryption with a method briefly described in [[1]](http://www.mcafee.com/us/resources/reports/rp-defeating-patchguard.pdf). When a PatchGuard context is waiting for being executed, it remains encrypted on an arbitrary memory location. There is, however, two flaws in that design in terms of protecting PatchGuard from being disabled:

1. Encryption is done only once by simple XOR operations.

  Although we do not know an XOR key value, we know that when a PatchGuard context is decrypted, it shows the same binary pattern as CmpAppendDllSection(), we can derive the key value by doing XOR with the binary pattern of CmpAppendDllSection(). Let's see an example.

  This is how XOR encryption is operated:

        CmpAppendDllSection[0..] ^ UnknownXorKey => EncryptedKernelMemory[0..]

  Now, we blindly apply XOR operations for kernel memory where may contain an encrypted PatchGuard context.

        ArbitraryKernelMemory[0] ^ CmpAppendDllSection[0] => PossibleXorKey

  If ArbitraryKernelMemory[0] is actually EncryptedKernelMemory[0], PossibleXorKey should be the same as UnknownXorKey. To determine if PossibleXorKey is really UnknownXorKey, we can apply XOR with that value and see if the XOR-ed values are CmpAppendDllSection.

        ArbitraryKernelMemory[1] ^ PossibleXorKey => DecryptedKernelMemory
        Is DecryptedKernelMemory == CmpAppendDllSection[1]?
        ArbitraryKernelMemory[2] ^ PossibleXorKey => DecryptedKernelMemory
        Is DecryptedKernelMemory == CmpAppendDllSection[2]?
        ...

  If you succeeded in decrypting CmpAppendDllSection(), PossibleXorKey is a value used in XOR encryption, so you can decrypt an entire PatchGuard context and modify it whatever you want. DisPG overwrites an entry point of the function that is first called when PatchGuard gets executed before any validation, which is CmpAppendDllSection(), with a RET instruction to disable PatchGuard.

1. A PatchGuard context is always located in the certain memory region

  It would be impossible to conduct a described attack above within a realistic time period if you had to seek entire kernel memory to try XOR. Fortunately for you, a PatchGuard context always resides in a memory region tracked by a structure called PoolBigPageTable. This table holds pointers to allocated pools whose sizes are bigger than the page size. As a size of a PatchGuard context is big enough to be handled by this table, and there is no other mechanism to allocate a PatchGuard context, all PatchGuard contexts can be found farily quickly by enumerating PoolBigPageTable and trying the XOR attack.

As a side-note, there is another runtime bypass method on Windows 7 and older platforms that installs a patch on RtlCaptureContext() [[3]](http://vrt-blog.snort.org/2014/08/the-windows-81-kernel-patch-protection.html).

### On Windows 8.1

On the other hand, DisPG takes a completely different approach on Windows 8.1 since the issues on Windows 7 and older platforms were addressed and no longer exploitable.

Regarding the issue of XOR, Windows 8.1 started to conduct more than one level XOR operations with different keys so that an attacker cannot decrypt code easily. Against the second issue, the latest PatchGuard has gotten a possibility to be located in another type of a memory region where is not directly tracked by anyone. This region is used when a memory is allocated by MmAllocateIndependentPages() and cannot be found without traversing the page table which is far more expensive than enumerating PoolBigPageTable (to my knowledge).

Simply put, DisPG patches some functions that are used by PatchGuard. These functions are as follows:

1. KiCommitThreadWait() and KiAttemptFastRemovePriQueue()
1. KeDelayExecutionThread() and KeWaitForSingleObject()
1. Pg_IndependentContextWorkItemRoutine() (at HalPerformEndOfInterrupt() - 0x4c)

In order to understand why they are targeted, I shortly explain how PatchGuard context runs. Here is the states of PatchGuard contexts (assume that an initial state is state 1, although it can be any of these):

1. **Waiting** for being executed (the context resides in one of timer mechanisms)
1. Running DPC\_LEVEL operations including calling the first validation routine (hereinafter referred to as Pg\_SelfValidation()), and ExQueueWorkItem() to enqueue the PatchGuard context for PASSIVE\_LEVEL operations.
1. **Waiting** for being dequeued from a work item queue
1. Running PASSIVE\_LEVEL operations including the very beginning of the second validation routine, FsRtlMdlReadCompleteDevEx(), that may call following sleep functions.
1. **May be Waiting** some time in either KeDelayExecutionThread() or KeWaitForSingleObject() if they were called.
1. Running PASSIVE\_LEVEL operations including exercising the second validation and en-queueing a next PatchGuard context into one of timer mechanisms (go to state 1)

Now let's get back to patched functions.

First, patching KiCommitThreadWait() and KiAttemptFastRemovePriQueue() allows you to see what work items are being dequeued and filter them if they are PatchGuard related items. It is important to prevent a PatchGuard context from switching from state 3 to state 4-6 with this filtering because state 6 detects any patch you made and calls SdbpCheckDll() to stop the system. It is, however, still safe to make patches if a PatchGuard context is in either state 1, 2 or 3 unless you modify certain functions and data structures validated by Pg\_SelfValidation() at state 2 (hereinafter referred to as protected functions). They are, for example, ExQueueWorkItem() and ExpWorkerThread(); that is why we cannot patch these functions to filter PatchGuard related work items more directly.

Second, by patching the end of KeDelayExecutionThread() and KeWaitForSingleObject(), you can find threads about to move from state 5 to state 6 by checking their return addresses. If you do not handle them properly, they will detect patches on KiCommitThreadWait() and KiAttemptFastRemovePriQueue() and cause system stop.

Also, Pg\_IndependentContextWorkItemRoutine() needs to be handled. This is an unnamed function located at HalPerformEndOfInterrupt() - 0x4c and used instead of ExQueueWorkItem() at state 2 only when PatchGuard scheduling method '3 - PsCreateSystemThread' (see [[2]](http://blog.ptsecurity.com/2014/09/microsoft-windows-81-kernel-patch.html)) is selected. In that case, since work items are not used, we additionally need to install the patch on Pg\_IndependentContextWorkItemRoutine() to catch threads shifting from state 2 to 3. Interestingly, in spite of the fact that ExQueueWorkItem() is one of protected functions (that is validated on state 2) and Pg\_IndependentContextWorkItemRoutine() works as that function, it is not a protected function so can be modified without being checked by Pg_SelfValidation().

Note that there is a window that a PatchGuard can detect these modification. If you installed the patches when one of PatchGuard contexts is state 4 and switched to state 6 without calling sleep functions, or in the middle of state 6, the second validation routine will report the corruption, although this possibility is not strong, and I have not faced this condition yet.

This approach is not as graceful as the ways described in [2] since it is full of hard-coded values that heavily depending on binary versions of ntoskrnel.exe and installing patches that can be detected by PatchGuard in theory, but it works okay, and the purpose of this code is not providing an implementation that could be used in the real world with malicious intention.

### References
1. [Defeating PatchGuard : Bypassing Kernel Security Patch Protection in Microsoft Windows](http://www.mcafee.com/us/resources/reports/rp-defeating-patchguard.pdf), Deepak Gupta and  Xiaoning Li, McAfee Labs, September 2012
1. [Microsoft Windows 8.1 Kernel Patch Protection Analysis & Attack Vectors](http://blog.ptsecurity.com/2014/09/microsoft-windows-81-kernel-patch.html), Mark Ermolov and Artem Shishkin, Positive Research, September 2014
1. [The Windows 8.1 Kernel Patch Protection](http://vrt-blog.snort.org/2014/08/the-windows-81-kernel-patch-protection.html), Andrea Allievi, VRT blog, August 2014
1. [Bypassing PatchGuard on Windows x64](http://www.uninformed.org/?v=3&a=3), Skape, Uninformed, December 2005

Tested Platforms
-----------------
- Windows 8.1 x64 (ntoskrnl.exe versions: 17085, 17041, 16452)
- Windows 7 SP1 x64 (ntoskrnl.exe versions: 18409, 18247)
- Windows Vista SP2 x64 (ntoskrnl.exe versions: 18881)
- Windows XP SP2 x64 (ntoskrnl.exe versions: 5138)

License
-----------------
This software is released under the MIT License, see LICENSE.


