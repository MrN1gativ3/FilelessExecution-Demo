# fileless execution demos — build system
#
# usage:
#   make          build everything
#   make run1     run demo1  (direct exec — this process becomes the payload)
#   make run2     run demo2  (fork + orphan — parent exits, payload lives on)
#   make clean    remove all build artifacts
#
# requires: gcc, python3

CC        = gcc
CFLAGS    = -Wall -Wextra -O2
CFLAGS_PL = -static -Os    # static: no ld.so, zero disk reads on exec

.PHONY: all clean run1 run2

all: demo1 demo2

# ── 1. compile the payload as a static ELF ─────────────────────────────
# static: load_elf_binary() maps PT_LOAD segments from shmem only —
# no PT_INTERP, no ld.so loaded from disk. zero disk reads on exec.
payload: payload.c
	$(CC) $(CFLAGS_PL) -o $@ $<

# ── 2. embed the payload binary as a C byte array header ────────────────
# embed.py replicates xxd -i with static const qualifiers.
# produces: static const unsigned char payload[] = { ... };
#           static const unsigned int  payload_len = N;
payload_bytes.h: payload embed.py
	python3 embed.py $< > $@
	@printf "[*] payload embedded — %d bytes\n" "$$(wc -c < payload)"

# ── 3. build the demos ──────────────────────────────────────────────────
demo1: demo1_direct.c payload_bytes.h
	$(CC) $(CFLAGS) -o $@ $<

demo2: demo2_fork_orphan.c payload_bytes.h
	$(CC) $(CFLAGS) -o $@ $<

run1: demo1
	./demo1

run2: demo2
	./demo2

clean:
	rm -f payload payload_bytes.h demo1 demo2
