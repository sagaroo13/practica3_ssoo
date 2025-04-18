#ifndef HEADER_FILE
#define HEADER_FILE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

# define BLACK "\033[30m"
# define RED "\033[31m"
# define GREEN "\033[32m"
# define YELLOW "\033[33m"
# define BLUE "\033[34m"
# define MAGENTA "\033[35m"
# define CYAN "\033[36m"
# define WHITE "\033[37m"
# define RESET "\033[0m"

typedef struct s_factory t_factory;

typedef struct s_element
{
	int num_edition;
	int id_belt;
	int last;
} t_element;

typedef struct s_tape
{
	int id;
	int semaphore_id;
	int max_size; //capacity
	int size; //size
	int num_elements;
	int num_created;
	int head;
	int tail;
	bool finished;
	pthread_t tape_id;
	pthread_cond_t not_full;
	pthread_cond_t not_empty;
	pthread_mutex_t queue_mtx;
	t_factory *factory;
	t_element *elements;
} t_tape;

typedef struct s_factory
{
	int max_tapes;
	int n_tapes;
	pthread_cond_t ready;
	pthread_mutex_t mtx;
	t_tape *tapes;
	sem_t *sems;
} t_factory;

typedef enum e_operations
{
	CREATE,
	DETACH,
	JOIN,
	INIT,
	DESTROY,
	LOCK,
	UNLOCK,
	WAIT,
	POST,
	SIGNAL,
	BROADCAST,
} t_operations;

int queue_init(t_tape *queue, int capacity);
int queue_destroy(t_tape *queue);
int queue_put(t_tape *queue, t_element *x);
t_element *queue_get(t_tape *queue);
int queue_empty(t_tape *queue);
int queue_full(t_tape *queue);
void *safe_malloc(size_t size, bool calloc_flag);
void safe_sem(sem_t *sem, int value, t_operations operation);
void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, void *retval, t_operations operation);
void safe_mutex(pthread_mutex_t *mutex, t_operations operation);
void safe_cond(pthread_cond_t *cond, pthread_mutex_t *mutex, t_operations operation);
void *process_manager (void *arg);
void set_bool(pthread_mutex_t *mutex, bool *target, bool value);

#endif
