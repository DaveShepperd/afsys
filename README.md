# afsys

Filesystem I/O for Atari Games and Midway Games West game disks and images. (FYI: in 1995 Atari Games was purchased by Midway so Atari Games became Midway Games West and remained
so until 2003 when Midway Games West closed up shop).

In the early 1990's Atari Games started making games having a hard disk. At the time there was some concern about the longevity of the data on those cheap disks being subjected to
the extremely harsh environments they experienced in the game arcades so I developed a filesystem that kept multiple copies of each file (settable at compile time, but remained 3
on all systems). As it turned out, that was a complete waste of time being that if the drive began having read failues, it didn't much matter whether there were multliple
copies of files since within an ever shortening period of time the drive would just quit working altogether and need replacing. I suppose it meant the unit lasted a few more months
than it would have otherwise, but I doubt any system survived what it was meant to.

>[!NOTE]
>I've been working on a Linux driver for this Atari/MidwayGames filesystem. It can be found in the repo [mgwfs](https://github.com/daveshepperd/mgwfs.git).
>At this writing, it is readonly and only builds and runs on Fedora 41 and UbuntuLTS24. Someday it may be fully read/write and possibly ported to PiOS.

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

Note, there may or may not be a partition table depending on the game. The Partition table, if there is one, is always found in sector 0 on the disk and the game's filesystem
always ignores sector 0. The upshot of this is you always have to refer to the disk (or its image) in a raw mode. I.e. /dev/sdg and not with /dev/sdg1, etc. If a disk image
is made, it too has to start the copy at sector 0 of the disk. I.e. `dd if=/dev/sdg of=disk.img` instead of `dd if=/dev/sdg1 of=disk.img`.

# Examples

Here is a technique one might use to extract all the files off a game disk image. First create an empty directory and cd into it:

`mkdir game_files; cd game_files`

Run afsys like this:

`afsys -U disk.img -lf > ../game_files.cmd`

Edit `../game_files.cmd` and change the line `DEFAULT UNIX=/d0 GAME=/d0 COPIES=1` to `DEFAULT UNIX=. GAME=/d0 COPIES=1`. Run afsys again:

`afsys -U disk.img -c ../game_files.cmd -u`

You should find all the files and directories present in the current default directory.

If you want to write an updated file back to the game disk, this ought to work:

`afsys -U disk.img -c ../game_files.cmd -t`

If for some inexplicable reason you want to create a new game disk image and copy all the game files to the new disk image, this ought to work:

`afsys -U disk.img -c ../game_files.cmd -F -N -t`

Note with **San Francisco Rush Tournament Edition**, you must use -P instead of -N when also using -F to preserve the partition table that game uses.

