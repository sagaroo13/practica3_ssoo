#include <stdio.h>
#include <stdlib.h>
#include "queue.h"


// Inicializar la cola circular
int queue_init(t_tape *queue, int capacity)
{
    queue->elements = safe_malloc(sizeof(struct element) * capacity); // Asignar memoria para los elementos
    queue->head = 0;
    queue->tail = -1;
	queue->size = 0;
    return (0); // Éxito
}

// Insertar un elemento en la cola
int queue_put(t_tape *queue, element *x)
{
    if (queue_full(queue))
        return (-1); // Cola llena
    queue->size++;
    x->id_belt = queue->id; // Asignar el id de la cinta
    x->num_edition = queue->num_created; // Asignar el número de edición
    queue->num_created++;
    if (queue->num_created == queue->num_elements)
        x->last = true;
    else
        x->last = false;
    queue->tail = (queue->tail + 1) % queue->max_size; // Avanzar el índice de rear
    queue->elements[queue->tail] = *x; // Copiar el elemento
    return (0); // Éxito
}

// Eliminar un elemento de la cola
element *queue_get(t_tape *queue)
{
	element *item;
    if (queue_empty(queue))
        return (NULL);
    item = &queue->elements[queue->head]; // Obtener el elemento
    queue->head = (queue->head + 1) % queue->max_size; // Avanzar el índice de front
    queue->size--;
    return (item);
}

// Verificar si la cola está vacía
int inline queue_empty(t_tape *queue)
{
    return (queue->size == 0);
}

// Verificar si la cola está llena
int inline queue_full(t_tape *queue)
{
    return (queue->size == queue->max_size);
}

// Destruir la cola y liberar los recursos
int queue_destroy(t_tape *queue)
{
    free(queue->elements);
    queue->elements = NULL;
    queue->max_size = 0;
    queue->head = 0;
    queue->tail = -1;
    queue->size = 0;
    return (0);
}
