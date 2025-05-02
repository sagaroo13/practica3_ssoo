#include "queue.h"

void *productor(void *arg)
{
	t_tape *queue;
	t_element item;
	int i;

	queue = (t_tape *)arg;
    for (i = 0; i < queue->num_elements; i++)
	{
        safe_mutex(&queue->queue_mtx, LOCK);
		while (queue->size == queue->max_size)
			safe_cond(&queue->not_full, &queue->queue_mtx, WAIT); // espera que se consuma

        queue_put(queue, &item);
        printf("[OK][queue] Introduced element with id %d in belt %d.\n", item.num_edition, item.id_belt);

		safe_cond(&queue->not_empty, &queue->queue_mtx, SIGNAL); // avisa al consumidor si estaba bloqueado

        safe_mutex(&queue->queue_mtx, UNLOCK);
        // usleep(2*1e5); // simula trabajo
    }

	safe_mutex(&queue->queue_mtx, LOCK);
	queue->finished = true;
	safe_cond(&queue->not_empty, &queue->queue_mtx, SIGNAL); // avisa al consumidor si estaba bloqueado
	safe_mutex(&queue->queue_mtx, UNLOCK);

    return (NULL);
}

void *consumer(void *arg)
{
	t_tape *queue;
	t_element *item;

	queue = (t_tape *)arg;
    while (true)
	{
        safe_mutex(&queue->queue_mtx, LOCK);
        while (queue->size < queue->max_size && !queue->finished)
            safe_cond(&queue->not_empty, &queue->queue_mtx, WAIT);

		// Salir si ya no habrá más producción
        if (queue->finished && !queue->size)
		{
            safe_mutex(&queue->queue_mtx, UNLOCK);
            break;
        }

        item = queue_get(queue);
        printf("[OK][queue] Obtained element with id %d in belt %d.\n", item->num_edition, item->id_belt);

        safe_cond(&queue->not_full, &queue->queue_mtx, SIGNAL); // avisa al productor si estaba bloqueado
		safe_mutex(&queue->queue_mtx, UNLOCK);
		// usleep(2*1e5);
    }

    return (NULL);
}

// Función para sincronizar los procesos en el que se emplea una doble barrera, la primera para esperar a que todos los procesos estén listos
// y la segunda para que todos los procesos estén esperando a que la fábrica esté lista para empezar
// Se emplea una condición y un semáforo para la primera barrera y una doble condición para la segunda barrera (empleando un broadcast activado de desde factory_manager)
static void	synchro(t_tape *queue, bool flag)
{
	safe_mutex(&queue->factory->mtx, LOCK);
	queue->factory->ready_tapes++;
	if (queue->factory->ready_tapes == queue->factory->n_tapes)
    	safe_cond(&queue->factory->ready_threads, NULL, SIGNAL); // Avisa a la fábrica que todos los procesos están listos
	safe_mutex(&queue->factory->mtx, UNLOCK);
	safe_sem(&queue->factory->sem, 0, WAIT); // Espera a que el resto de procesos estén listos

	if (flag)
		printf("[OK][process_manager] Process_manager with id %d waiting to produce %d elements.\n", queue->id, queue->num_elements);

	safe_mutex(&queue->factory->mtx, LOCK);
	queue->factory->waiting_tapes++;
	if (queue->factory->waiting_tapes == queue->factory->n_tapes)
    	safe_cond(&queue->factory->waiting_threads, NULL, SIGNAL); // Avisa a la fábrica que todas las cintas están esperando
	safe_mutex(&queue->factory->mtx, UNLOCK);
	safe_mutex(&queue->factory->mtx, LOCK);
    safe_cond(&queue->factory->broadcast, &queue->factory->mtx, WAIT); // Espera a que la fábrica esté lista para empezar
    safe_mutex(&queue->factory->mtx, UNLOCK);
}

void *process_manager (void *arg)
{
	pthread_t threads[NUM_THREADS];
	t_tape *queue;
	int *status;
	int ret;

	status = safe_malloc(sizeof(int), false);
	*status = 0;

	queue = (t_tape *)arg;
	// Aunque ya hayamos comprobado estas condiciones al hacer el parseo, lo implementamos
	// para verificar que el fallo de un hilo no interrumpe la ejecución del resto
	if (queue->num_elements <= 0 || queue->max_size <= 0)
	{
		*status = -1;
		synchro(queue, false);
		return (fprintf(stderr, "[ERROR][process_manager] Arguments not valid.\n"), status);
	}

	synchro(queue, true);

	if (queue_init(queue, queue->max_size) == -1)
	{
		*status = -1;
		return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
	}
	printf("[OK][process_manager] Belt with id %d has been created with a maximum of %d elements.\n", queue->id, queue->max_size);
	safe_thread(&threads[0], productor, queue, NULL, CREATE);
	safe_thread(&threads[1], consumer, queue, NULL, CREATE);

	if ((ret = pthread_join(threads[0], NULL)))
	{
		*status = -1;
		return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
	}
	if ((ret = pthread_join(threads[1], NULL)))
	{
		*status = -1;
		return (fprintf(stderr, "[ERROR][process_manager] There was an error executing process_manager with id %d\n", queue->id), status);
	}

	queue_destroy(queue);
	printf("[OK][process_manager] Process_manager with id %d has produced %d elements.\n", queue->id, queue->num_created);
	// printf("[OK][factory_manager] Process_manager with id %d has finished.\n", queue->id);
	// if (queue->semaphore_id + 1 < queue->factory->n_tapes)
	// 	safe_sem(&queue->factory->sems[queue->semaphore_id + 1], 0, POST);

   	return (status);
}
