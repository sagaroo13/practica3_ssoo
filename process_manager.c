/*
 *
 * process_manager.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <pthread.h>
#include "queue.h"
#include <semaphore.h>

#define NUM_THREADS 2

void	set_bool(pthread_mutex_t *mutex, bool *target, bool value)
{
	safe_mutex(mutex, LOCK);
	*target = value;
	safe_mutex(mutex, UNLOCK);
}

bool	get_bool(pthread_mutex_t *mutex, bool *target)
{
	bool value;

	safe_mutex(mutex, LOCK);
	value = *target;
	safe_mutex(mutex, UNLOCK);
	return (value);
}

void	synchro_start(t_factory *factory)
{
	while (!get_bool(&factory->mtx, &factory->ready))
		;
}

void *productor(void *arg)
{
	t_tape *queue;
	element item;
	int i;

	queue = (t_tape *)arg;
    for (i = 0; i < queue->num_elements; i++)
	{
        safe_mutex(&queue->queue_mtx, LOCK);
		while (queue->size == queue->max_size)
			safe_cond(&queue->cond_full, &queue->queue_mtx, WAIT); // espera que se consuma

        queue_put(queue, &item);
        printf(GREEN"[OK]"RESET"[queue] Introduced element with id %d in belt %d.\n", item.num_edition, item.id_belt);

        // Si cola llena o últimos elementos, avisa
		if (queue->size == queue->max_size || i == queue->num_elements - 1)
			safe_cond(&queue->cond_full, &queue->queue_mtx, SIGNAL); // avisa al consumidor si estaba bloqueado

        safe_mutex(&queue->queue_mtx, UNLOCK);
        usleep(5*1e5); // simula trabajo
    }

	safe_mutex(&queue->queue_mtx, LOCK);
	queue->finished = true;
	safe_cond(&queue->cond_full, &queue->queue_mtx, SIGNAL); // avisa al consumidor si estaba bloqueado
	safe_mutex(&queue->queue_mtx, UNLOCK);

    return (NULL);
}

void *consumer(void *arg)
{
	t_tape *queue;
	element *item;

	queue = (t_tape *)arg;
    while (true)
	{
        safe_mutex(&queue->queue_mtx, LOCK);
        while (queue->size < queue->max_size && !queue->finished)
            safe_cond(&queue->cond_full, &queue->queue_mtx, WAIT);

        while (queue->size > 0)
		{
        	item = queue_get(queue);
            printf(GREEN"[OK]"RESET"[queue] Obtained element with id %d in belt %d.\n", item->num_edition, item->id_belt);
        }

        // Salir si ya no habrá más producción
        if (queue->finished && !queue->size)
		{
            safe_mutex(&queue->queue_mtx, UNLOCK);
            break;
        }
        safe_cond(&queue->cond_full, &queue->queue_mtx, SIGNAL); // avisa al productor si estaba bloqueado
		safe_mutex(&queue->queue_mtx, UNLOCK);
        usleep(5*1e5); // simula consumo
    }

    return (NULL);
}

void *process_manager (void *arg)
{
	pthread_t threads[NUM_THREADS];
	t_tape *queue;

	queue = (t_tape *)arg;
	synchro_start(queue->factory);
	printf(GREEN"[OK]"RESET"[process_manager] Process_manager with id %d waiting to produce %d elements.\n", queue->id, queue->num_elements);
	safe_sem(&queue->factory->sems[queue->semaphore_id], 0, WAIT);
	queue_init(queue, queue->max_size);
	printf(GREEN"[OK]"RESET"[process_manager] Belt with id %d has been created with a maximum of %d elements.\n", queue->id, queue->max_size);
	safe_thread(&threads[0], productor, queue, NULL, CREATE);
	safe_thread(&threads[1], consumer, queue, NULL, CREATE);
	safe_thread(&threads[0], NULL, NULL, NULL, JOIN);
	safe_thread(&threads[1], NULL, NULL, NULL, JOIN);
	queue_destroy(queue);
	printf(GREEN"[OK]"RESET"[process_manager] Process_manager with id %d has produced %d elements.\n", queue->id, queue->num_created);
	// printf(GREEN"[OK]"RESET"[factory_manager] Process_manager with id %d has finished.\n", queue->id);
	// if (queue->semaphore_id + 1 < queue->factory->n_tapes)
	// 	safe_sem(&queue->factory->sems[queue->semaphore_id + 1], 0, POST);
   	return (NULL);
}
