/*
   Big Brother File System
   Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

   This program can be distributed under the terms of the GNU GPLv3.
   See the file COPYING.

   This code is derived from function prototypes found /usr/include/fuse/fuse.h
   Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
   His code is licensed under the LGPLv2.
   A copy of that code is included in the file fuse.h

   The point of this FUSE filesystem is to provide an introduction to
   FUSE.  It was my first FUSE filesystem as I got to know the
   software; hopefully, the comments in this code will help people who
   follow later to get a gentler introduction.

   This might be called a no-op filesystem:  it doesn't impose
   filesystem semantics on top of any other existing structure.  It
   simply reports the requests that come in, and passes them to an
   underlying filesystem.  The information is saved in a logfile named
   bbfs.log, in thebb directory from which you run bbfs.
 */

#include "params.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

//#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
//#endif

#include "log.h"

#include <pthread.h>
#include <semaphore.h>

#define MAX_THREAD_POOL 2
#define MAX_WAIT 100
#define BUFFSIZE 4096
#define UPDATE_DIR "/home/seonjoo/Documents/update.log"

char *SSD_DIR;
char *HDD_DIR;

FILE *update_log;


struct ph
{
	char fpath[PATH_MAX];
	char hddpath[PATH_MAX];
	char *path;
};

struct schedule_info
{
	char current_fpath[PATH_MAX];
	char current_path[PATH_MAX];

	int status[MAX_THREAD_POOL];
	int wait_num;
	int wait_id;
	int current_empty_id;

};

struct update
{
	char path[PATH_MAX];
	char buf[BUFFSIZE];
	int size;
	long long int offset;

};

pthread_cond_t async_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t async_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t alloc2mig_sem[MAX_THREAD_POOL];
sem_t mig2alloc_sem[MAX_THREAD_POOL];
sem_t mig2s_sem;
sem_t s2mig_sem[MAX_THREAD_POOL];

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t emptyid_lock = PTHREAD_MUTEX_INITIALIZER;


pthread_mutex_t waitcnt_mutex = PTHREAD_MUTEX_INITIALIZER;
char *wait_fpath[MAX_WAIT];

pthread_t p_thread;

struct schedule_info s_info;

void hb_update(char *fpath)
{
	

	FILE *fd;
	int update_fd;
	
	struct update myupdate;
	char c;

	char update_path[PATH_MAX];
	int readsize;

	
	fd = fopen(UPDATE_DIR, "r");

	if( fd == NULL ){
		perror("open");
	}

	if( c=fgetc(fd) == EOF )
		printf("EOF\n");
	else{

		rewind(fd);
	
		while(1)
		{
			fscanf(fd, "path=%s size=%d offset=%lld buf=", myupdate.path, &myupdate.size, &myupdate.offset);

			printf("[update info] %s %d %lld\n", myupdate.path, myupdate.size, myupdate.offset);
			
			memset(myupdate.buf, 0, sizeof(myupdate.buf));
			readsize = fread(myupdate.buf, 1, myupdate.size, fd);
			printf("[update read size] %d\n", readsize);

			printf("%s", myupdate.buf);

			//work
			//compare fpath and update path
			//if the same, pwrite to hybrid
			sprintf(update_path, "%s%s", HDD_DIR, myupdate.path);
			printf("\n[update path] %s\n", update_path);
			if( strcmp(fpath, update_path) ){
				update_fd = open(update_path, O_RDWR | O_CREAT);
				pwrite(update_fd, myupdate.buf, strlen(myupdate.buf), (off_t)myupdate.offset);
				memset(myupdate.buf, 0, sizeof(myupdate.buf));
				printf("[update finish]\n");
			}
			c=fgetc(fd);
			c=fgetc(fd);


			printf("[current file pointer] %d\n", ftell(fd));
			if( c == EOF )
				break;
		}
	}

	fclose(fd);

}

void get_xattr(char *fpath, struct stat newstat, struct stat *statbuf)
{
	// extended attribute
	// st_dev
	getxattr(fpath, "user.st_dev", &(newstat.st_dev), sizeof(newstat.st_dev));
	//st_ino
	getxattr(fpath, "user.st_ino", &(newstat.st_ino), sizeof(newstat.st_ino));
	//st_mode
	getxattr(fpath, "user.st_mode", &(newstat.st_mode), sizeof(newstat.st_mode));
	//st_nlink
	getxattr(fpath, "user.st_nlink", &(newstat.st_nlink), sizeof(newstat.st_nlink));
	//st_uid
	getxattr(fpath, "user.st_uid", &(newstat.st_uid), sizeof(newstat.st_uid));
	//st_gid
	getxattr(fpath, "user.st_gid", &(newstat.st_gid), sizeof(newstat.st_gid));
	//st_rdev
	getxattr(fpath, "user.st_rdev", &(newstat.st_rdev), sizeof(newstat.st_rdev));
	//st_size
	getxattr(fpath, "user.st_size", &(newstat.st_size), sizeof(newstat.st_size));
	//st_blksize
	getxattr(fpath, "user.st_blksize", &(newstat.st_blksize), sizeof(newstat.st_blksize));
	//st_blocks
	getxattr(fpath, "user.st_blocks", &(newstat.st_blocks), sizeof(newstat.st_blocks));
	//st_atime
	getxattr(fpath, "user.st_atime", &(newstat.st_atime), sizeof(newstat.st_atime));
	//st_mtime
	getxattr(fpath, "user.st_mtime", &(newstat.st_mtime), sizeof(newstat.st_mtime));
	//st_ctime
	getxattr(fpath, "user.st_ctime", &(newstat.st_ctime), sizeof(newstat.st_ctime));

	statbuf->st_dev = newstat.st_dev;
	statbuf->st_ino = newstat.st_ino;
	statbuf->st_mode = newstat.st_mode;
	statbuf->st_nlink = newstat.st_nlink;
	statbuf->st_uid = newstat.st_uid;
	statbuf->st_gid = newstat.st_gid;
	statbuf->st_rdev = newstat.st_rdev;
	statbuf->st_size = newstat.st_size;
	statbuf->st_blksize = newstat.st_blksize;
	statbuf->st_blocks = newstat.st_blocks;
	statbuf->st_atime = newstat.st_atime;
	statbuf->st_mtime = newstat.st_mtime;
	statbuf->st_ctime = newstat.st_ctime;

}

void set_xattr(char* fpath, struct stat hddstat)
{
	//extended attribute
	//st_dev
	setxattr(fpath, "user.st_dev", &(hddstat.st_dev), sizeof(hddstat.st_dev), 0);
	//st_ino
	setxattr(fpath, "user.st_ino", &(hddstat.st_ino), sizeof(hddstat.st_ino), 0);
	//st_mode
	setxattr(fpath, "user.st_mode", &(hddstat.st_mode), sizeof(hddstat.st_mode), 0);
	//st_nlink
	setxattr(fpath, "user.st_nlink", &(hddstat.st_nlink), sizeof(hddstat.st_nlink), 0);
	//st_uid
	setxattr(fpath, "user.st_uid", &(hddstat.st_uid), sizeof(hddstat.st_uid), 0);
	//st_gid
	setxattr(fpath, "user.st_gid", &(hddstat.st_gid), sizeof(hddstat.st_gid), 0);
	//st_rdev
	setxattr(fpath, "user.st_rdev", &(hddstat.st_rdev), sizeof(hddstat.st_rdev), 0);
	//st_size
	setxattr(fpath, "user.st_size", &(hddstat.st_size), sizeof(hddstat.st_size), 0);
	//st_blksize
	setxattr(fpath, "user.st_blksize", &(hddstat.st_blksize), sizeof(hddstat.st_blksize), 0);	
	//st_blocks
	setxattr(fpath, "user.st_blocks", &(hddstat.st_blocks), sizeof(hddstat.st_blocks), 0);
	//st_atime
	setxattr(fpath, "user.st_atime", &(hddstat.st_atime), sizeof(hddstat.st_atime), 0);
	//st_mtime
	setxattr(fpath, "user.st_mtime", &(hddstat.st_mtime), sizeof(hddstat.st_mtime), 0);
	//st_ctime
	setxattr(fpath, "user.st_ctime", &(hddstat.st_ctime), sizeof(hddstat.st_ctime), 0);
}


void hb_migration(char *fpath, char *path)
{
	struct stat ssdstat;
	struct stat hddstat;

	int fd_r;
	int fd_w;

	int readsize;
	int writesize;
	char buf[1024];
	char wfname[PATH_MAX];

	
	struct stat nnstat;


	fd_r = open(fpath, O_RDONLY);
	if(fd_r == -1){
		printf("[open error] %s\n", fpath);
		perror("open");
	}

	fstat(fd_r, &ssdstat);
	
	strcpy(wfname, HDD_DIR);
	strncat(wfname, path, PATH_MAX);

	
	fd_w = open(wfname, O_CREAT | O_WRONLY | O_TRUNC, ssdstat.st_mode);

	if(fd_w == -1){
		printf("[new open error] %s\n", fpath);
		perror("new open");
	}

	while( (readsize = read(fd_r, buf, 1024)) > 0){
		//sleep(1);
		//printf("[read write]\n");
		writesize=write(fd_w, buf, readsize);
		
		if( writesize != readsize ){
			perror("write != read");
		}

	}

	if(readsize == -1){
		perror("read");
	}

	close(fd_r);

	close(fd_w);

	//remove file
	if( unlink(fpath) == -1 ){
		perror("unlink");
	}	

	//create symbolic link file
	if( symlink(wfname, fpath) == -1 ){
		perror("symlink");
	}
	
}


int thread_allocate(const char *fpath, int emptyid){
	int i;
	int k;
	int current_id;


	printf("[before status for %s] ", fpath);
	for(k=0; k<MAX_THREAD_POOL; k++){
		printf("%d ", s_info.status[k]);
		
	}
	printf("\n");

	if(emptyid != -1){
		current_id = emptyid;
	}
	else{

		for(i=0; i<MAX_THREAD_POOL; i++){
			if( s_info.status[i] == 0 ){
				current_id = i;
				break;
			}
		}

		if( i == MAX_THREAD_POOL ){
			printf("[MAX_THREAD_POOL]\n");
			return -1;
		}

	}

		s_info.status[current_id]=1;
		printf("[after status for %s] ", fpath);
		for(k=0; k<MAX_THREAD_POOL; k++){
			printf("%d ", s_info.status[k]);
			
		}
		printf("\n");

		pthread_mutex_lock(&mutex_lock);
		strcpy( s_info.current_fpath, fpath );
		pthread_mutex_unlock(&mutex_lock);

		printf("[alloc:start migration %d]\n", current_id);
		sem_post(&alloc2mig_sem[current_id]);

		
		sem_wait(&mig2alloc_sem[current_id]);
		printf("[alloc:set path safely from mig %d]\n", current_id);
	
	
	return current_id;
}

void *thread_func(void *data){
	int myfd;
	int mynum = *( (int *)data );
	
	
	struct stat hddstat;
	struct ph myph;


	pthread_mutex_lock(&async_mutex);
	pthread_cond_signal(&async_cond);
	pthread_mutex_unlock(&async_mutex);

	
	while(1)
	{

		//from allocate in release
		printf("[mig %d:wait from alloc]\n", mynum);
		sem_wait(&alloc2mig_sem[mynum]);
		
		pthread_mutex_lock(&mutex_lock);
		strcpy(myph.fpath, s_info.current_fpath);
		pthread_mutex_unlock(&mutex_lock);
		

		printf("[mig %d:notice setting path safely to alloc]\n", mynum);
		sem_post(&mig2alloc_sem[mynum]);
		
		//path
		myph.path = myph.fpath + strlen(SSD_DIR);

		
		
		printf("[migration %d start]%s\n", mynum, myph.fpath);
		
		//copy file
		hb_migration(myph.fpath, myph.path);
		printf("[migration %d finish]%s\n", mynum, myph.fpath);
		
		//->update function
		hb_update(myph.fpath);


		//set xattr : fpath, hddstat
		//hdd path
		strcpy(myph.hddpath, HDD_DIR);
		strncat(myph.hddpath, myph.path, PATH_MAX);
		stat(myph.hddpath, &hddstat);
		set_xattr(myph.fpath, hddstat);

		pthread_mutex_lock(&mutex_lock);
		s_info.current_fpath[0]='\0';
		pthread_mutex_unlock(&mutex_lock);
		s_info.status[mynum]=0;
		printf("[finish work %d]\n", mynum);
		
		if( s_info.wait_num > 0 ){
			//mutex
			pthread_mutex_lock(&emptyid_lock);
			s_info.current_empty_id = mynum;
			pthread_mutex_unlock(&emptyid_lock);

			printf("[mig %d:notice empty]\n", mynum);
			sem_post(&mig2s_sem);

			
			sem_wait(&s2mig_sem[mynum]);
			printf("[mig %d:set id safely from scheduler]\n", mynum);
		}
		
	}

}


void *schedule_thread(void *data){

	int i;
	int j=0;
	//int k;

	int myempty_id;

	while(1){

		printf("[s:empty wait from mig]\n");
		sem_wait(&mig2s_sem);
		//mutex
		pthread_mutex_lock(&emptyid_lock);
		myempty_id = s_info.current_empty_id;
		pthread_mutex_unlock(&emptyid_lock);

		printf("[s:notice setting id safely to mig %d]\n", myempty_id);
		sem_post(&s2mig_sem[myempty_id]);

		printf("[wait num:%d]\n", s_info.wait_num);

		printf("[wait list]\n");
		for(i=0; i<s_info.wait_id; i++){
			printf("[#%d: %s]\n", i, wait_fpath[i]);
		}

		if( wait_fpath[j] != NULL ){
		

			thread_allocate(wait_fpath[j], myempty_id);

			strncat(wait_fpath[j], ":alloc", strlen(":alloc"));
			
		}
		else{
			// ferror(schedulelog);
			printf("[no wait list]\n");
		}

		j++;
		pthread_mutex_lock(&waitcnt_mutex);
		(s_info.wait_num)--;
		pthread_mutex_unlock(&waitcnt_mutex);
		// fprintf(schedulelog, "[schedule end]\n\n");


	}
}



//  All the paths I see are relative to the root of the mounted
//  filesystem.  In order to get to the underlying filesystem, I need to
//  have the mountpoint.  I'll save it away early on in main(), and then
//  whenever I need a path for something I'll call this to construct
//  it.
static void bb_fullpath(char fpath[PATH_MAX], const char *path)
{
	strcpy(fpath, BB_DATA->ssddir);
	strncat(fpath, path, PATH_MAX); // ridiculously long paths will
	// break here

	log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
			BB_DATA->ssddir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int bb_getattr(const char *path, struct stat *statbuf)
{
	int retstat;
	char fpath[PATH_MAX];

	char *buf;
	size_t size;
	off_t offset;
	struct fuse_file_info *fi;

	long long st_size;
	int i;
	struct stat newstat;



	log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x, threshold=%d, ssd=%s, hdd=%s)\n",
			path, statbuf, BB_DATA->threshold, BB_DATA->ssddir, BB_DATA->hdddir);
	bb_fullpath(fpath, path);

	retstat = log_syscall("lstat", lstat(fpath, statbuf), 0);
	log_stat(statbuf);

	//if symbolic link, change statbuf
	
	if( S_ISLNK(statbuf->st_mode) )
	{
		get_xattr(fpath, newstat, statbuf);

	}
	
	log_msg("333333333333333333\n");
	log_stat(statbuf);

	

	return retstat;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int bb_readlink(const char *path, char *link, size_t size)
{
	int retstat;
	char fpath[PATH_MAX];

	log_msg("bb_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
			path, link, size);
	bb_fullpath(fpath, path);

	retstat = log_syscall("fpath", readlink(fpath, link, size - 1), 0);
	if (retstat >= 0) {
		link[retstat] = '\0';
		retstat = 0;
	}

	return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int bb_mknod(const char *path, mode_t mode, dev_t dev)
{
	int retstat;
	char fpath[PATH_MAX];

	log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
			path, mode, dev);
	bb_fullpath(fpath, path);

	// On Linux this could just be 'mknod(path, mode, dev)' but this
	// tries to be be more portable by honoring the quote in the Linux
	// mknod man page stating the only portable use of mknod() is to
	// make a fifo, but saying it should never actually be used for
	// that.
	if (S_ISREG(mode)) {
		retstat = log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
		if (retstat >= 0)
			retstat = log_syscall("close", close(retstat), 0);
	} else
		if (S_ISFIFO(mode))
			retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
		else
			retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);

	return retstat;
}

/** Create a directory */
int bb_mkdir(const char *path, mode_t mode)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n",
			path, mode);
	bb_fullpath(fpath, path);

	return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int bb_unlink(const char *path)
{
	char fpath[PATH_MAX];

	log_msg("bb_unlink(path=\"%s\")\n",
			path);
	bb_fullpath(fpath, path);

	return log_syscall("unlink", unlink(fpath), 0);
}

/** Remove a directory */
int bb_rmdir(const char *path)
{
	char fpath[PATH_MAX];

	log_msg("bb_rmdir(path=\"%s\")\n",
			path);
	bb_fullpath(fpath, path);

	return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int bb_symlink(const char *path, const char *link)
{
	char flink[PATH_MAX];

	log_msg("\nbb_symlink(path=\"%s\", link=\"%s\")\n",
			path, link);
	bb_fullpath(flink, link);

	return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int bb_rename(const char *path, const char *newpath)
{
	char fpath[PATH_MAX];
	char fnewpath[PATH_MAX];

	log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n",
			path, newpath);
	bb_fullpath(fpath, path);
	bb_fullpath(fnewpath, newpath);

	return log_syscall("rename", rename(fpath, fnewpath), 0);
}

/** Create a hard link to a file */
int bb_link(const char *path, const char *newpath)
{
	char fpath[PATH_MAX], fnewpath[PATH_MAX];

	log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n",
			path, newpath);
	bb_fullpath(fpath, path);
	bb_fullpath(fnewpath, newpath);

	return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int bb_chmod(const char *path, mode_t mode)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_chmod(fpath=\"%s\", mode=0%03o)\n",
			path, mode);
	bb_fullpath(fpath, path);

	return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int bb_chown(const char *path, uid_t uid, gid_t gid)

{
	char fpath[PATH_MAX];

	log_msg("\nbb_chown(path=\"%s\", uid=%d, gid=%d)\n",
			path, uid, gid);
	bb_fullpath(fpath, path);

	return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int bb_truncate(const char *path, off_t newsize)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n",
			path, newsize);
	bb_fullpath(fpath, path);

	return log_syscall("truncate", truncate(fpath, newsize), 0);
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int bb_utime(const char *path, struct utimbuf *ubuf)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_utime(path=\"%s\", ubuf=0x%08x)\n",
			path, ubuf);
	bb_fullpath(fpath, path);

	return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int bb_open(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	int fd;
	char fpath[PATH_MAX];

	log_msg("\nbb_open(path\"%s\", fi=0x%08x)\n",
			path, fi);
	bb_fullpath(fpath, path);

	// if the open call succeeds, my retstat is the file descriptor,
	// else it's -errno.  I'm making sure that in that case the saved
	// file descriptor is exactly -1.
	fd = log_syscall("open", open(fpath, fi->flags), 0);
	if (fd < 0)
		retstat = log_error("open");

	fi->fh = fd;

	log_fi(fi);

	return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int bb_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int bb_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;
	int writesize;

	log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);

	printf("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);
	printf("[buffer]\n");
	printf("%s\n", buf);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	fprintf(update_log, "path=%s size=%d offset=%lld buf=%s\n", path, size, offset, buf);

	writesize = pwrite(fi->fh, buf, size, offset);
	printf("write size: %d\n", writesize);

	return log_syscall("pwrite", writesize, 0);
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int bb_statfs(const char *path, struct statvfs *statv)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_msg("\nbb_statfs(path=\"%s\", statv=0x%08x)\n",
			path, statv);
	bb_fullpath(fpath, path);

	// get stats for underlying filesystem
	retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);

	log_statvfs(statv);

	return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
int bb_flush(const char *path, struct fuse_file_info *fi)
{
	log_msg("\nbb_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
	// no need to get fpath on this one, since I work from fi->fh not the path
	log_fi(fi);

	return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int bb_release(const char *path, struct fuse_file_info *fi)
{
	char fpath[PATH_MAX];
	struct stat statbuf;
	int restat;
	
	int i;
	int j;
	int ret;
	int k;
	int flag=0;

	log_msg("\nbb_release(path=\"%s\", fi=0x%08x)\n",
			path, fi);
	log_fi(fi);

	bb_fullpath(fpath, path);

	if( lstat(fpath, &statbuf) == -1 ){
		log_error("release lsat");
	}


	
	
	
	if( !strcmp(s_info.current_fpath, fpath) ){
		printf("[same path migration]\n");
		flag=1;
		
	}

	for(i=0; i<s_info.wait_id; i++){
		if( !strcmp(wait_fpath[i], fpath) ){
			printf("[already wait list]\n");
			flag=1;
		}
	}
	
	if(!flag){
		printf("[release fpath:%s]\n", fpath);
		printf("[release current path:%s]\n", s_info.current_fpath);

		if( S_ISREG(statbuf.st_mode) ){
			if( (statbuf.st_size) >= (BB_DATA->threshold) && strcmp(path, "/") ){
				
				ret = thread_allocate(fpath, -1);
				
				if( ret == -1 ){
					
					if( s_info.wait_id > MAX_WAIT ){
						log_error("max wait");
					}
					
					wait_fpath[s_info.wait_id] = (char*) malloc( sizeof(char*)*strlen(fpath) );
					strcpy( wait_fpath[s_info.wait_id], fpath );
					
					pthread_mutex_lock(&waitcnt_mutex);
					(s_info.wait_num)++;
					pthread_mutex_unlock(&waitcnt_mutex);

					
					(s_info.wait_id)++;
				}
			}
		}

	}



	// We need to close the file.  Had we allocated any resources
	// (buffers etc) we'd need to free them here as well.
	restat = log_syscall("close", close(fi->fh), 0);

	

	return restat;



	
	
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int bb_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
	log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
			path, datasync, fi);
	log_fi(fi);

	// some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
	if (datasync)
		return log_syscall("fdatasync", fdatasync(fi->fh), 0);
	else
#endif	
		return log_syscall("fsync", fsync(fi->fh), 0);
}

//#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int bb_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
			path, name, value, size, flags);
	bb_fullpath(fpath, path);

	return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int bb_getxattr(const char *path, const char *name, char *value, size_t size)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_msg("\nbb_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
			path, name, value, size);
	bb_fullpath(fpath, path);

	retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
	if (retstat >= 0)
		log_msg("    value = \"%s\"\n", value);

	return retstat;
}

/** List extended attributes */
int bb_listxattr(const char *path, char *list, size_t size)
{
	int retstat = 0;
	char fpath[PATH_MAX];
	char *ptr;

	log_msg("bb_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
			path, list, size
	       );
	bb_fullpath(fpath, path);

	retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
	if (retstat >= 0) {
		log_msg("    returned attributes (length %d):\n", retstat);
		for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
			log_msg("    \"%s\"\n", ptr);
	}

	return retstat;
}

/** Remove extended attributes */
int bb_removexattr(const char *path, const char *name)
{
	char fpath[PATH_MAX];

	log_msg("\nbb_removexattr(path=\"%s\", name=\"%s\")\n",
			path, name);
	bb_fullpath(fpath, path);

	return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
}
//#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int bb_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;
	int retstat = 0;
	char fpath[PATH_MAX];

	log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n",
			path, fi);
	bb_fullpath(fpath, path);

	// since opendir returns a pointer, takes some custom handling of
	// return status.
	dp = opendir(fpath);
	log_msg("    opendir returned 0x%p\n", dp);
	if (dp == NULL)
		retstat = log_error("bb_opendir opendir");

	fi->fh = (intptr_t) dp;

	log_fi(fi);

	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int bb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;
	DIR *dp;
	struct dirent *de;

	log_msg("\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
			path, buf, filler, offset, fi);
	// once again, no need for fullpath -- but note that I need to cast fi->fh
	dp = (DIR *) (uintptr_t) fi->fh;

	// Every directory contains at least two entries: . and ..  If my
	// first call to the system readdir() returns NULL I've got an
	// error; near as I can tell, that's the only condition under
	// which I can get an error from readdir()
	de = readdir(dp);
	log_msg("    readdir returned 0x%p\n", de);
	if (de == 0) {
		retstat = log_error("bb_readdir readdir");
		return retstat;
	}

	// This will copy the entire directory into the buffer.  The loop exits
	// when either the system readdir() returns NULL, or filler()
	// returns something non-zero.  The first case just means I've
	// read the whole directory; the second means the buffer is full.
	do {
		log_msg("calling filler with name %s\n", de->d_name);
		if (filler(buf, de->d_name, NULL, 0) != 0) {
			log_msg("    ERROR bb_readdir filler:  buffer full");
			return -ENOMEM;
		}
	} while ((de = readdir(dp)) != NULL);

	log_fi(fi);

	return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int bb_releasedir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n",
			path, fi);
	log_fi(fi);

	closedir((DIR *) (uintptr_t) fi->fh);

	return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int bb_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_msg("\nbb_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
			path, datasync, fi);
	log_fi(fi);

	return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *bb_init(struct fuse_conn_info *conn)
{
	log_msg("\nbb_init()\n");

	//thread
	int i;
	//int pool_size=2;

	char update_path[PATH_MAX];
	

	log_conn(conn);
	log_fuse_context(fuse_get_context());


	
	//mycond = (pthread_cond_t *) malloc( sizeof(pthread_cond_t)*MAX_THREAD_POOL );

	// for(i=0; i<MAX_THREAD_POOL; i++){
	// 	pthread_mutex_init(&mutex_lock[i], NULL);
	// }
	sem_init(&mig2s_sem, 0, 0);
	

	for(i=0; i<MAX_THREAD_POOL; i++){
		sem_init(&alloc2mig_sem[i], 0, 0);
		sem_init(&mig2alloc_sem[i], 0, 0);
		sem_init(&s2mig_sem[i], 0, 0);
	}
	
	for(i=0; i<MAX_THREAD_POOL; i++){


		pthread_mutex_lock(&async_mutex);
		if( pthread_create(&p_thread, NULL, thread_func, (void *)&i) < 0 )
		{
			log_error("create thread");
		}
		
		pthread_cond_wait(&async_cond, &async_mutex);
		
		pthread_mutex_unlock(&async_mutex);

	}

	pthread_create(&p_thread, NULL, schedule_thread, (void *)NULL);


	sprintf(update_path, "/home/seonjoo/Documents/update.log");

	update_log = fopen(update_path, "w");
	if (update_log == NULL) {
		perror("update log");
		//exit(EXIT_FAILURE);
	}

	// set logfile to line buffering
	setvbuf(update_log, NULL, _IONBF, 0); 



	return BB_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void bb_destroy(void *userdata)
{
	int status;
	log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);
	pthread_join(p_thread, (void **)status);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int bb_access(const char *path, int mask)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	log_msg("\nbb_access(path=\"%s\", mask=0%o)\n",
			path, mask);
	bb_fullpath(fpath, path);

	retstat = access(fpath, mask);

	if (retstat < 0)
		retstat = log_error("bb_access access");

	return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int bb_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_msg("\nbb_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
			path, offset, fi);
	log_fi(fi);

	retstat = ftruncate(fi->fh, offset);
	if (retstat < 0)
		retstat = log_error("bb_ftruncate ftruncate");

	return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int bb_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
	int retstat = 0;

	log_msg("\nbb_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
			path, statbuf, fi);
	log_fi(fi);

	// On FreeBSD, trying to do anything with the mountpoint ends up
	// opening it, and then using the FD for an fgetattr.  So in the
	// special case of a path of "/", I need to do a getattr on the
	// underlying root directory instead of doing the fgetattr().
	if (!strcmp(path, "/"))
		return bb_getattr(path, statbuf);

	retstat = fstat(fi->fh, statbuf);
	if (retstat < 0)
		retstat = log_error("bb_fgetattr fstat");

	log_stat(statbuf);

	return retstat;
}

struct fuse_operations bb_oper = {
	.getattr = bb_getattr,
	.readlink = bb_readlink,
	// no .getdir -- that's deprecated
	.getdir = NULL,
	.mknod = bb_mknod,
	.mkdir = bb_mkdir,
	.unlink = bb_unlink,
	.rmdir = bb_rmdir,
	.symlink = bb_symlink,
	.rename = bb_rename,
	.link = bb_link,
	.chmod = bb_chmod,
	.chown = bb_chown,
	.truncate = bb_truncate,
	.utime = bb_utime,
	.open = bb_open,
	.read = bb_read,
	.write = bb_write,
	/** Just a placeholder, don't set */ // huh???
	.statfs = bb_statfs,
	.flush = bb_flush,
	.release = bb_release,
	.fsync = bb_fsync,

//#ifdef HAVE_SYS_XATTR_H
	.setxattr = bb_setxattr,
	.getxattr = bb_getxattr,
	.listxattr = bb_listxattr,
	.removexattr = bb_removexattr,
//#endif

	.opendir = bb_opendir,
	.readdir = bb_readdir,
	.releasedir = bb_releasedir,
	.fsyncdir = bb_fsyncdir,
	.init = bb_init,
	.destroy = bb_destroy,
	.access = bb_access,
	.ftruncate = bb_ftruncate,
	.fgetattr = bb_fgetattr
};

void bb_usage()
{
	fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
	abort();
}

int main(int argc, char *argv[])
{
	int fuse_stat;
	struct bb_state *bb_data;
	
	

	// bbfs doesn't do any access checking on its own (the comment
	// blocks in fuse.h mention some of the functions that need
	// accesses checked -- but note there are other functions, like
	// chown(), that also need checking!).  Since running bbfs as root
	// will therefore open Metrodome-sized holes in the system
	// security, we'll check if root is trying to mount the filesystem
	// and refuse if it is.  The somewhat smaller hole of an ordinary
	// user doing it with the allow_other flag is still there because
	// I don't want to parse the options string.
	if ((getuid() == 0) || (geteuid() == 0)) {
		fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
		return 1;
	}

	// See which version of fuse we're running
	fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);

	// Perform some sanity checking on the command line:  make sure
	// there are enough arguments, and that neither of the last two
	// start with a hyphen (this will break if you actually have a
	// rootpoint or mountpoint whose name starts with a hyphen, but so
	// will a zillion other programs)
	if ((argc < 5))
		bb_usage();

	bb_data = malloc(sizeof(struct bb_state));
	if (bb_data == NULL) {
		perror("main calloc");
		abort();
	}

	// Pull the rootdir out of the argument list and save it in my
	// internal data
	bb_data->threshold = atoi(argv[argc-4]);
	bb_data->ssddir = realpath(argv[argc-3], NULL);
	bb_data->hdddir = realpath(argv[argc-2], NULL);

	SSD_DIR = bb_data->ssddir;
	HDD_DIR = bb_data->hdddir;
	
	
	argv[argc-4] = argv[argc-1];
	argv[argc-3] = NULL;
	//argv[argc-2] = NULL;
	//argv[argc-1] = NULL;
	
	//printf("%s\n", argv[argc-4]);
	//printf("%s\n", argv[argc-5]);
	

	bb_data->logfile = log_open();

	// turn over control to fuse
	fprintf(stderr, "about to call fuse_main\n");
	fuse_stat = fuse_main(argc-3, argv, &bb_oper, bb_data);
	fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
}
