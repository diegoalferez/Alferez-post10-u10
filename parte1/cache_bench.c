/*
 * cache_bench.c - Parte 1: Benchmark de la jerarquia de cache
 *
 * Mide la latencia de acceso a memoria con tres patrones:
 *   1. bench_seq    : acceso secuencial (stride de 1 byte)
 *      bench_seq_line: acceso secuencial leyendo 1 byte por linea de cache
 *   2. bench_rand   : acceso aleatorio (indices mezclados con Fisher-Yates)
 *   3. bench_stride : acceso con stride fijo (expone conflictos de asociatividad)
 *
 * Compilar: gcc -O0 -Wall -o cache_bench cache_bench.c
 * Ejecutar: ./cache_bench
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define REPEAT       100   /* repeticiones: secuencial */
#define REPEAT_RAND   10   /* repeticiones: aleatorio (cada acceso es mucho mas lento) */
#define REPEAT_STRIDE 1000 /* repeticiones: stride (pocos pasos por vuelta, hace falta mas tiempo medido) */

#define KB (1024UL)
#define MB (1024UL * 1024UL)

#define LINE_BYTES    64UL    /* tamano de una linea de cache */
#define STRIDE_LINE   LINE_BYTES
#define STRIDE_HALF   2048UL  /* mitad del tamano de pagina (evidencia de la decision 1) */
#define STRIDE_PAGE   4096UL  /* tamano de pagina = 32 KB / 8 vias */

static double elapsed_ns(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
}

/* Acceso secuencial a un array de n_bytes. Devuelve ns por byte. */
double bench_seq(size_t n_bytes)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void *)arr, 1, n_bytes);          /* toca todas las paginas antes de medir */

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < n_bytes; i++)
            (void)arr[i];                     /* lectura (volatile: no se elimina) */
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free((void *)arr);
    return elapsed_ns(t0, t1) / (REPEAT * (double)n_bytes);
}

/* Acceso secuencial leyendo 1 byte por linea de cache (cada 64 bytes).
 * Con lectura byte a byte el costo del bucle (~0.7 ns/byte) oculta el efecto de la
 * cache, porque 64 lecturas comparten una misma linea. Leyendo una vez por linea
 * cada acceso necesita una linea distinta y los saltos entre niveles se ven.
 * Devuelve ns por linea. Las repeticiones se ajustan para mover ~400 MB en total. */
double bench_seq_line(size_t n_bytes)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void *)arr, 1, n_bytes);

    size_t lines = n_bytes / LINE_BYTES;
    long rep = (long)(400UL * MB / n_bytes);
    if (rep < 10) rep = 10;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long r = 0; r < rep; r++)
        for (size_t i = 0; i < n_bytes; i += LINE_BYTES)
            (void)arr[i];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free((void *)arr);
    return elapsed_ns(t0, t1) / (rep * (double)lines);
}

/* Acceso aleatorio con indices pre-mezclados (Fisher-Yates). Devuelve ns por acceso (int). */
double bench_rand(size_t n_bytes)
{
    size_t n = n_bytes / sizeof(int);
    int *arr = (int *)malloc(n * sizeof(int));
    size_t *idx = (size_t *)malloc(n * sizeof(size_t));
    if (!arr || !idx) {
        free(arr);
        free(idx);
        return -1.0;
    }

    for (size_t i = 0; i < n; i++) {
        arr[i] = (int)i;
        idx[i] = i;
    }
    for (size_t i = n - 1; i > 0; i--) {      /* mezcla Fisher-Yates */
        size_t j = (size_t)rand() % (i + 1);
        size_t tmp = idx[i];
        idx[i] = idx[j];
        idx[j] = tmp;
    }

    struct timespec t0, t1;
    volatile long sum = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT_RAND; r++)
        for (size_t i = 0; i < n; i++)
            sum += arr[idx[i]];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free(arr);
    free(idx);
    return elapsed_ns(t0, t1) / (REPEAT_RAND * (double)n);
}

/* Acceso con stride fijo sobre n_bytes. Devuelve ns por acceso. */
double bench_stride(size_t n_bytes, size_t stride)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void *)arr, 1, n_bytes);

    size_t steps = n_bytes / stride;          /* i * stride < n_bytes siempre */

    /* Cada direccion depende del valor leido en el acceso anterior (el byte
     * leido siempre es 1, asi que "& 0" no cambia la direccion). Esa dependencia
     * evita que la CPU solape los accesos y deja a la vista la latencia real. */
    struct timespec t0, t1;
    volatile long sink = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < REPEAT_STRIDE; r++) {
        size_t pos = 0;
        for (size_t i = 0; i < steps; i++) {
            long v = arr[pos];
            sink += v;
            pos = (i + 1) * stride + (size_t)(v & 0);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free((void *)arr);
    return elapsed_ns(t0, t1) / (REPEAT_STRIDE * (double)steps);
}

int main(void)
{
    size_t sizes[] = {
        4 * KB, 8 * KB, 16 * KB, 32 * KB,          /* region L1 */
        64 * KB, 128 * KB, 256 * KB,               /* region L2 */
        512 * KB, 1 * MB, 4 * MB,                  /* region L3 */
        16 * MB, 64 * MB                           /* region RAM */
    };
    int n = (int)(sizeof(sizes) / sizeof(sizes[0]));

    srand(42);                                     /* mezcla reproducible */

    printf("=== Secuencial vs aleatorio (Checkpoints 2 y 3) ===\n");
    printf("%-11s %14s %17s %17s\n", "Array (KB)", "seq (ns/byte)",
           "seq (ns/linea)", "rand (ns/acceso)");
    for (int i = 0; i < n; i++) {
        double seq  = bench_seq(sizes[i]);
        double line = bench_seq_line(sizes[i]);
        double rnd  = bench_rand(sizes[i]);
        printf("%-11zu %14.3f %17.3f %17.3f\n", sizes[i] / KB, seq, line, rnd);
        fflush(stdout);
    }

    printf("\n=== Stride sobre array de 1 MB (Checkpoint 4) ===\n");
    printf("%-12s %14s\n", "Stride", "ns/acceso");
    printf("%-12s %14.3f\n", "64 B",   bench_stride(1 * MB, STRIDE_LINE));
    printf("%-12s %14.3f\n", "2048 B", bench_stride(1 * MB, STRIDE_HALF));
    printf("%-12s %14.3f\n", "4096 B", bench_stride(1 * MB, STRIDE_PAGE));

    return 0;
}
