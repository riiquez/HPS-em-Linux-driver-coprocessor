#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "elm_driver.h"

#define LW_BASE       0xFF200000u
#define LW_SPAN       0x00010000u
#define DATA_IN_OFF   0x40u
#define DATA_OUT_OFF  0x50u
#define CTRL_OFF      0x60u

#define NUM_PIX       784
#define ELM_DEFAULT_STABILITY_RUNS 100

static void fill_ports(elm_ports_t *ports, void *mapped)
{
    uintptr_t base = (uintptr_t)mapped;
    ports->data_in = (volatile uint32_t *)(base + DATA_IN_OFF);
    ports->ctrl = (volatile uint32_t *)(base + CTRL_OFF);
    ports->data_out = (volatile uint32_t *)(base + DATA_OUT_OFF);
}

static int mmap_lightweight(elm_ports_t *ports, void **mapped, int *fd_mem)
{
    *mapped = MAP_FAILED;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
        return -3;

    void *m = mmap(NULL, LW_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   (off_t)LW_BASE);

    if (m == MAP_FAILED) {
        close(fd);
        return -4;
    }

    *mapped = m;
    *fd_mem = fd;

    fill_ports(ports, m);

    return 0;
}

static void munmap_lightweight(void *mapped, int fd_mem)
{
    if (mapped && mapped != MAP_FAILED)
        munmap(mapped, LW_SPAN);

    if (fd_mem >= 0)
        close(fd_mem);
}


/* IMG */
/* ========================================================= */

static int send_img784(elm_ports_t *ports, const char *path_img_raw)
{
    uint8_t buf[NUM_PIX];

    int fd_img = open(path_img_raw, O_RDONLY);
    if (fd_img < 0)
        return -1;

    ssize_t n = read(fd_img, buf, NUM_PIX);

    close(fd_img);

    if (n != NUM_PIX)
        return -2;

    elm_reset(ports);

    for (unsigned i = 0; i < NUM_PIX; i++) {

        int err = elm_store_img_pixel(ports, buf[i], i);

        if (err != 0)
            return -10 - (int)i;
    }

    return 0;
}

/* WEIGHTS - pesos*/
/* ========================================================= */

int elm_store_weights_win_raw(elm_ports_t *ports, const char *path_raw)
{
    int fd_w = open(path_raw, O_RDONLY);

    if (fd_w < 0)
        return -20;

    void *blob = malloc(ELM_WEIGHT_RAW_BYTES);

    if (!blob) {
        close(fd_w);
        return -21;
    }

    ssize_t n = read(fd_w, blob, ELM_WEIGHT_RAW_BYTES);

    close(fd_w);

    if ((size_t)n != ELM_WEIGHT_RAW_BYTES) {
        free(blob);
        return -22;
    }

    const uint16_t *wtab = (const uint16_t *)blob;

    for (unsigned addr = 0; addr < ELM_WEIGHT_COUNT; addr++) {

        uint16_t valor = wtab[addr];

        int e = elm_weights_send_addr(ports, addr);

        if (e != 0) {
            free(blob);
            return -30 - (int)addr;
        }

        elm_busy_wait_cleared(ports);

        e = elm_weights_send_value(ports, (unsigned)valor);

        if (e != 0) {
            free(blob);
            return -40 - (int)addr;
        }
    }

    free(blob);

    return 0;
}

/* BIAS */
/* ========================================================= */

int elm_store_bias_raw(elm_ports_t *ports, const char *path_raw)
{
    int fd = open(path_raw, O_RDONLY);

    if (fd < 0)
        return -71;

    void *blob = malloc(ELM_BIAS_RAW_BYTES);

    if (!blob) {
        close(fd);
        return -72;
    }

    ssize_t n = read(fd, blob, ELM_BIAS_RAW_BYTES);

    close(fd);

    if ((size_t)n != ELM_BIAS_RAW_BYTES) {
        free(blob);
        return -73;
    }

    const uint16_t *tab = (const uint16_t *)blob;

    for (unsigned addr = 0; addr < ELM_BIAS_COUNT; addr++) {

        int e = elm_store_bias(ports, addr, (unsigned)tab[addr]);

        if (e != 0) {
            free(blob);
            return -74 - (int)addr;
        }
    }

    free(blob);

    return 0;
}

/* BETA */
/* ========================================================= */

int elm_store_beta_raw(elm_ports_t *ports, const char *path_raw)
{
    int fd_b = open(path_raw, O_RDONLY);

    if (fd_b < 0)
        return -50;

    void *blob = malloc(ELM_BETA_RAW_BYTES);

    if (!blob) {
        close(fd_b);
        return -51;
    }

    ssize_t n = read(fd_b, blob, ELM_BETA_RAW_BYTES);

    close(fd_b);

    if ((size_t)n != ELM_BETA_RAW_BYTES) {
        free(blob);
        return -52;
    }

    const uint16_t *btab = (const uint16_t *)blob;

    for (unsigned addr = 0; addr < ELM_BETA_COUNT; addr++) {

        int e = elm_store_beta(ports, addr, (unsigned)btab[addr]);

        if (e != 0) {
            free(blob);
            return -53 - (int)addr;
        }
    }

    free(blob);

    return 0;
}

/* PIPELINE */
/* ========================================================= */

static int pipeline_upload_all(elm_ports_t *ports,
                               const char *path_img_raw,
                               const char *path_win_raw,
                               const char *path_bias_raw,
                               const char *path_beta_raw)
{
    int rc;

    rc = send_img784(ports, path_img_raw);

    if (rc != 0)
        return rc;

    rc = elm_store_weights_win_raw(ports, path_win_raw);

    if (rc != 0)
        return rc;

    rc = elm_store_bias_raw(ports, path_bias_raw);

    if (rc != 0)
        return rc;

    rc = elm_store_beta_raw(ports, path_beta_raw);

    return rc;
}

/* auxiliares */
/* ========================================================= */

static uint64_t timespec_elapsed_ns(const struct timespec *a,
                                    const struct timespec *b)
{
    int64_t s =
        (int64_t)b->tv_sec - (int64_t)a->tv_sec;

    int64_t ns =
        (int64_t)b->tv_nsec - (int64_t)a->tv_nsec;

    return (uint64_t)(s * 1000000000LL + ns);
}

/* MAIN */
/* ========================================================= */

int main(int argc, char **argv)
{
    const char *path_img =
        (argc >= 2) ? argv[1] : ELM_DEFAULT_RAW_IMG;

    unsigned nruns = ELM_DEFAULT_STABILITY_RUNS;

    if (argc >= 3) {

        errno = 0;

        unsigned long x =
            strtoul(argv[2], NULL, 0);

        if (errno != 0 || x == 0 || x > 500000UL) {

            fprintf(stderr,
                    "uso: %s [imagem.raw [N]]\n",
                    argv[0]);

            return 1;
        }

        nruns = (unsigned)x;
    }

    elm_ports_t ports;

    void *mapped;

    int fd_mem;

    int rc =
        mmap_lightweight(&ports,
                         &mapped,
                         &fd_mem);

    if (rc != 0) {

        fprintf(stderr,
                "mmap falhou %d errno=%s\n",
                rc,
                strerror(errno));

        return 1;
    }

    rc = pipeline_upload_all(&ports,
                             path_img,
                             ELM_DEFAULT_RAW_WIN,
                             ELM_DEFAULT_RAW_BIAS,
                             ELM_DEFAULT_RAW_BETA);

    if (rc != 0) {

        fprintf(stderr,
                "carga inicial falhou código %d\n",
                rc);

        munmap_lightweight(mapped, fd_mem);

        return 1;
    }

    /*
     * Warmup
     */
    rc = elm_inference_start(&ports);

    if (rc != 0) {

        fprintf(stderr,
                "warmup falhou %d\n",
                rc);

        munmap_lightweight(mapped, fd_mem);

        return 1;
    }

    uint64_t latency_sum_ns = 0;

    unsigned first_digit = 0;

    unsigned same_as_first_count = 0;

    printf("\n");
    printf("========================================\n");
    printf("INFERENCIAS E RESULTADOS\n");
    printf("========================================\n");

    for (unsigned i = 0; i < nruns; i++) {

        struct timespec t0, t1;

        if (clock_gettime(CLOCK_MONOTONIC,
                          &t0) != 0) {

            perror("clock_gettime");

            munmap_lightweight(mapped, fd_mem);

            return 1;
        }

        rc = elm_inference_start(&ports);

        if (rc != 0) {

            fprintf(stderr,
                    "inferência #%u falhou código %d\n",
                    i + 1,
                    rc);

            munmap_lightweight(mapped, fd_mem);

            return 1;
        }

        if (clock_gettime(CLOCK_MONOTONIC,
                          &t1) != 0) {

            perror("clock_gettime");

            munmap_lightweight(mapped, fd_mem);

            return 1;
        }

        uint64_t dt_ns =
            timespec_elapsed_ns(&t0, &t1);

        latency_sum_ns += dt_ns;

        /*
         * PEGA O DIGITO PREDITO
         */
        unsigned dig =
            elm_ports_predicted_digit(&ports);

        /*
         * SALVA PRIMEIRO RESULTADO
         */
        if (i == 0)
            first_digit = dig;

        /*
         * CONTA ESTABILIDADE
         */
        if (dig == first_digit)
            same_as_first_count++;

        /*
         * MOSTRA RESULTADO DA INFERENCIA
         */
        printf("Inferencia %3u -> DIGITO: %u | LATENCIA: %.3f ms\n",
               i + 1,
               dig,
               dt_ns / 1e6);
    }

    double mean_ms =
        latency_sum_ns / (double)nruns / 1e6;

    printf("\n");
    printf("========================================\n");
    printf("RESULTADO FINAL\n");
    printf("========================================\n");

    printf("Digito predominante: %u\n",
           first_digit);

    printf("Consistencia: %u/%u (%.2f%%)\n",
           same_as_first_count,
           nruns,
           100.0 *
               (double)same_as_first_count /
               (double)nruns);

    printf("Latencia media: %.4f ms\n",
           mean_ms);

    munmap_lightweight(mapped, fd_mem);

    return (same_as_first_count == nruns)
               ? 0
               : 2;
}
