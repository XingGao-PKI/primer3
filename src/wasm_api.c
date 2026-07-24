#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thal.h"
#include "oligotm.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

// Define our static result buffer
#define RESULT_BUF_SIZE 16384
static char result_buf[RESULT_BUF_SIZE];

// Silent initialization state
static int thal_initialized = 0;
static thal_parameters thal_params;
static char custom_config_path[1024] = "/primer3_config";

EMSCRIPTEN_KEEPALIVE
void wasm_set_config_path(const char* path) {
    if (path && strlen(path) > 0 && strlen(path) < sizeof(custom_config_path)) {
        strncpy(custom_config_path, path, sizeof(custom_config_path) - 1);
        custom_config_path[sizeof(custom_config_path) - 1] = '\0';
        // Reset initialization so it reloads parameters from the new path
        thal_initialized = 0;
    }
}

// Helper to JSON-escape string
static void json_escape(const char* src, char* dst, size_t dst_len) {
    if (!src || !dst || dst_len == 0) return;
    size_t i = 0;
    size_t j = 0;
    while (src[i] != '\0' && j < dst_len - 1) {
        if (src[i] == '\\') {
            if (j + 2 < dst_len) {
                dst[j++] = '\\';
                dst[j++] = '\\';
            } else break;
        } else if (src[i] == '"') {
            if (j + 2 < dst_len) {
                dst[j++] = '\\';
                dst[j++] = '"';
            } else break;
        } else if (src[i] == '\n') {
            if (j + 2 < dst_len) {
                dst[j++] = '\\';
                dst[j++] = 'n';
            } else break;
        } else if (src[i] == '\r') {
            if (j + 2 < dst_len) {
                dst[j++] = '\\';
                dst[j++] = 'r';
            } else break;
        } else if (src[i] == '\t') {
            if (j + 2 < dst_len) {
                dst[j++] = '\\';
                dst[j++] = 't';
            } else break;
        } else {
            dst[j++] = src[i];
        }
        i++;
    }
    dst[j] = '\0';
}

static int ensure_thal_initialized(char *err_msg, size_t err_len) {
    if (thal_initialized) {
        return 0;
    }
    
    thal_set_null_parameters(&thal_params);
    thal_results o;
    o.sec_struct = NULL;
    o.msg[0] = '\0';
    
    if (thal_load_parameters(custom_config_path, &thal_params, &o) != 0) {
        snprintf(err_msg, err_len, "Failed to load parameters from %s: %s", custom_config_path, o.msg);
        return -1;
    }
    
    if (get_thermodynamic_values(&thal_params, &o) != 0) {
        snprintf(err_msg, err_len, "Failed to load thermodynamic values: %s", o.msg);
        return -1;
    }
    
    thal_initialized = 1;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_oligotm(const char* seq, 
                        double dna_conc, 
                        double salt_conc, 
                        double divalent_conc, 
                        double dntp_conc, 
                        double dmso_conc, 
                        double dmso_fact, 
                        double formamide_conc, 
                        int nn_max_len,
                        int tm_method, 
                        int salt_corrections, 
                        double annealing_temp) {
    if (!seq || strlen(seq) == 0) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"Empty or null sequence\"}");
        return result_buf;
    }

    tm_ret r = seqtm(seq, dna_conc, salt_conc, divalent_conc, dntp_conc, 
                     dmso_conc, dmso_fact, formamide_conc, nn_max_len,
                     (tm_method_type)tm_method, 
                     (salt_correction_type)salt_corrections, 
                     annealing_temp);
                      
    double dg = oligodg(seq, tm_method);
    int sym = symmetry(seq);

    snprintf(result_buf, RESULT_BUF_SIZE,
             "{\"Tm\": %.4f, \"bound\": %.4f, \"dg\": %.4f, \"symmetry\": %d}",
             r.Tm, r.bound, dg, sym);

    return result_buf;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_hairpin(const char* seq, 
                         double mv, 
                         double dv, 
                         double dntp, 
                         double dna_conc, 
                         double temp) {
    char init_err[512] = {0};
    if (ensure_thal_initialized(init_err, sizeof(init_err)) != 0) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"%s\"}", init_err);
        return result_buf;
    }

    if (!seq || strlen(seq) == 0) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"Empty or null sequence\"}");
        return result_buf;
    }

    thal_args a;
    set_thal_default_args(&a);
    a.type = thal_hairpin;
    a.dimer = 0;
    a.mv = mv;
    a.dv = dv;
    a.dntp = dntp;
    a.dna_conc = dna_conc;
    a.temp = temp + 273.15; // Convert Celsius to Kelvin

    thal_results o;
    o.sec_struct = NULL;
    o.msg[0] = '\0';

    thal((const unsigned char*)seq, (const unsigned char*)seq, &a, THL_STRUCT, &o);

    if (o.temp == THAL_ERROR_SCORE) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"Thermodynamic calculation error: %s\"}", o.msg);
        if (o.sec_struct) free(o.sec_struct);
        return result_buf;
    }

    char escaped_struct[RESULT_BUF_SIZE / 2] = {0};
    if (o.sec_struct) {
        json_escape(o.sec_struct, escaped_struct, sizeof(escaped_struct));
        free(o.sec_struct);
    }

    snprintf(result_buf, RESULT_BUF_SIZE,
             "{\"temp\": %.4f, \"dg\": %.4f, \"dh\": %.4f, \"ds\": %.4f, \"align_end_1\": %d, \"align_end_2\": %d, \"structure\": \"%s\"}",
             o.temp, o.dg, o.dh, o.ds, o.align_end_1, o.align_end_2, escaped_struct);

    return result_buf;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_dimer(const char* seq1, 
                       const char* seq2, 
                       int align_type,
                       double mv, 
                       double dv, 
                       double dntp, 
                       double dna_conc, 
                       double temp) {
    char init_err[512] = {0};
    if (ensure_thal_initialized(init_err, sizeof(init_err)) != 0) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"%s\"}", init_err);
        return result_buf;
    }

    if (!seq1 || strlen(seq1) == 0 || !seq2 || strlen(seq2) == 0) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"Empty or null sequences\"}");
        return result_buf;
    }

    thal_args a;
    set_thal_default_args(&a);
    
    // Set alignment type
    if (align_type == 2) {
        a.type = thal_end1;
    } else if (align_type == 3) {
        a.type = thal_end2;
    } else {
        a.type = thal_any;
    }
    
    a.dimer = 1;
    a.mv = mv;
    a.dv = dv;
    a.dntp = dntp;
    a.dna_conc = dna_conc;
    a.temp = temp + 273.15; // Convert Celsius to Kelvin

    thal_results o;
    o.sec_struct = NULL;
    o.msg[0] = '\0';

    thal((const unsigned char*)seq1, (const unsigned char*)seq2, &a, THL_STRUCT, &o);

    if (o.temp == THAL_ERROR_SCORE) {
        snprintf(result_buf, RESULT_BUF_SIZE, "{\"error\": \"Thermodynamic calculation error: %s\"}", o.msg);
        if (o.sec_struct) free(o.sec_struct);
        return result_buf;
    }

    char escaped_struct[RESULT_BUF_SIZE / 2] = {0};
    if (o.sec_struct) {
        json_escape(o.sec_struct, escaped_struct, sizeof(escaped_struct));
        free(o.sec_struct);
    }

    snprintf(result_buf, RESULT_BUF_SIZE,
             "{\"temp\": %.4f, \"dg\": %.4f, \"dh\": %.4f, \"ds\": %.4f, \"align_end_1\": %d, \"align_end_2\": %d, \"structure\": \"%s\"}",
             o.temp, o.dg, o.dh, o.ds, o.align_end_1, o.align_end_2, escaped_struct);

    return result_buf;
}
