/*
 * mymaps.c - Parte 2, Paso 1: mapa de memoria virtual de un proceso
 *
 * Imprime direcciones de distintas regiones y se pausa con getchar()
 * para que /proc/PID/maps pueda leerse mientras el proceso sigue vivo.
 *
 * Compilar: gcc -O0 -Wall -o mymaps mymaps.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_data = 42;           /* segmento .data   */
const char *msg = "hola";       /* puntero en .data, cadena en .rodata */

int main(void)
{
    void *heap = malloc(1024 * 1024);   /* 1 MB en heap */

    printf("PID: %d\n", getpid());
    printf("text addr   (main): %p\n", (void *)main);
    printf("rodata addr (msg) : %p\n", (const void *)msg);
    printf("global addr       : %p\n", (void *)&global_data);
    printf("heap addr         : %p\n", heap);
    printf("stack var addr    : %p\n", (void *)&heap);
    printf("Presione Enter para continuar...\n");
    getchar();

    free(heap);
    return 0;
}
