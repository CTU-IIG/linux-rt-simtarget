#define S_FUNCTION_NAME latency_sfnc /* Defines and Includes */
#define S_FUNCTION_LEVEL 2

#define PRIOR_KERN	50
#define PRIOR_HIGH	49
#define PRIOR_LOW	20

#define _GNU_SOURCE
#include "simstruc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>	/*threads*/
#include <time.h>	/*nanosleep*/
#include <semaphore.h>
#include <sched.h>
#include <unistd.h> /* getpid() */

#define TIMES_COUNT 40
#define THREAD_SHARED 0
/* Global Variables */
FILE *f;
int get_time_index;		/*index of the latest got time */
int saved_time_index;	/* index of the latest saved time */
struct timespec times[TIMES_COUNT];
struct timespec now, prev;

sem_t sem;
/*
 * \brief
 * Creates RT thread
 */
int create_rt_task(pthread_t *thread, int prio, void *(*start_routine) (void *), void *arg){
	int ret ;

	pthread_attr_t attr;
	struct sched_param schparam;

	/*inicializace implicitnich atributu*/
	if (pthread_attr_init(&attr) != 0) {
		fprintf(stderr, "pthread_attr_init failed\n");
		return -1;
	}

	/*nastavi dedeni planovace z attr*/
	if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0) {
		fprintf(stderr, "pthread_attr_setinheritsched failed\n");
		return -1;
	}

	/*nastaveni planovace*/
	if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
		fprintf(stderr, "pthread_attr_setschedpolicy SCHED_FIFO failed\n");
		return -1;
	}

	schparam.sched_priority = prio;

	/*nastavit atribut attr podle hodnoty schparam*/
	if (pthread_attr_setschedparam(&attr, &schparam) != 0) {
		fprintf(stderr, "pthread_attr_setschedparam failed\n");
		return -1;
	}

	/*vytvori vlakno*/
	ret = pthread_create(thread, &attr, start_routine, arg);

	/*uvolni strukturu, nema vliv na vlakna jiz vytvorena*/
	pthread_attr_destroy(&attr);

	return ret;
}
static void mdlInitializeSizes(SimStruct *S)
{
    ssSetNumSFcnParams(S, 0);
    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        return; /* Parameter mismatch reported by the Simulink engine*/
    }

    if (!ssSetNumInputPorts(S, 0)) return;

    if (!ssSetNumOutputPorts(S,0)) return;

    ssSetNumSampleTimes(S, 1);

    /* Take care when specifying exception free code - see sfuntmpl.doc */
    ssSetOptions(S, SS_OPTION_EXCEPTION_FREE_CODE);
    }

static void mdlInitializeSampleTimes(SimStruct *S)
{
    ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}

/*
 * \brief
 * Periodically logs timestampt to file.
 */
void * log_loop(void* param){
	pthread_t 			log_thread_id;
	pthread_attr_t 		log_attr;
	struct sched_param 	log_sched_param;
	int 				log_sched_policy;

#ifndef _WIN32
	log_thread_id=pthread_self();
	pthread_getattr_np(log_thread_id, &log_attr);
	
	pthread_attr_getschedparam(&log_attr,&log_sched_param);
	fprintf(f,"#log_priority: %d\n", log_sched_param.sched_priority);
	
	pthread_attr_getschedpolicy(&log_attr,&log_sched_policy);
	switch (log_sched_policy){
		case SCHED_OTHER :
			fprintf(f,"#log_sched_policy: SCHED_OTHER\n");
			break;
		case SCHED_FIFO :
			fprintf(f,"#log_sched_policy: SCHED_FIFO\n");
			break;
		case SCHED_RR :
			fprintf(f,"#log_sched_policy: SCHED_RR\n");
			break;
		default:
			fprintf(f,"#log_sched_policy: unknown\n");
	}
#endif /*_WIN32*/

		while(1){
			sem_wait(&sem);
			fprintf(f, "%u %ld\n", (unsigned)times[saved_time_index].tv_sec, (long)(times[saved_time_index].tv_nsec));
			saved_time_index++;
			saved_time_index%=TIMES_COUNT;
		}
}

/* 
 * \brief
 * Subtract the `struct timespec' values X and Y,
 * storing the result in RESULT (result = x - y).
 *  Return 1 if the difference is negative, otherwise 0.
 */
int
timespec_subtract (struct timespec *result,
           struct timespec *x,
           struct timespec *y)
{
  /* Perform the carry for the later subtraction by updating Y. */
  if (x->tv_nsec < y->tv_nsec) {
    int num_sec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
    y->tv_nsec -= 1000000000 * num_sec;
    y->tv_sec += num_sec;
  }
  if (x->tv_nsec - y->tv_nsec > 1000000000) {
    int num_sec = (x->tv_nsec - y->tv_nsec) / 1000000000;
    y->tv_nsec += 1000000000 * num_sec;
    y->tv_sec -= num_sec;
  }

  /* Compute the time remaining to wait.
     `tv_nsec' is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}
#define MDL_START
void mdlStart(SimStruct *S){
	pthread_t base_thread_id;
	time_t sa,si;
	char name[32];
	FILE *fp;
	char data[128];
	pthread_attr_t this_attr;
	struct sched_param this_sched_param;
	int sched_policy;
	
	
	/* Get Final simulation time */
	si=ssGetTFinal(S);
	/* Get Fundamental Sample time */
	sa=ssGetSampleTime(S,0);
	sprintf(name,"simul_out.dat");
	f = fopen(name, "w");
	if (f == NULL){
		printf("Error opening file!\n");
		exit(1);
	}
	
	
	fprintf(f, "%s", "#LOG_DATA_V1\n");
	fprintf(f,"#this_process_ID: %d\n", getpid());
#ifndef _WIN32
	fprintf(f,"#parents_ID: %d\n", getppid());
#endif /*_WIN32*/
	
	fp = popen("uname -a", "r");
	while (fgets(data,sizeof(data),fp)){
		fprintf(f,"#system: %s",data);
	}
	fp = popen("date", "r");
	while (fgets(data,sizeof(data),fp)){
		fprintf(f,"#date: %s",data);
	}

	pclose(fp);

#ifndef _WIN32
	base_thread_id=pthread_self();
	pthread_getattr_np(base_thread_id, &this_attr);
	
	pthread_attr_getschedparam(&this_attr,&this_sched_param);
	fprintf(f,"#main_fnc_priority: %d\n", this_sched_param.sched_priority);
	
	pthread_attr_getschedpolicy(&this_attr,&sched_policy);
	switch (sched_policy){
		case SCHED_OTHER :
			fprintf(f,"#main_fnc_sched_policy: SCHED_OTHER\n");
			break;
		case SCHED_FIFO :
			fprintf(f,"#main_fnc_sched_policy: SCHED_FIFO\n");
			break;
		case SCHED_RR :
			fprintf(f,"#main_fnc_sched_policy: SCHED_RR\n");
			break;
		default:
			fprintf(f,"#main_fnc_sched_policy: unknown\n");
	}
	
	sem_init(&sem,THREAD_SHARED,0);
#endif /*_WIN32*/
	create_rt_task(&base_thread_id,PRIOR_LOW,log_loop,NULL);
	clock_gettime(CLOCK_MONOTONIC, &prev);
}

static void mdlOutputs(SimStruct *S, int_T tid){
	/*
	pthread_t 			out_thread_id;
	pthread_attr_t 		out_attr;
	struct sched_param 	out_sched_param;
	int 				out_sched_policy;
	
	out_thread_id=pthread_self();
	pthread_getattr_np(out_thread_id, &out_attr);
	
	pthread_attr_getschedparam(&out_attr,&out_sched_param);
	printf("#out_priority: %d\n", out_sched_param.sched_priority);
	
	pthread_attr_getschedpolicy(&out_attr,&out_sched_policy);
	switch (out_sched_policy){
		case SCHED_OTHER :
			printf("#out_sched_policy: SCHED_OTHER\n");
			break;
		case SCHED_FIFO :
			printf("#out_sched_policy: SCHED_FIFO\n");
			break;
		case SCHED_RR :
			printf("#out_sched_policy: SCHED_RR\n");
			break;
		default:
			printf("#main_sched_policy: unknown\n");
	}
	*/
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespec_subtract(&times[get_time_index++],&now,&prev);
	get_time_index%=TIMES_COUNT;
	prev=now;
	sem_post(&sem);
}

static void mdlTerminate(SimStruct *S){
	fclose(f);
}

#ifdef MATLAB_MEX_FILE /* Is this file being compiled as a MEX-file? */
#include "simulink.c" /* MEX-file interface mechanism */
#else
#include "cg_sfun.h" /* Code generation registration function */
#endif
