/*
 *
 * factory_manager.c
 *
 */

#include "queue.h"

void	free_all(t_factory *factory)
{
	if (factory)
	{
		if (factory->tapes)
			free(factory->tapes);
		if (factory->sems)
		{
			for (int i = 0; i < factory->n_tapes; i++)
				safe_sem(&factory->sems[i], 0, DESTROY);
			free(factory->sems);
		}
		free(factory);
	}
}

void	err_free_exit(t_factory *factory, const char *msg)
{
	free_all(factory);
	if (msg)
		fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void *safe_malloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr)
		err_free_exit(NULL, RED"[ERROR]"RESET" Memory allocation failed.");
	return (ptr);
}

void safe_sem(sem_t *sem, int value, t_operation operation)
{
	int ret;

	if (operation == INIT)
		ret = sem_init(sem, 0, value);
	else if (operation == DESTROY)
		ret = sem_destroy(sem);
	else if (operation == WAIT)
		ret = sem_wait(sem);
	else if (operation == POST)
		ret = sem_post(sem);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Semaphore operation failed.");
}

void safe_thread(pthread_t *thread, void *(*f)(void *), void *arg, t_operation operation)
{
	int ret;

	if (operation == CREATE)
		ret = pthread_create(thread, NULL, f, arg);
	else if (operation == JOIN)
		ret = pthread_join(*thread, NULL);
	else if (operation == DETACH)
		ret = pthread_detach(*thread);
	if (ret)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Thread operation failed.");
}

void safe_close(FILE *fd)
{
	int ret;

	ret = fclose(fd);
	if (ret == EOF)
		err_free_exit(NULL, RED"[ERROR]"RESET"[factory_manager] Invalid file.");
}

t_factory	*parser(const char *filename)
{
    FILE *fd;
    t_tape temp;
    t_factory *factory;
    int	last;

    factory = safe_malloc(sizeof(t_factory));
    if (!(fd = fopen(filename, "r")))
        err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Unable to open file.");
    
    if (fscanf(fd, "%d", &factory->max_tapes) != 1 || factory->max_tapes <= 0)
        err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Invalid max_tapes value.");
    
    factory->tapes = safe_malloc(factory->max_tapes * sizeof(t_tape));
    factory->n_tapes = 0;
    factory->sems = safe_malloc(factory->max_tapes * sizeof(sem_t));

    while (fscanf(fd, "%d %d %d", &temp.id, &temp.tape_size, &temp.num_elements) == 3)
    {
        if (temp.tape_size <= 0 || temp.num_elements <= 0)
            err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Invalid tape data.");
        safe_sem(&factory->sems[factory->n_tapes], factory->n_tapes == 0 ? 1 : 0, INIT);
		temp.semaphore_id = factory->n_tapes;
		temp.factory = factory;
        factory->tapes[factory->n_tapes++] = temp;
        if (factory->n_tapes > factory->max_tapes)
            err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Too many tapes in input file.");
		last = temp.id;
    }

    if (last != temp.id)
        err_free_exit(factory, RED"[ERROR]"RESET"[factory_manager] Extra data in input file.");
    
    safe_close(fd);
    return (factory);
}

void	print_factory(t_factory *factory)
{
	int i;

	printf(GREEN"[INFO]"RESET"[factory_manager] Factory with %d tapes:\n", factory->n_tapes);
	for (i = 0; i < factory->n_tapes; i++)
	{
		printf(BLUE"[INFO]"RESET"[factory_manager] Tape %d: size %d, elements %d\n",
			factory->tapes[i].id, factory->tapes[i].tape_size, factory->tapes[i].num_elements);
	}
}

void	run_factory(t_factory *factory)
{
	int i;

	for (i = 0; i < factory->n_tapes; i++)
		safe_thread(&factory->tapes[i].tape_id, process_manager, &factory->tapes[i], CREATE);
	for (i = 0; i < factory->n_tapes; i++)
		safe_thread(&factory->tapes[i].tape_id, NULL, NULL, JOIN);
}

int main (int argc, const char **argv)
{
	int* status;
	t_factory *factory;

	if (argc != 2)
	{
		fprintf(stderr, RED"[ERROR]"RESET"[factory_manager] Usage: %s <input_file>\n", argv[0]);
		return (-1);
	}
	factory = parser(argv[1]);
	run_factory(factory);
	print_factory(factory);
	free_all(factory);

	return (0);
}
