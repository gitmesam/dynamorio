/* **********************************************************
 * Copyright (c) 2008-2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * * Neither the name of VMware, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * API regression test that registers for all supported event callbacks
 * (except the nudge and security violation callback)
 */

#ifdef WINDOWS
# include <windows.h>
# include <stdio.h>
#else
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <dlfcn.h>
# include <signal.h>
# include <ucontext.h>
# include <assert.h>
#endif

#ifdef LINUX
/* handler with SA_SIGINFO flag set gets three arguments: */
typedef void (*handler_t)(int, struct siginfo *, void *);

static void
signal_handler(int sig, siginfo_t *siginfo, ucontext_t *ucxt)
{
    if (sig == SIGUSR1)
	fprintf(stderr, "Got SIGUSR1\n");
    else if (sig == SIGUSR2)
	fprintf(stderr, "Got SIGUSR2\n");
    else if (sig == SIGURG)
	fprintf(stderr, "Got SIGURG\n");
}

/* set up signal_handler as the handler for signal "sig" */
static void
intercept_signal(int sig, handler_t handler)
{
    int rc;
    struct sigaction act;
    act.sa_sigaction = handler;
    rc = sigfillset(&act.sa_mask); /* block all signals within handler */
    assert(rc == 0);
    act.sa_flags = SA_SIGINFO | SA_ONSTACK; /* send 3 args to handler */
    /* arm the signal */
    rc = sigaction(sig, &act, NULL);
    assert(rc == 0);
}
#endif /* LINUX */

int
main(int argc, char** argv)
{
#ifdef WINDOWS
    HMODULE hmod;

    /* 
     * Cause an exception event
     */
    __try {
        HANDLE heap = GetProcessHeap();
        char *buf = (char *)HeapAlloc(((char *)heap)+1, HEAP_GENERATE_EXCEPTIONS, 10);
    }
    __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? 
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    }

    /* 
     * Load and unload a module to cause a module unload event
     */
    hmod = LoadLibrary("shell32.dll");
    FreeLibrary(hmod);
#endif

#ifdef LINUX
    void *hmod;
    char buf[1000];
    size_t len = 0;
    char *end_path = NULL;
    /* 
     * Load and unload a module to cause a module unload event
     */
#if 1
    if (argc != 2)
        exit(1);
    strncpy(buf, argv[1], sizeof(buf));
    buf[sizeof(buf)-1] = '\0';
    end_path = strrchr(buf, '/');
    if (end_path == NULL)
        len = 0;
    else
        len = (end_path - buf) + 1;
#else
    getcwd(buf, sizeof(buf));
    buf[sizeof(buf)-1] = '\0';
    len = strlen(buf);
    strncpy(buf+len, "/../client-interface/", sizeof(buf)-len);
    buf[sizeof(buf)-1] = '\0';
    len = strlen(buf);
#endif

      /* small .bss */
#ifdef X64
    strncpy(buf+len, "events64_dummy1.so", sizeof(buf)-len); 
#else
    strncpy(buf+len, "events_dummy1.so", sizeof(buf)-len);
#endif
    buf[sizeof(buf)-1] = '\0';
    hmod = dlopen(buf, RTLD_LAZY|RTLD_LOCAL);
    if (hmod != NULL)
	dlclose(hmod);
    else
	printf("module load of %s failed\n", buf);
    
    /* large .bss */
#ifdef X64
    strncpy(buf+len, "events64_dummy2.so", sizeof(buf)-len); 
#else
    strncpy(buf+len, "events_dummy2.so", sizeof(buf)-len);
#endif
    buf[sizeof(buf)-1] = '\0';
    hmod = dlopen(buf, RTLD_LAZY|RTLD_LOCAL);
    if (hmod != NULL)
	dlclose(hmod);
    else
	printf("module load of %s failed\n", buf);

    /* test load of non-existent file */
    hmod = dlopen("foo_bar_no_exist.so", RTLD_LAZY|RTLD_LOCAL);
    if (hmod != NULL) {
	printf("ERROR - module load of %s succeeded\n", buf);
	dlclose(hmod);
    }

    intercept_signal(SIGUSR1, (handler_t) signal_handler);
    intercept_signal(SIGUSR2, (handler_t) signal_handler);
    intercept_signal(SIGURG, (handler_t) signal_handler);
    fprintf(stderr, "Sending SIGUSR1\n");
    kill(getpid(), SIGUSR1);
    fprintf(stderr, "Sending SIGUSR2\n");
    kill(getpid(), SIGUSR2);
    fprintf(stderr, "Sending SIGURG\n");
    kill(getpid(), SIGURG);
    fprintf(stderr, "Done\n");

    /*
     * Cause a fork event
     */
    if (fork() == 0) {
        wait(NULL);
    }
    else {
        abort();
    }
#endif /* LINUX */

#ifdef WINDOWS
    /* 
     * Cause an exception event, we test redirecting the application to redirect()
     */
    __try {
        HANDLE heap = GetProcessHeap();
        char *buf = (char *)HeapAlloc(((char *)heap)+1, HEAP_GENERATE_EXCEPTIONS, 10);
    }
    __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? 
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    }
    /* Never Reached */
    printf("Shouldn't be reached\n");
#endif
}

#ifdef WINDOWS
__declspec(dllexport) 
void
redirect()
{
    printf("Redirect success!\n");
    exit(0);
}
#endif
