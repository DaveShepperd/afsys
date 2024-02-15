/* See LICENSE.txt for license details */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char *argv[]) {
    struct stat st;
    int sts;
    struct tm *tim;

    if (argc < 2) {
	printf("Usage: sstat filename\n");
	return 1;
    }

    sts = stat(argv[1], &st);
    if (sts < 0) {
	perror("Unable to stat file");
	return 2;
    }
    printf("File %s:\n", argv[1]);
    printf("\tdevice=%ld\n", st.st_dev);
    printf("\tinode=%ld\n", st.st_ino);
    printf("\tlink count=%d\n", st.st_nlink);
    printf("\tuid=%d\n", st.st_uid);
    printf("\tgid=%d\n", st.st_gid);
    printf("\trdevice=%ld\n", st.st_rdev);
    printf("\tsize=%ld\n", st.st_size);
    printf("\tblocksize=%ld\n", st.st_blksize);
    printf("\tblocks=%ld\n", st.st_blocks);
    tim = localtime(&st.st_atime);
    printf("\tatime=0x%08lX %02d/%02d/%04d %02d:%02d:%02d\n",
    	st.st_atime,
	tim->tm_mon+1, tim->tm_mday, tim->tm_year+1900,
	tim->tm_hour, tim->tm_min, tim->tm_sec);
    tim = localtime(&st.st_mtime);
    printf("\tmtime=0x%08lX %02d/%02d/%04d %02d:%02d:%02d\n",
    	st.st_mtime,
	tim->tm_mon+1, tim->tm_mday, tim->tm_year+1900,
	tim->tm_hour, tim->tm_min, tim->tm_sec);
    tim = localtime(&st.st_ctime);
    printf("\tctime=0x%08lX %02d/%02d/%04d %02d:%02d:%02d\n",
    	st.st_ctime,
	tim->tm_mon+1, tim->tm_mday, tim->tm_year+1900,
	tim->tm_hour, tim->tm_min, tim->tm_sec);
    return 0;
}
