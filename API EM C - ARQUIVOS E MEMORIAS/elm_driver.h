#ifndef ELM_DRIVER_H
#define ELM_DRIVER_H

#include <stdint.h>

/*para enviar os ponteiros como parametro nas funcoes assembly*/
typedef struct {
    volatile uint32_t *data_in;
    volatile uint32_t *ctrl;
    volatile uint32_t *data_out;
} elm_ports_t;

/* Bits data_out — CoProcessor.v assign: { ..., error, busy, done, predicted[3:0] } */
#define ELM_MASK_DONE           (1u << 4)
#define ELM_MASK_BUSY           (1u << 5)
#define ELM_MASK_ERROR          (1u << 6)
/** digito previsto nos 4 lsb valido apos inferencia terminada com done */
#define ELM_MASK_PREDICTED_DIG  (0xFu)

#define ELM_WEIGHT_COUNT       100352u
#define ELM_WEIGHT_RAW_BYTES   (ELM_WEIGHT_COUNT * sizeof(uint16_t))

#define ELM_BIAS_COUNT         128u
#define ELM_BIAS_RAW_BYTES     (ELM_BIAS_COUNT * sizeof(uint16_t))

#define ELM_BETA_COUNT         1280u
#define ELM_BETA_RAW_BYTES     (ELM_BETA_COUNT * sizeof(uint16_t))

/*default para os caminhos dos arquivos a serem usados*/
#ifndef ELM_DEFAULT_RAW_IMG
#define ELM_DEFAULT_RAW_IMG   "imagem_teste.raw"
#endif
#ifndef ELM_DEFAULT_RAW_WIN
#define ELM_DEFAULT_RAW_WIN   "W_in.raw"
#endif
#ifndef ELM_DEFAULT_RAW_BIAS
#define ELM_DEFAULT_RAW_BIAS  "b.raw"
#endif
#ifndef ELM_DEFAULT_RAW_BETA
#define ELM_DEFAULT_RAW_BETA  "beta.raw"
#endif

static inline uint32_t elm_ports_data_out_read(elm_ports_t *ports)
{
    return *ports->data_out;
}

static inline void elm_busy_wait_cleared(elm_ports_t *ports)
{
    while (elm_ports_data_out_read(ports) & ELM_MASK_BUSY)
        ;
}

static inline unsigned elm_ports_predicted_digit(elm_ports_t *ports)
{
    return (unsigned)(elm_ports_data_out_read(ports) & ELM_MASK_PREDICTED_DIG);
}

int elm_store_img_pixel(elm_ports_t *ports, unsigned pixel_gray,
                        unsigned img_index);

int elm_weights_send_addr(elm_ports_t *ports, unsigned addr_index);
int elm_weights_send_value(elm_ports_t *ports, unsigned value_q4_12);
int elm_store_weights_win_raw(elm_ports_t *ports, const char *path_raw);

/** store bias — opc 011 addr [0..127] espera done. */
int elm_store_bias(elm_ports_t *ports, unsigned addr, unsigned value_q4_12_16bit);
int elm_store_bias_raw(elm_ports_t *ports, const char *path_raw);

/** store beta — opc 100 addr [0..1279] tbm espera done */
int elm_store_beta(elm_ports_t *ports, unsigned addr, unsigned value_q4_12_16bit);
int elm_store_beta_raw(elm_ports_t *ports, const char *path_raw);

/** start opc 101 espera done e ler digito via elm_ports_predicted_digit(). */
int elm_inference_start(elm_ports_t *ports);

/**
 * mmap unico — envia img + W_in + bias + beta, START, devolve digito previsto em *out_digit.
 */
int elm_run_maik_classification(const char *path_img_raw,
                                const char *path_win_raw,
                                const char *path_bias_raw,
                                const char *path_beta_raw,
                                unsigned *out_digit);

/** reset */                               
int elm_reset(elm_ports_t *ports); 

/** helpsers */
int elm_upload_img_and_win_raw(const char *path_img_raw,
                               const char *path_win_raw);
int elm_upload_img_win_beta_raw(const char *path_img_raw,
                                const char *path_win_raw,
                                const char *path_beta_raw);
int elm_store_img_from_raw(const char *path_raw);

#endif

