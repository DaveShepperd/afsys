# afsys

Filesystem I/O for Atari Games and Midway Games West game disks and images. (FYI: in 1995 Atari Games was purchased by Midway so Atari Games became Midway Games West and remained
so until 2003 when Midway Games West closed up shop).

In the early 1990's Atari Games started making games having a hard disk. At the time there was some concern about the longevity of the data on those cheap disks being subjected to
the extremely harsh environments they experienced in the game arcades so I developed a filesystem that kept multiple copies of each file (settable at compile time, but remained 3
on all systems). As it turned out, that was a complete waste of time being that if the drive began having read failues, it didn't much matter whether there were multliple
copies of files since within an ever shortening period of time the drive would just quit working altogether and need replacing. I suppose it meant the unit lasted a few more months
than it would have otherwise, but I doubt any system survived what it was meant to.

In any case, after the place closed up shop, I thought it might be interesting to adapt what the game code used in a Linux environment to be able to read/write old game disks.
So around 2003 I wrote this program using bits and pieces from game code. Note, in the game the disk I/O was done using threads and was completely async (it used a QIO system
I cooked up). Under Linux and to make it real simple I thought I could just replicate the QIO subsystem to be single threaded. That works just fine especially since there is
no requirement this standalone tool needed async I/O. It was simply an exercise for me to keep the brain functioning with nothing better to do. Nothing ever came of it. Nobody
has ever asked for such a thing and I just stuffed it away in an archive. Lately there's been some interest in resurrecting some old Atari Games and Midway Games West products
from the early 2000's and I've seen some old game code show up on github and other places, so I thought this tool might come in handy for somebody someday.

I've only ever built and tested this code on an x86 Linux system (64 bit kernel) and a Raspberry Pi5 (32 bit kernel, version 8). Seems to work without incident. I just recently
made edits to allow it to build and run properly on a 64 bit kernel. Previously it needed a -m32 option on gcc.

It is not an interactive tool. What has to be done is either feed commands to it via a -c option or run it individually for each file to read or write. The syntax for the commands
is a bit obscure. One way to help find out what you might do is to get a listing of the current disk image in command mode using -lf, edit the result and feed it back via -c.

Sorry, but it's been too long. I don't remember what all the different commands do or what can be done. I never made any documentation on how to work the tool. Probably it's
best just to use the tool to replace file(s) with more updated ones or to pick them off the filesystem to see what's in them but otherwise leave things alone on the game disk.
