/*
 * pagefault_demo.c - Parte 2, Paso 3: page faults agregados y crecimiento de VmRSS
 *
 * malloc() de 100 MB solo reserva direcciones virtuales (demand paging).
 * El memset fuerza un page fault por cada pagina de 4 KB (~25 600 faults).
 *
 * Compilar: gcc -O0 -Wall -o pagefault_demo pagefault_demo.c
 * Medir:    /usr/bin/time -v ./pagefault_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

/* Imprime VmPeak, VmSize y VmRSS leyendo /proc/self/status del propio proceso.
 * (No se usa system("grep ... /proc/self/status") porque ahi "self" seria grep.) */
static void print_mem(const char *etiqueta)
{
    char line[256];
    struct rusage ru;
    FILE *f = fopen("/proc/self/status", "r");

    printf("--- %s ---\n", etiqueta);
    if (!f) {
        perror("fopen /proc/self/status");
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmPeak", 6) == 0 ||
            strncmp(line, "VmSize", 6) == 0 ||
            strncmp(line, "VmRSS", 5) == 0)
            fputs(line, stdout);
    }
    fclose(f);

    getrusage(RUSAGE_SELF, &ru);
    printf("minor faults acumulados: %ld | major faults acumulados: %ld\n",
           ru.ru_minflt, ru.ru_majflt);
}

int main(void)
{
    size_t N = 100UL * 1024 * 1024;     /* 100 MB */
    char *buf = (char *)malloc(N);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    print_mem("ANTES del memset (reservado, no residente)");

    memset(buf, 0xAA, N);               /* fuerza ~25 600 page faults */

    print_mem("DESPUES del memset (RSS crece ~100 MB)");

    free(buf);
    return 0;
}
