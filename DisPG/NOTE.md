How DisPG Disarms PatchGuard
---------------

Before you start to read these explanations, I strongly recommend to take a look at Skape's article [[4]](http://www.uninformed.org/?v=3&a=3) to get general ideas about the internals of PatchGuard and some terms.

### On Windows 7 and Older Platforms
DisPG attacks poor encryption with a method briefly described in [[1]](http://www.mcafee.com/us/resources/reports/rp-defeating-patchguard.pdf). When a PatchGuard context is waiting for being executed, it remains encrypted on an arbitrary memory location so that an attacker cannot easily find a PatchGuard context and attack it. There are, however, two flaws in this protection design in terms of securing PatchGuard from being attacked.

#### 1. Encryption is done only once by simple XOR operations.

  Although we do not know an XOR key value for the encryption, we know that a properly decrypted PatchGuard context shows the same binary pattern as CmpAppendDllSection(), so we can derive the key value by doing XOR with the binary pattern of CmpAppendDllSection(). Let's see an example.

  This is how XOR encryption is operated:

        CmpAppendDllSection[0..] ^ UnknownXorKey => EncryptedKernelMemory[0..]

  Now, we blindly apply XOR operations with CmpAppendDllSection's binary pattern to kernel memory where may contain an encrypted PatchGuard context.

        ArbitraryKernelMemory[0] ^ CmpAppendDllSection[0] => PossibleXorKey

  If ArbitraryKernelMemory[0] is actually EncryptedKernelMemory[0], gotten PossibleXorKey should be the same as UnknownXorKey. To determine if PossibleXorKey is really UnknownXorKey (as we do not know the value of UnknownXorKey), we can apply XOR with PossibleXorKey to following ArbitraryKernelMemory and see if the XOR-ed values become CmpAppendDllSection.

        ArbitraryKernelMemory[1] ^ PossibleXorKey => DecryptedKernelMemory
        Is DecryptedKernelMemory == CmpAppendDllSection[1]?
        ArbitraryKernelMemory[2] ^ PossibleXorKey => DecryptedKernelMemory
        Is DecryptedKernelMemory == CmpAppendDllSection[2]?
        ...

  If you succeeded in decrypting CmpAppendDllSection(), PossibleXorKey is a value used in XOR encryption, so you can decrypt an entire PatchGuard context and modify it whatever you want. DisPG overwrites an entry point of the function that is first called when PatchGuard gets executed before any validation, which is CmpAppendDllSection(), with a RET instruction to disable PatchGuard.

#### 2. A PatchGuard context is always located in the certain memory region

  It would be impossible to conduct the described attack above within a realistic time period if you had to seek entire kernel memory to try XOR. Fortunately for you, a PatchGuard context always resides in a memory region tracked by a structure called PoolBigPageTable. This table holds pointers to allocated pools whose sizes are bigger than the page size. As a size of a PatchGuard context is big enough to be handled by this table, and there is no other mechanism to allocate a PatchGuard context, all PatchGuard contexts can be found farily quickly by enumerating PoolBigPageTable and trying the XOR attack.

Exploiting these flaws allows you to find and modify PatchGuard contexts without heavily deoending on hard-coded values.

As a note, there is another runtime bypass method on Windows 7 and older platforms; that installs a patch on RtlCaptureContext() [[3]](http://vrt-blog.snort.org/2014/08/the-windows-81-kernel-patch-protection.html).

### On Windows 8.1

#### Countermeasures Against Old Attack Vectors

DisPG, on the other hand, takes a completely different approach on Windows 8.1 since the issues on Windows 7 and older platforms were addressed and no longer exploitable.

Regarding the issue of XOR, Windows 8.1 introduced more than one level of XOR operations with different keys so that an attacker cannot decrypt code easily. Against the second issue, the latest PatchGuard has gotten a possibility of being located in another type of a memory region where is not directly tracked by anyone. This region is used when a memory is allocated by MmAllocateIndependentPages() and cannot be found without traversing the page table, which is far more expensive than enumerating PoolBigPageTable to my knowledge.

#### The Attack Vector

Although it may be still possbile to overcome these countermeasures, DisPG takes a different way on Windows 8.1; that is installing patches into some functions used by PatchGuard. Here is a list of these target functions:

1. KiCommitThreadWait() and KiAttemptFastRemovePriQueue()
1. KeDelayExecutionThread() and KeWaitForSingleObject()
1. Pg_IndependentContextWorkItemRoutine() (at HalPerformEndOfInterrupt() - 0x4c)

In order to understand the reason why they are targeted, let's take a look at the states of PatchGuard contexts shown here to see how these functions are used (assume that a current state of a PatchGuard context is state 1, although it can be any of these):

1. Waiting for being executed via one of timer mechanisms
1. Running DPC\_LEVEL operations such as calling the first validation routine (hereinafter referred to as Pg\_SelfValidation()), and ExQueueWorkItem() in order to enqueue the PatchGuard context for PASSIVE\_LEVEL operations.
1. Waiting for being dequeued from a work item queue through KiCommitThreadWait() or KiAttemptFastRemovePriQueue().
1. Running very beginning of PASSIVE\_LEVEL operations that possibly call some sleep functions.
1. Waiting some time in either KeDelayExecutionThread() or KeWaitForSingleObject() if they were called at state 4.
1. Running remaining PASSIVE\_LEVEL operations such as executing the second validation routine (FsRtlMdlReadCompleteDevEx()) and schduling a next PatchGuard context into one of timer mechanisms (go to state 1)

The basic idea is _catch all PatchGuard contexts before they run the second validation._ The targeted functions listed above were selected for this porpose.

First, patching KiCommitThreadWait() and KiAttemptFastRemovePriQueue() allows you to see what work items are being dequeued and filter them if they are PatchGuard related items. It is important to prevent a PatchGuard context from switching from state 3 to state 4-6 by this filtering because state 6 detects any patch you made and calls SdbpCheckDll() to stop the system. It is, however, still safe to make patches if a PatchGuard context is in either state 1, 2 or 3 unless you modify certain functions and data structures validated by Pg\_SelfValidation() at state 2 (hereinafter referred to as protected functions). These functions include ExQueueWorkItem() and ExpWorkerThread(); so we cannot patch these functions to find PatchGuard related work items more directly.

Second, by modifying the end of KeDelayExecutionThread() and KeWaitForSingleObject(), you can detect threads about to move from state 5 to 6 by checking their return addresses (remember state 5 is executing PatchGuard related functions, so you can tell whether the return address is one of these functions). If you do not handle them properly, they will detect patches on KiCommitThreadWait() and KiAttemptFastRemovePriQueue() and cause system stop.

In addition to that, Pg\_IndependentContextWorkItemRoutine() needs to be handled. This is an unnamed function located at HalPerformEndOfInterrupt() - 0x4c and called instead of ExQueueWorkItem() on state 2 to shift to state 3 only when PatchGuard scheduling method '3 - PsCreateSystemThread' (see [[2]](http://blog.ptsecurity.com/2014/09/microsoft-windows-81-kernel-patch.html)) was selected. In order to handle this case, we additionally need to install the patch on Pg\_IndependentContextWorkItemRoutine() to catch PatchGuard related threads since work items are not used. Interestingly, in spite of the fact that ExQueueWorkItem() is one of protected functions (validated on state 2) and Pg\_IndependentContextWorkItemRoutine() is a replacement of this function, it is not a protected function and can be modified without being checked by Pg_SelfValidation().

By installing these patches, you should be able to eliminate all PatchGuard contexts before they run into the second validation routine without being detected by the fiest validation routine.

#### Faults and Other Attack Vectors

There is a possibility that a PatchGuard can detect these modification. If you installed the patches when one of PatchGuard contexts is state 4 and switched to state 6 without calling sleep functions, or in the middle of state 6, the second validation routine will report the corruption, although this windows is short, and I have not faced this condition yet.

Also, since it employes full of hard-coded values that heavily depending on binary versions of ntoskrnel.exe, this approach is not as generic as the ways described in [2]. If you are interested in PatchGuard, regardless of whether you want to disable it, it will be really fun to implement those generic attack vectors.

### References

1. [Defeating PatchGuard : Bypassing Kernel Security Patch Protection in Microsoft Windows](http://www.mcafee.com/us/resources/reports/rp-defeating-patchguard.pdf), Deepak Gupta and  Xiaoning Li, McAfee Labs, September 2012
1. [Microsoft Windows 8.1 Kernel Patch Protection Analysis & Attack Vectors](http://blog.ptsecurity.com/2014/09/microsoft-windows-81-kernel-patch.html), Mark Ermolov and Artem Shishkin, Positive Research, September 2014
1. [The Windows 8.1 Kernel Patch Protection](http://vrt-blog.snort.org/2014/08/the-windows-81-kernel-patch-protection.html), Andrea Allievi, VRT blog, August 2014
1. [Bypassing PatchGuard on Windows x64](http://www.uninformed.org/?v=3&a=3), Skape, Uninformed, December 2005

