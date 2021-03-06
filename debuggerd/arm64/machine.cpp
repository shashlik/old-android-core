/*
 *
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <linux/elf.h>

#include "../utility.h"
#include "../machine.h"

/* enable to dump memory pointed to by every register */
#define DUMP_MEMORY_FOR_ALL_REGISTERS 1

/*
 * If configured to do so, dump memory around *all* registers
 * for the crashing thread.
 */
void dump_memory_and_code(log_t* log, pid_t tid, int scope_flags) {
    struct user_pt_regs regs;
    struct iovec io;
    io.iov_base = &regs;
    io.iov_len = sizeof(regs);

    if (ptrace(PTRACE_GETREGSET, tid, (void*)NT_PRSTATUS, &io) == -1) {
        _LOG(log, scope_flags, "%s: ptrace failed to get registers: %s\n",
             __func__, strerror(errno));
        return;
    }

    if (IS_AT_FAULT(scope_flags) && DUMP_MEMORY_FOR_ALL_REGISTERS) {
        for (int reg = 0; reg < 31; reg++) {
            uintptr_t addr = regs.regs[reg];

            /*
             * Don't bother if it looks like a small int or ~= null, or if
             * it's in the kernel area.
             */
            if (addr < 4096 || addr >= (1UL<<63)) {
                continue;
            }

            _LOG(log, scope_flags | SCOPE_SENSITIVE, "\nmemory near x%d:\n", reg);
            dump_memory(log, tid, addr, scope_flags | SCOPE_SENSITIVE);
        }
    }

    _LOG(log, scope_flags, "\ncode around pc:\n");
    dump_memory(log, tid, (uintptr_t)regs.pc, scope_flags);

    if (regs.pc != regs.sp) {
        _LOG(log, scope_flags, "\ncode around sp:\n");
        dump_memory(log, tid, (uintptr_t)regs.sp, scope_flags);
    }
}

void dump_registers(log_t* log, pid_t tid, int scope_flags)
{
  struct user_pt_regs r;
  struct iovec io;
  io.iov_base = &r;
  io.iov_len = sizeof(r);

  bool only_in_tombstone = !IS_AT_FAULT(scope_flags);

  if (ptrace(PTRACE_GETREGSET, tid, (void*) NT_PRSTATUS, (void*) &io) == -1) {
    _LOG(log, scope_flags, "ptrace error: %s\n", strerror(errno));
    return;
  }

  for (int i = 0; i < 28; i += 4) {
    _LOG(log, scope_flags, "    x%-2d  %016lx  x%-2d  %016lx  x%-2d  %016lx  x%-2d  %016lx\n",
         i, (uint64_t)r.regs[i],
         i+1, (uint64_t)r.regs[i+1],
         i+2, (uint64_t)r.regs[i+2],
         i+3, (uint64_t)r.regs[i+3]);
  }

  _LOG(log, scope_flags, "    x28  %016lx  x29  %016lx  x30  %016lx\n",
       (uint64_t)r.regs[28], (uint64_t)r.regs[29], (uint64_t)r.regs[30]);

  _LOG(log, scope_flags, "    sp   %016lx  pc   %016lx\n",
       (uint64_t)r.sp, (uint64_t)r.pc);


  struct user_fpsimd_state f;
  io.iov_base = &f;
  io.iov_len = sizeof(f);

  if (ptrace(PTRACE_GETREGSET, tid, (void*) NT_PRFPREG, (void*) &io) == -1) {
    _LOG(log, scope_flags, "ptrace error: %s\n", strerror(errno));
    return;
  }

  for (int i = 0; i < 32; i += 4) {
    _LOG(log, scope_flags, "    v%-2d  %016lx  v%-2d  %016lx  v%-2d  %016lx  v%-2d  %016lx\n",
         i, (uint64_t)f.vregs[i],
         i+1, (uint64_t)f.vregs[i+1],
         i+2, (uint64_t)f.vregs[i+2],
         i+3, (uint64_t)f.vregs[i+3]);
  }
}
