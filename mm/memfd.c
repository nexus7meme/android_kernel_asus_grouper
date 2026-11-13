/*
 * Anonymous shmem file descriptors for legacy Android JIT dual mappings.
 * Based on Linux v3.17 mm/shmem.c, released under the GPL.
 * This backport supports unsealable memfds and CLOEXEC. Mutable sealing is
 * deliberately rejected, so callers can reliably detect its absence.
 */
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/memfd.h>
#include <linux/mm.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#define MFD_NAME_PREFIX "memfd:"
#define MFD_NAME_PREFIX_LEN (sizeof(MFD_NAME_PREFIX) - 1)
#define MFD_NAME_MAX_LEN (NAME_MAX - MFD_NAME_PREFIX_LEN)

SYSCALL_DEFINE2(memfd_create, const char __user *, uname, unsigned int, flags)
{

	struct file *file;
	char *name;
	long len;
	int fd, error;

	if (flags & ~MFD_CLOEXEC)
		return -EINVAL;
	len = strnlen_user(uname, MFD_NAME_MAX_LEN + 1);
	if (!len)
		return -EFAULT;
	if (len > MFD_NAME_MAX_LEN + 1)
		return -EINVAL;
	name = kmalloc(MFD_NAME_PREFIX_LEN + len, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	memcpy(name, MFD_NAME_PREFIX, MFD_NAME_PREFIX_LEN);
	error = -EFAULT;
	if (copy_from_user(name + MFD_NAME_PREFIX_LEN, uname, len) ||
	    name[MFD_NAME_PREFIX_LEN + len - 1])
		goto out_name;
	fd = get_unused_fd_flags((flags & MFD_CLOEXEC) ? O_CLOEXEC : 0);
	if (fd < 0) {
		error = fd;
		goto out_name;
	}
	file = shmem_file_setup(name, 0, VM_NORESERVE);
	if (IS_ERR(file)) {
		error = PTR_ERR(file);
		put_unused_fd(fd);
		goto out_name;
	}
	file->f_mode |= FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE;
	file->f_flags |= O_RDWR | O_LARGEFILE;
	fd_install(fd, file);
	error = fd;
out_name:
	kfree(name);
	return error;
}
