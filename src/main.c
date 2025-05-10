/*
 * Copyright (c) 2025, Frostfuno
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of Frostfuno nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <structures.h>

#ifdef __x86_64__
#include <sys/user.h>     // user_regs_struct for x86_64
#elif defined(__aarch64__)
#include <sys/uio.h>      // For PTRACE_GETREGSET instead
#endif

double timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void printusage(char *name) {
    fprintf(stderr, "Usage: %s [-l logfile] command [args...]\n", name);
}

struct command parse_options(int argc, char **argv) {
    struct command cmd = {
        .command_index = -1,
        .logfile = NULL
    };
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            cmd.logfile = argv[++i]; // skip to logfile argument
        } else {
            cmd.command_index = i;
            break; // first non-option — treat as command
        }
    }
    return cmd;
}

int main(int argc, char **argv) {
    pid_t child;
    int status;
    int first = 1;
    int in_syscall;
    int syscall;
    unsigned int count = 0;

    double start = timestamp();
    double now = timestamp();
    double duration = timestamp() - now;

    struct user_regs_struct regs;
    struct syscall_stats stats;
    struct command cmd;
    
    FILE *writefile = stdout;

    if (argc < 2)
    {
        printusage(argv[0]);
        return 1;
    }

    cmd = parse_options(argc, argv);

    if (cmd.command_index == -1 || cmd.command_index >= argc)
    {
        fprintf(stderr, "Missing command to trace.\n");
        printusage(argv[0]);
        return 1;
    }

    if (cmd.logfile)
    {
        writefile = fopen(cmd.logfile, "a");
        if (!writefile)
        {
            perror("fopen");
            return 1;
        }
    }
    
    if ((child = fork()) == 0) {
        ptrace(PTRACE_TRACEME, 0);
        raise(SIGSTOP);
        int cmd_argc = argc - cmd.command_index;
        char **exec_args = malloc((cmd_argc + 1) * sizeof(char *));
        for (int i = 0; i < cmd_argc; i++)
        {
            exec_args[i] = argv[cmd.command_index + i];
        }
        exec_args[cmd_argc] = NULL;
        
        execvp(exec_args[0], exec_args);

        fprintf(stderr, "execvp");
        return 1;
    } // the usual ptrace forking procedure
    
    fprintf(writefile, "\n==========START==========\n\n");
    while (1)
    {
        ptrace(PTRACE_SYSCALL, child, 0, 0); // trace the child pid for syscall
        waitpid(child, &status, 0);
        if (WIFEXITED(status)) // check if the process exited
        {
            break;
        }

        ptrace(PTRACE_GETREGS, child, NULL, &regs); // get the registers to get the syscall
        syscall = regs.orig_rax; // rax which is the syscall number
        
        if (first)
        {
            first = 0;
            count += 1;
            continue;
        }

        duration = timestamp() - now;
        fprintf(writefile, "%4d | %.9f | %3d : %15s | Duration : %.9f\n",count, timestamp() - start, syscall, syscalls[syscall], duration); // prints the syscall name
        
        if (duration >= stats.max_time)
        {
            stats.longest_call_number = syscall;
            stats.longest_call = count;
            stats.max_time = duration;
        }

        count += 1;
        now = timestamp();
    }
    
    fprintf(writefile, "\n\n========== SUMMARY ==========\n");
    fprintf(writefile, "Calls : %d\n", count);
    fprintf(writefile, "Logest syscall : %d | %s\n", stats.longest_call, syscalls[stats.longest_call_number]);
    fprintf(writefile, "Longest syscall time : %.9f\n", stats.max_time);
    fclose(writefile);

    return 0;
}












