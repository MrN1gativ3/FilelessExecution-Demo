/*
 * demo2_fork_orphan.c — fileless execution, fork + orphan
 *
 * the parent creates the memfd, writes the payload, wipes the .data copy,
 * forks a child, then exits immediately. the child calls execveat and
 * becomes the payload. the child is reparented to init — the loader is
 * completely gone but the payload keeps running.
 *
 * memory hygiene:
 *   payload[] lives in .data (not .rodata — mutable).
 *   after write() copies it into the shmem page cache, explicit_bzero()
 *   wipes the .data copy before fork(). the child inherits the already-
 *   zeroed .data, so neither parent nor child carries payload bytes in
 *   their address space after the wipe. only the shmem page cache copy
 *   remains, which load_elf_binary() maps and the child executes from.
 *
 * build:   make demo2
 * run:     ./demo2
 *
 * inspect while child sleeps:
 *   ls -la /proc/<child_pid>/exe            →  memfd:payload (deleted)
 *   cat /proc/<child_pid>/status | grep PPid →  PPid: 1
 *   cat /proc/<child_pid>/maps | head        →  anonymous only
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>

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
    /* ── step 1: memfd_create ─────────────────────────────────────────
     * parent creates the anonymous fd before forking.
     * fork() does NOT trigger close-on-exec — only exec*() does —
     * so the child inherits the fd intact across fork().
     */
    int fd = (int)syscall(SYS_memfd_create, "payload", MFD_CLOEXEC);
    if (fd < 0) { perror("memfd_create"); return 1; }

    /* ── step 2: write ────────────────────────────────────────────────
     * parent fills the memfd from the .data buffer.
     * shmem page cache now holds the authoritative copy.
     */
    if (write(fd, payload, payload_len) != (ssize_t)payload_len) {
        perror("write"); return 1;
    }

    /* ── wipe ─────────────────────────────────────────────────────────
     * zero the .data copy NOW — before fork() — so neither the child
     * nor the parent carries payload bytes in their address space.
     * the child inherits this already-zeroed page via copy-on-write.
     * explicit_bzero() cannot be optimised away by the compiler.
     */
    explicit_bzero(payload, payload_len);

    printf("[parent] pid=%-6d  %u bytes in shmem · .data copy wiped\n",
           getpid(), payload_len);

    /* ── step 3: fork ─────────────────────────────────────────────────
     * child inherits: open memfd fd, zeroed .data, rest of address space.
     * child does NOT inherit: the payload bytes (already zeroed above).
     */
    pid_t child = fork();
    if (child < 0) { perror("fork"); return 1; }

    /* ════════════════════════════════════════════════════════════════
     * CHILD
     * ════════════════════════════════════════════════════════════════ */
    if (child == 0) {

        /* ── step 4 (child): execveat ─────────────────────────────────
         * AT_EMPTY_PATH: operate on fd, skip pathname resolution.
         * MFD_CLOEXEC closes fd after exec — no fd leak.
         * flush_old_exec() tears down the child's address space
         * (including the zeroed .data) — point of no return.
         * load_elf_binary() maps PT_LOAD from shmem page cache.
         * once parent exits, child is reparented to init (PPid: 1).
         */
        printf("[child]  pid=%-6d  calling execveat\n\n", getpid());
        fflush(stdout);

        char *argv[] = { "payload", NULL };
        syscall(SYS_execveat, fd, "", argv, environ, AT_EMPTY_PATH);

        perror("execveat");
        _exit(1);
    }

    /* ════════════════════════════════════════════════════════════════
     * PARENT — exits without wait() → child orphaned to init
     * ════════════════════════════════════════════════════════════════ */
    printf("[parent] pid=%-6d  exiting — child %d orphaned to init\n\n",
           getpid(), child);
    printf("  inspect the orphaned payload:\n");
    printf("    ls -la /proc/%d/exe\n", child);
    printf("    cat /proc/%d/status | grep PPid\n", child);
    printf("    cat /proc/%d/maps | head\n\n", child);

    close(fd);
    return 0;
}
