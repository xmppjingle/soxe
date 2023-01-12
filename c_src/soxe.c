#include <sox.h>
#include <unistd.h>
#include <assert.h>

#include "erl_nif.h"

#define MAX_SAMPLES ((size_t) 4096)
#define MAX_STRING_SIZE 1028
#define BOTH 0
#define BEGINNING 1
#define END 2
#define MAX_PATH_LENGTH 256
#define MAX_PATH_LEN 256
#define MAX_TRIM_END_LEN 5
#define MAX_THRESHOLD_LEN 5
#define MAX_MIN_LENGTH_LEN 5

sox_bool always_true(const char*);

sox_bool always_true(const char* str) {
    return sox_true;
}

static int sox_status = 0;

static ERL_NIF_TERM soxe_start(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    if (!sox_status) {
        sox_init();
        sox_status = 1;
        return enif_make_atom(env, "ok");
    }
    return enif_make_atom(env, "already_started");
}

static ERL_NIF_TERM soxe_stop(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    if (sox_status) {
        sox_quit();
        sox_status = 0;
        return enif_make_atom(env, "ok");
    }
    return enif_make_atom(env, "already_stopped");
}

static ERL_NIF_TERM soxe_convert(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    sox_format_t *in, *out;
    sox_sample_t samples[MAX_SAMPLES];
    char fsrc[MAX_STRING_SIZE], fdst[MAX_STRING_SIZE];

    if (argc != 2) {
        return enif_make_badarg(env);
    }

    if (!sox_status) {
        return enif_make_atom(env, "not_started");
    }

    enif_get_string(env, argv[0], fsrc, MAX_STRING_SIZE, ERL_NIF_LATIN1);
    enif_get_string(env, argv[1], fdst, MAX_STRING_SIZE, ERL_NIF_LATIN1);

    in = sox_open_read(fsrc, NULL, NULL, NULL);
    if (!in) {
        /* FIXME: change this to another kind of error */
        return enif_make_badarg(env);
    }
    out = sox_open_write(fdst, &in->signal, NULL, NULL, NULL, &always_true);
    if (!out) {
        /* FIXME: change this to another kind of error */
        sox_close(in);
        return enif_make_badarg(env);
    }

    while (sox_read(in, samples, MAX_SAMPLES)) {
        if (!sox_write(out, samples, MAX_SAMPLES)) {
            /* FIXME: change this to another kind of error */
            sox_close(in);
            sox_close(out);
            return enif_make_badarg(env);
        }
    }

    sox_close(in);
    sox_close(out);

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM soxe_info(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    char filename[MAX_STRING_SIZE];
    sox_format_t *in;
    int duration = 0;

    if (argc != 1) {
        return enif_make_badarg(env);
    }

    enif_get_string(env, argv[0], filename, MAX_STRING_SIZE, ERL_NIF_LATIN1);
    in = sox_open_read(filename, NULL, NULL, NULL);
    if (!in) {
        /* FIXME: change this to another kind of error */
        return enif_make_badarg(env);
    }

    duration = (in->signal.length / in->signal.channels) / in->signal.rate;

    ERL_NIF_TERM result = enif_make_tuple8(env,
        enif_make_atom(env, "soxe_info"),
        enif_make_string(env, filename, ERL_NIF_LATIN1),
        enif_make_int(env, in->signal.rate),
        enif_make_int(env, in->signal.channels),
        enif_make_string(env, sox_encodings_info[in->encoding.encoding].name,
            ERL_NIF_LATIN1),
        enif_make_int(env, in->encoding.encoding),
        enif_make_int(env, in->signal.length),
        enif_make_int(env, duration)
    );

    sox_close(in);

    return result;
}

/*
Expected parameters: 
input_file_path: The path to the input audio file as a string.
output_file_path: The path to the output audio file as a string.
min_duration: The minimum duration of a segment of silence to be considered for trimming, in seconds, as a floating point number.
threshold: The threshold level below which a signal is considered to be silence, as a floating point number between 0 and 1.
*/
static ERL_NIF_TERM soxe_trim_silence(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    char input_path[MAX_PATH_LEN];
    char output_path[MAX_PATH_LEN];
    char min_duration_str[MAX_THRESHOLD_LEN];
    char threshold_str[MAX_THRESHOLD_LEN];
    static sox_format_t * in, * out; /* input and output files */
    sox_effects_chain_t * chain;
    sox_effect_t * e;
    char * args[1];
    
    // Get input and output paths
    if (!enif_get_string(env, argv[0], input_path, MAX_PATH_LEN, ERL_NIF_LATIN1) ||
        !enif_get_string(env, argv[1], output_path, MAX_PATH_LEN, ERL_NIF_LATIN1)) {
        return enif_make_badarg(env);
    }
    
    if (!enif_get_string(env, argv[2], min_duration_str, MAX_PATH_LEN, ERL_NIF_LATIN1) ||
        !enif_get_string(env, argv[3], threshold_str, MAX_PATH_LEN, ERL_NIF_LATIN1)) {
        return enif_make_badarg(env);
    }

    /* Open the input file (with default parameters) */
  assert(in = sox_open_read(input_path, NULL, NULL, NULL));

  /* Open the output file; we must specify the output signal characteristics.
   * Since we are using only simple effects, they are the same as the input
   * file characteristics */
  assert(out = sox_open_write(output_path, &in->signal, NULL, NULL, NULL, NULL));

  /* Create an effects chain; some effects need to know about the input
   * or output file encoding so we provide that information here */
  chain = sox_create_effects_chain(&in->encoding, &out->encoding);

  /* The first effect in the effect chain must be something that can source
   * samples; in this case, we use the built-in handler that inputs
   * data from an audio file */
  e = sox_create_effect(sox_find_effect("input"));
  args[0] = (char *)in, assert(sox_effect_options(e, 1, args) == SOX_SUCCESS);
  /* This becomes the first `effect' in the chain */
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);
  free(e);

  /* Create the `vol' effect, and initialise it with the desired parameters: */
  e = sox_create_effect(sox_find_effect("silence"));
  char* silence_params[7] = {"-l", "1", min_duration_str, threshold_str, "1", min_duration_str, threshold_str};
  assert(sox_effect_options(e, 4, silence_params) == SOX_SUCCESS);
  /* Add the effect to the end of the effects processing chain: */
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);
  free(e);

  /* The last effect in the effect chain must be something that only consumes
   * samples; in this case, we use the built-in handler that outputs
   * data to an audio file */
  e = sox_create_effect(sox_find_effect("output"));
  args[0] = (char *)out, assert(sox_effect_options(e, 1, args) == SOX_SUCCESS);
  assert(sox_add_effect(chain, e, &in->signal, &in->signal) == SOX_SUCCESS);
  free(e);

  /* Flow samples through the effects processing chain until EOF is reached */
  int flow_result = sox_flow_effects(chain, NULL, NULL);

  /* All done; tidy up: */
  sox_delete_effects_chain(chain);
  sox_close(out);
  sox_close(in);
    
    if (flow_result != SOX_SUCCESS) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_string(env, sox_strerror(flow_result), ERL_NIF_LATIN1));
    }
    
    return enif_make_atom(env, "ok");
}


int upgrade(ErlNifEnv* env, void** priv_data, void** old_priv_data, ERL_NIF_TERM load_info) {
    return 0;
}

static ErlNifFunc nif_funcs[] = {
    {"start", 0, soxe_start},
    {"stop", 0, soxe_stop},
    {"convert", 2, soxe_convert},
    {"info", 1, soxe_info},
    {"trim_silence", 4, soxe_trim_silence}
};

ERL_NIF_INIT(soxe, nif_funcs, NULL, NULL, upgrade, NULL)
