/*
 * demo1_direct.c — fileless execution, direct
 *
 * three syscalls, zero disk:
 *   memfd_create() → write() → execveat(AT_EMPTY_PATH)
 *
 * the calling process transforms directly into the payload via execveat.
 * same PID, new process image — the loader code is gone after the exec.
 *
 * memory hygiene:
 *   the embedded payload bytes live in .data (not .rodata).
 *   after write() copies them into the shmem page cache, explicit_bzero()
 *   wipes that .data copy immediately — no stale payload bytes remain in
 *   the loader's address space before execveat tears it down.
 *
 * build:   make demo1
 * run:     ./demo1
 *
 * after execveat fires:
 *   /proc/<pid>/exe  →  memfd:payload (deleted)
 *   /proc/<pid>/maps →  anonymous regions only — no file path
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

#include "payload_bytes.h"   /* mutable unsigned char payload[] in .data */

#ifndef SYS_memfd_create
#  define SYS_memfd_create 319
#endif
#ifndef MFD_CLOEXEC
#  define MFD_CLOEXEC 0x0001U
#endif
#ifndef AT_EMPTY_PATH
#  define AT_EMPTY_PATH 0x1000
#endif

int main(void)
{
    printf("[demo1] pid=%d  —  direct fileless execution\n\n", getpid());

    /* ── step 1: memfd_create ──────────────────────────────────────────
     * allocates an anonymous inode on the internal shmem/tmpfs filesystem.
     * no path, no directory entry, no block device backing it.
     * MFD_CLOEXEC: fd auto-closes after execveat so it never leaks
     * into the new process image.
     */
    int fd = (int)syscall(SYS_memfd_create, "payload", MFD_CLOEXEC);
    if (fd < 0) { perror("memfd_create"); return 1; }

    printf("[1] memfd_create  fd=%-3d  /proc/%d/fd/%d = memfd:payload\n",
           fd, getpid(), fd);

    /* ── step 2: write ─────────────────────────────────────────────────
     * copies the payload bytes from .data into the shmem page cache.
     * bytes go: .data buffer → vfs_write() → shmem page cache → RAM only.
     */
    if (write(fd, payload, payload_len) != (ssize_t)payload_len) {
        perror("write"); return 1;
    }

    printf("[2] write         %u bytes → shmem page cache\n", payload_len);

    /* ── wipe ──────────────────────────────────────────────────────────
     * the shmem page cache now holds the only copy we need.
     * zero the .data copy immediately — explicit_bzero() is guaranteed
     * not to be optimised away by the compiler, unlike memset().
     * after this, no payload bytes remain in the loader's address space.
     */
    explicit_bzero(payload, payload_len);

    printf("[3] explicit_bzero  .data copy wiped — payload only in shmem\n");

    /* ── step 3: execveat ──────────────────────────────────────────────
     * AT_EMPTY_PATH: kernel operates on fd directly — no path lookup.
     * do_execveat_common() → do_open_execat() grabs the shmem inode.
     * load_elf_binary():
     *   flush_old_exec()    — tears down this address space (no return)
     *   elf_map()/vm_mmap() — maps PT_LOAD segments from shmem page cache
     *   start_thread()      — rip = ELF entry point
     *
     * after exec: /proc/<pid>/exe = memfd:payload (deleted)
     */
    printf("[4] execveat      replacing process image from fd=%d\n\n", fd);

    char *argv[] = { "payload", NULL };
    syscall(SYS_execveat, fd, "", argv, environ, AT_EMPTY_PATH);

    perror("execveat");
    return 1;
}
