/*
 * payload.c — safe demo payload for fileless execution demos
 *
 * prints its own PID, shows what to inspect in /proc, then sleeps 60s
 * so you have time to verify it left no trace on disk.
 *
 * compiled static: kernel loads PT_LOAD segments from shmem only —
 * no ld.so, no disk read at all during execution.
 */
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = getpid();

    printf("\n");
    printf("  +--------------------------------------------+\n");
    printf("  |  payload running in RAM                    |\n");
    printf("  |  pid  : %-6d                              |\n", pid);
    printf("  |  /proc/%d/exe -> memfd:payload (deleted)   |\n", pid);
    printf("  +--------------------------------------------+\n");
    printf("\n");
    printf("  verify in another terminal:\n");
    printf("    ls -la /proc/%d/exe\n",    pid);
    printf("    cat /proc/%d/maps | head\n", pid);
    printf("    cat /proc/%d/status | grep -E 'Name|PPid'\n", pid);
    printf("\n");
    printf("  sleeping 60s — inspect now...\n");
    fflush(stdout);

    sleep(5000);

    printf("  [done]\n");
    return 0;
}
