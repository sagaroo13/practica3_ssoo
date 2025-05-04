#include "queue.h"

// Función para imprimir el estado de la cola (comentada)
/*
void print_queue(t_tape *queue)
{
    int i;

    printf(GREEN"[INFO]"RESET"[queue] Queue with id %d has %d elements:\n", queue->id, queue->size);
    for (i = 0; i < queue->max_size; i++)
    {
        if (i == 0 && i == queue->max_size - 1)
            printf("[%d]\n", queue->elements[i].num_edition);
        else if (i == 0)
            printf("[%d,", queue->elements[i].num_edition);
        else if (i == queue->max_size - 1)
            printf("%d]\n", queue->elements[i].num_edition);
        else
            printf("%d,", queue->elements[i].num_edition);
    }
}
*/

// Inicializar la cola circular
int queue_init(t_tape *queue, int capacity)
{
    // Asignar memoria para los elementos de la cola
    queue->elements = calloc(capacity, sizeof(t_element));
    if (!queue->elements) // Verificar si la asignación de memoria falló
    {
        fprintf(stderr, "[ERROR][queue] There was an error while using queue with id: %d\n", queue->id);
        return (-1); // Retornar error
    }
    queue->head = 0; // Inicializar el índice de la cabeza
    queue->tail = -1; // Inicializar el índice de la cola
    queue->size = 0; // Inicializar el tamaño de la cola
    return (0); // Éxito
}

// Insertar un elemento en la cola
int queue_put(t_tape *queue, t_element *x)
{
    if (queue_full(queue)) // Verificar si la cola está llena
        return (-1); // Retornar error si la cola está llena

    queue->size++; // Incrementar el tamaño de la cola
    x->id_belt = queue->id; // Asignar el id de la cinta al elemento
    x->num_edition = queue->num_created; // Asignar el número de edición al elemento
    queue->num_created++; // Incrementar el contador de elementos creados

    // Marcar el elemento como el último si es el último que se debe producir
    if (queue->num_created == queue->num_elements)
        x->last = true;
    else
        x->last = false;

    // Avanzar el índice de la cola de forma circular
    queue->tail = (queue->tail + 1) % queue->max_size;
    queue->elements[queue->tail] = *x; // Copiar el elemento en la cola
    // print_queue(queue); // (Comentado) Imprimir el estado de la cola
    return (0); // Éxito
}

// Eliminar un elemento de la cola
t_element *queue_get(t_tape *queue)
{
    t_element *item;

    if (queue_empty(queue)) // Verificar si la cola está vacía
        return (NULL); // Retornar NULL si no hay elementos

    item = &queue->elements[queue->head]; // Obtener el elemento en la cabeza de la cola
    // Avanzar el índice de la cabeza de forma circular
    queue->head = (queue->head + 1) % queue->max_size;
    queue->size--; // Decrementar el tamaño de la cola
    // print_queue(queue); // (Comentado) Imprimir el estado de la cola
    return (item); // Retornar el elemento eliminado
}

// Verificar si la cola está vacía
int inline queue_empty(t_tape *queue)
{
    return (queue->size == 0); // Retorna verdadero si el tamaño de la cola es 0
}

// Verificar si la cola está llena
int inline queue_full(t_tape *queue)
{
    return (queue->size == queue->max_size); // Retorna verdadero si el tamaño de la cola es igual a su capacidad máxima
}

// Destruir la cola y liberar los recursos
int queue_destroy(t_tape *queue)
{
    free(queue->elements); // Liberar la memoria asignada para los elementos
    queue->elements = NULL; // Establecer el puntero a NULL
    queue->head = 0; // Reiniciar el índice de la cabeza
    queue->tail = -1; // Reiniciar el índice de la cola
    queue->size = 0; // Reiniciar el tamaño de la cola
    return (0); // Éxito
}
