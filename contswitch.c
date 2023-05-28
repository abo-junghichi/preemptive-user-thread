#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <ucontext.h>
typedef struct {
    /* 0=unused, other=used */
    int used;
    ucontext_t uctx;
    char stack[16384];
} thread_context;
/* 0=switch_thread, 1=suspend */
typedef enum {
    interrupt_ctx = 0, interrupt_spawn, interrupt_swap, interrupt_exit
} interrupt_state_type;
volatile interrupt_state_type interrupt_state;
size_t thread_current;
pid_t self_pid;
struct sigaction old_act;
ucontext_t uctx_init, uctx_exit;
#define THREAD_MAX 3
thread_context threads[THREAD_MAX];
int next_running(size_t *thread_next)
{
    size_t i;
    printf("%i:", thread_current);
    for (i = 0; i < THREAD_MAX; i++)
	printf("%i=%i,", i, threads[i].used);
    for (i = thread_current + 1; i < THREAD_MAX; i++)
	if (threads[i].used)
	    goto found;
    for (i = 0; i < thread_current; i++)
	if (threads[i].used)
	    goto found;
    return 1;
  found:
    *thread_next = i;
    return 0;
}
void sched_sigaction(int signum, siginfo_t *info, void *vague)
{
    size_t next;
    ucontext_t *uctx = vague;
    printf("intr=%i,cur=%i\n", interrupt_state, thread_current);
    switch (interrupt_state) {
    case interrupt_spawn:
	break;
    case interrupt_swap:
	if (THREAD_MAX <= thread_current)
	    break;
	if (next_running(&next))
	    break;
	threads[thread_current].uctx = *uctx;
	goto do_swap;
    case interrupt_exit:
	interrupt_state = interrupt_swap;
	if (THREAD_MAX > thread_current)
	    threads[thread_current].used = 0;
	if (next_running(&next)) {
	    thread_current = THREAD_MAX;
	    break;
	}
	goto do_swap;
      do_swap:
	*uctx = threads[next].uctx;
	thread_current = next;
	break;
    case interrupt_ctx:
	interrupt_state = interrupt_spawn;
	uctx_init = *uctx;
	break;
    }
}
void init_scheduler(void)
{
    struct sigaction act, old_act;
    self_pid = getpid();
    thread_current = THREAD_MAX;
    act.sa_sigaction = sched_sigaction;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &act, &old_act);
    kill(self_pid, SIGALRM);
}
int thread_create(void (*callee)(void *arg), void *arg, size_t arg_size)
{
    int i, rtn = 1;
    char *sp;
    ucontext_t uctx = uctx_init;
    interrupt_state = interrupt_spawn;
    for (i = 0; i < THREAD_MAX; i++)
	if (!threads[i].used)
	    goto spawn;
    goto end;
  spawn:
    sp = threads[i].stack;
    memcpy(sp, arg, arg_size);
    uctx.uc_stack.ss_sp = sp + arg_size;;
    uctx.uc_stack.ss_size = sizeof(threads[i].stack) - arg_size;
    uctx.uc_link = &uctx_exit;
    makecontext(&uctx, callee, 1, sp);
    threads[i].uctx = uctx;
    threads[i].used = 1;
    rtn = 0;
  end:
    interrupt_state = interrupt_swap;
    return rtn;
}
void run_scheduler(void)
{
    struct timeval tt;
    struct itimerval itime, old_itime;
    tt.tv_sec = 1;
    tt.tv_usec = 0;
    itime.it_value = itime.it_interval = tt;
    setitimer(ITIMER_REAL, &itime, &old_itime);
    getcontext(&uctx_exit);
    interrupt_state = interrupt_exit;
    kill(self_pid, SIGALRM);
    setitimer(ITIMER_REAL, &old_itime, NULL);
    sigaction(SIGALRM, &old_act, NULL);
}
void count(void *nump)
{
    int c = 0, num, next;
    memcpy(&num, nump, sizeof(int));
    next = num + 1;
    while (1) {
	int born;
	printf("[%i]input 'a' to end.\n", num);
	c = getchar();
	printf("%i\n", c);
	if ('a' == c)
	    break;
	born = thread_create(count, &next, sizeof(int));
	if (0 == born)
	    printf("born!\n");
    }
}
int main(void)
{
    int num = 0;
    init_scheduler();
    thread_create(count, &num, sizeof(int));
    run_scheduler();
    return 0;
}
