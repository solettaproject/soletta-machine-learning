/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <float.h>
#include <math.h>
#include <sol-vector.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sol-worker-thread.h>

#include "sml_ann.h"
#include "sml_fuzzy.h"
#include "machine_learning_sml_data.h"

#include "machine_learning_gen.h"

/* TODO should we have SML_ACTIVATION_FUNCTION_LAST? */
#define MAX_FUNCTIONS (SML_ANN_ACTIVATION_FUNCTION_SIN_SYMMETRIC + 1)
#define AUTOMATIC_TERMS (15)

struct tagged_float_data {
    struct sol_drange value;
    char *tag;
};

static int
mutex_lock(pthread_mutex_t *lock)
{
    int error = pthread_mutex_lock(lock);

    if (error)
        SOL_WRN("Impossible to lock mutex. Error code: %d\n", error);
    return -error;
}

static void
tagged_float_packet_dispose(const struct sol_flow_packet_type *packet_type,
    void *mem)
{
    struct tagged_float_data *tagged_float = mem;

    free(tagged_float->tag);
}

static int
tagged_float_packet_init(const struct sol_flow_packet_type *packet_type,
    void *mem, const void *input)
{
    const struct tagged_float_data *in = input;
    struct tagged_float_data *tagged_float = mem;

    tagged_float->tag = strdup(in->tag);
    SOL_NULL_CHECK(tagged_float->tag, -ENOMEM);
    tagged_float->value = in->value;

    return 0;
}

#define TAGGED_FLOAT_PACKET_TYPE_API_VERSION (1)

static const struct sol_flow_packet_type _PACKET_TYPE_TAGGED_FLOAT = {
    .api_version = TAGGED_FLOAT_PACKET_TYPE_API_VERSION,
    .name = "TaggedFloat",
    .data_size = sizeof(struct tagged_float_data),
    .init = tagged_float_packet_init,
    .dispose = tagged_float_packet_dispose,
};
static const struct sol_flow_packet_type *PACKET_TYPE_TAGGED_FLOAT =
    &_PACKET_TYPE_TAGGED_FLOAT;

#undef TAGGED_FLOAT_PACKET_TYPE_API_VERSION

static struct sol_flow_packet *
packet_new_tagged_float(struct sol_drange *value, const char *tag)
{
    struct tagged_float_data tagged_float;

    SOL_NULL_CHECK(value, NULL);
    SOL_NULL_CHECK(tag, NULL);

    tagged_float.value = *value;
    tagged_float.tag = (char *)tag;

    return sol_flow_packet_new(PACKET_TYPE_TAGGED_FLOAT, &tagged_float);
}

static int
packet_get_tagged_float(const struct sol_flow_packet *packet,
    struct sol_drange *value, char **tag)
{
    struct tagged_float_data tagged_float;
    int ret;

    SOL_NULL_CHECK(packet, -EBADR);
    if (sol_flow_packet_get_type(packet) != PACKET_TYPE_TAGGED_FLOAT)
        return -EINVAL;

    ret = sol_flow_packet_get(packet, &tagged_float);
    SOL_INT_CHECK(ret, != 0, ret);

    if (tag)
        *tag = tagged_float.tag;
    if (value)
        *value = tagged_float.value;

    return ret;
}

static int
send_tagged_float_packet(struct sol_flow_node *src, uint16_t src_port,
    struct sol_drange *value, const char *tag)
{
    struct sol_flow_packet *packet;

    packet = packet_new_tagged_float(value, tag);
    SOL_NULL_CHECK(packet, -ENOMEM);

    return sol_flow_send_packet(src, src_port, packet);
}

struct tagger_data {
    char *tag;
};

static int
tagger_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    struct tagger_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_tagger_options *opts;

    opts = (const struct
        sol_flow_node_type_machine_learning_tagger_options *)options;

    if (!opts->tag) {
        SOL_WRN("Valid tag is required");
        return -EINVAL;
    }

    mdata->tag = strdup(opts->tag);
    SOL_NULL_CHECK(mdata->tag, -ENOMEM);

    return 0;
}

static void
tagger_close(struct sol_flow_node *node, void *data)
{
    struct tagger_data *mdata = data;

    free(mdata->tag);
}

static int
tagger_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct tagger_data *mdata = data;
    int r;
    struct sol_drange in_value;

    r = sol_flow_packet_get_drange(packet, &in_value);
    SOL_INT_CHECK(r, < 0, r);

    return send_tagged_float_packet(node,
        SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_TAGGER__OUT__OUT,
        &in_value, mdata->tag);
}

static int
filter_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct tagger_data *mdata = data;
    int r;
    struct sol_drange value;
    char *tag;

    r = packet_get_tagged_float(packet, &value, &tag);
    SOL_INT_CHECK(r, < 0, r);

    if (strcmp(tag, mdata->tag))
        return 0;

    return sol_flow_send_drange_packet(node,
        SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FILTER__OUT__OUT,
        &value);
}

struct machine_learning_data {
    //Used only in open method or/and worker thread. No need to lock
    struct sml_object *sml;
    uint16_t number_of_terms;
    bool run_process;
    char *sml_data_dir;

    //Used by main thread and process thread. Need to be locked
    struct sol_vector input_vec;
    struct sol_vector input_id_vec;
    struct sol_vector output_vec;
    struct sol_vector output_id_vec;
    bool process_needed, predict_needed, send_process_finished, save_needed;
    char *debug_file;

    struct sol_flow_node *node;
    struct sol_worker_thread *worker;
    pthread_mutex_t process_lock;
    pthread_mutex_t read_lock;
};

struct machine_learning_var {
    struct sml_variable *sml_variable;
    struct sol_drange value;
    bool range_changed;
};

struct machine_learning_input_var {
    struct machine_learning_var base;
};

struct machine_learning_output_var {
    struct machine_learning_var base;
    double predicted_value;
    char *tag;
};

static void
set_variable(struct machine_learning_data *mdata,
    struct machine_learning_var *var)
{
    float width;

    sml_variable_set_value(mdata->sml, var->sml_variable, var->value.val);

    if (!var->range_changed)
        return;

    var->range_changed = false;
    if (sml_is_fuzzy(mdata->sml)) {
        width = fmax((var->value.max - var->value.min + 1) /
            mdata->number_of_terms, var->value.step);

        sml_fuzzy_variable_set_default_term_width(mdata->sml, var->sml_variable,
            width);
    }

    sml_variable_set_range(mdata->sml, var->sml_variable, var->value.min,
        var->value.max);
}

static bool
read_state_cb(struct sml_object *sml, void *data)
{
    struct machine_learning_data *mdata = data;
    struct machine_learning_input_var *input_var;
    struct machine_learning_output_var *output_var;
    int i;

    if (mutex_lock(&mdata->read_lock))
        return false;

    SOL_VECTOR_FOREACH_IDX (&mdata->input_vec, input_var, i)
        set_variable(mdata, &input_var->base);
    SOL_VECTOR_FOREACH_IDX (&mdata->input_id_vec, input_var, i)
        set_variable(mdata, &input_var->base);
    SOL_VECTOR_FOREACH_IDX (&mdata->output_vec, output_var, i)
        set_variable(mdata, &output_var->base);
    SOL_VECTOR_FOREACH_IDX (&mdata->output_id_vec, output_var, i)
        set_variable(mdata, &output_var->base);

    pthread_mutex_unlock(&mdata->read_lock);
    return true;
}

static void
_process_state_changed_output(struct sml_object *sml,
    struct sml_variables_list *changed,
    struct machine_learning_output_var *output_var)
{
    double value;

    if (changed && !sml_variables_list_contains(sml, changed,
        output_var->base.sml_variable))
        return;
    value = sml_variable_get_value(sml, output_var->base.sml_variable);
    if (isnan(value))
        return;

    output_var->predicted_value = value;
}

static void
output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed,
    void *data)
{
    struct machine_learning_data *mdata = data;
    struct machine_learning_output_var *output_var;
    int i;

    if (mutex_lock(&mdata->read_lock))
        return;

    SOL_VECTOR_FOREACH_IDX (&mdata->output_vec, output_var, i)
        _process_state_changed_output(sml, changed, output_var);

    SOL_VECTOR_FOREACH_IDX (&mdata->output_id_vec, output_var, i)
        _process_state_changed_output(sml, changed, output_var);

    pthread_mutex_unlock(&mdata->read_lock);
}

static int
create_sml_fuzzy(struct sml_object **sml, int32_t stabilization_hits)
{
    struct sml_object *sml_fuzzy;

    sml_fuzzy = sml_fuzzy_new();
    SOL_NULL_CHECK(sml_fuzzy, -ENOMEM);

    if (stabilization_hits < 0) {
        SOL_WRN("Stabilization hits (%d) must be a positive value. Assuming 0.",
            stabilization_hits);
        stabilization_hits = 0;
    }

    if (!sml_set_stabilization_hits(sml_fuzzy, stabilization_hits)) {
        SOL_WRN("Failed to set stabilization hits");
        sml_free(sml_fuzzy);
        return -EINVAL;
    }

    *sml = sml_fuzzy;
    return 0;
}

static int
init_machine_learning(struct machine_learning_data *mdata)
{
    int r;

    if (!sml_set_read_state_callback(mdata->sml, read_state_cb, mdata)) {
        SOL_WRN("Failed to set read callback");
        return -EINVAL;
    }

    if (!sml_set_output_state_changed_callback(mdata->sml,
        output_state_changed_cb, mdata)) {
        SOL_WRN("Failed to set change state callback");
        return -EINVAL;
    }

    sol_vector_init(&mdata->input_vec,
        sizeof(struct machine_learning_input_var));
    sol_vector_init(&mdata->input_id_vec,
        sizeof(struct machine_learning_input_var));
    sol_vector_init(&mdata->output_vec,
        sizeof(struct machine_learning_output_var));
    sol_vector_init(&mdata->output_id_vec,
        sizeof(struct machine_learning_output_var));

    if ((r = pthread_mutex_init(&mdata->process_lock, NULL)) ||
        (r = pthread_mutex_init(&mdata->read_lock, NULL))) {
        SOL_WRN("Failed to initialize pthread mutex lock");
        return -r;
    }
    return 0;
}

static int
copy_str(char **dst, const char *src)
{
    *dst = strdup(src);
    SOL_NULL_CHECK(*dst, -ENOMEM);
    return 0;
}

static int
machine_learning_fuzzy_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    struct machine_learning_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_fuzzy_options *opts;
    int r;

    opts = (const struct
        sol_flow_node_type_machine_learning_fuzzy_options *)options;

    mdata->node = node;

    if (opts->number_of_terms.val >= 0)
        mdata->number_of_terms = opts->number_of_terms.val;
    else {
        SOL_WRN("Number of fuzzy terms (%d) must be a positive value. "
            "Assuming %d.", opts->number_of_terms.val, AUTOMATIC_TERMS);
        mdata->number_of_terms = AUTOMATIC_TERMS;
    }

    r = create_sml_fuzzy(&mdata->sml, opts->stabilization_hits.val);
    SOL_INT_CHECK(r, < 0, r);

    r = init_machine_learning(mdata);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!opts->data_dir)
        return 0;

    r = copy_str(&mdata->sml_data_dir, opts->data_dir);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!sml_load(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Could not load the sml data at:%s", mdata->sml_data_dir);

    return 0;

err:
    sml_free(mdata->sml);
    return r;
}

static const struct activation_function {
    enum sml_ann_activation_function function;
    const char *name;
} activation_functions[] = {
    { SML_ANN_ACTIVATION_FUNCTION_SIGMOID, "sigmoid" },
    { SML_ANN_ACTIVATION_FUNCTION_SIGMOID_SYMMETRIC, "sigmoid_symmetric" },
    { SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN, "gaussian" },
    { SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN_SYMMETRIC, "gaussian_symmetric" },
    { SML_ANN_ACTIVATION_FUNCTION_ELLIOT, "elliot" },
    { SML_ANN_ACTIVATION_FUNCTION_ELLIOT_SYMMETRIC, "elliot_symmetric" },
    { SML_ANN_ACTIVATION_FUNCTION_COS, "cos" },
    { SML_ANN_ACTIVATION_FUNCTION_COS_SYMMETRIC, "cos_symmetric" },
    { SML_ANN_ACTIVATION_FUNCTION_SIN, "sin" },
    { SML_ANN_ACTIVATION_FUNCTION_SIN_SYMMETRIC, "sin_symmetric" },
    { 0, NULL }
};

static void
use_default_functions(enum sml_ann_activation_function *functions,
    unsigned int *size)
{
    const struct activation_function *cur = &activation_functions[0];
    unsigned int i = 0;

    while (cur->name) {
        functions[i++] = cur->function;
        cur++;
        *size = i;
    }
}

static int
parse_functions(const char *options,
    enum sml_ann_activation_function *functions,
    unsigned int *size)
{
    const struct activation_function *cur;
    char *token, *saveptr, *func_options;
    unsigned int j, i = 0;

    func_options = strdup(options);
    SOL_NULL_CHECK(func_options, -ENOMEM);

    /* TODO: do we have something like strtok for sol_str_slice? */
    token = strtok_r(func_options, " ", &saveptr);
    while (token) {
        cur = &activation_functions[0];
        while (1) {
            if (!strcmp(cur->name, token)) {
                if (!i)
                    functions[i++] = cur->function;
                else {
                    for (j = 0; j < i; j++)
                        if (functions[j] == cur->function)
                            break;
                    functions[i++] = cur->function;
                }
                break;
            }

            cur++;
            if (!cur->name) {
                SOL_WRN("Invalid function: %s. Skiping it.", token);
                break;
            }
        }
        token = strtok_r(NULL, " ", &saveptr);
    }

    *size = i;
    free(func_options);

    return 0;
}

static int
create_sml_ann(struct sml_object **sml, int32_t stabilization_hits, double mse,
    int32_t initial_required_observations, const char *training_algorithm,
    const char *activation_functions)
{
    struct sml_object *sml_ann;
    unsigned int functions_size;
    int r;
    enum sml_ann_activation_function functions[MAX_FUNCTIONS];
    enum sml_ann_training_algorithm algorithm =
        SML_ANN_TRAINING_ALGORITHM_RPROP;

    sml_ann = sml_ann_new();

    if (stabilization_hits < 0) {
        SOL_WRN("Stabilization hits (%d) must be a positive value. Assuming 0.",
            stabilization_hits);
        stabilization_hits = 0;
    }

    if (!sml_set_stabilization_hits(sml_ann, stabilization_hits)) {
        SOL_WRN("Failed to set stabilization hits");
        goto err;
    }

    if (initial_required_observations > 0) {
        if (!sml_ann_set_initial_required_observations(sml_ann,
            initial_required_observations)) {
            SOL_WRN("Failed to set initial required observations");
            goto err;
        }
    }

    /* TODO: maybe we should have some kind of enum? */
    if (!strcmp(training_algorithm, "quickprop"))
        algorithm = SML_ANN_TRAINING_ALGORITHM_QUICKPROP;
    else if (strcmp(training_algorithm, "rprop"))
        SOL_WRN("Training algorithm %s not supported. Using rprop.",
            training_algorithm);
    if (!sml_ann_set_training_algorithm(sml_ann, algorithm)) {
        SOL_WRN("Failed to set training algorithm");
        goto err;
    }

    /* TODO: maybe we should have an array of strings option type? */
    if (!activation_functions) {
        SOL_WRN("Activation functions is mandatory. Using all candidates");
        use_default_functions(functions, &functions_size);
    } else {
        r = parse_functions(activation_functions, functions, &functions_size);
        SOL_INT_CHECK_GOTO(r, < 0, err_r);

        if (functions_size == 0)
            use_default_functions(functions, &functions_size);
    }
    if (!sml_ann_set_activation_function_candidates(sml_ann, functions,
        functions_size)) {
        SOL_WRN("Failed to set desired error");
        goto err;
    }

    if (!isgreater(mse, 0)) {
        SOL_WRN("Desired mean squared error (%f) must be a positive value."
            "Assuming 0.1", mse);
        mse = 0.1;
    }
    if (!sml_ann_set_desired_error(sml_ann, mse)) {
        SOL_WRN("Failed to set desired error");
        goto err;
    }

    *sml = sml_ann;
    return 0;

err:
    r = -EINVAL;
err_r:
    sml_free(sml_ann);
    return r;
}

static int
machine_learning_neural_network_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    int r;
    struct machine_learning_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_neural_network_options
    *opts;

    opts = (const struct
        sol_flow_node_type_machine_learning_neural_network_options *)options;

    mdata->node = node;

    r = create_sml_ann(&mdata->sml, opts->stabilization_hits.val,
        opts->mse.val, opts->initial_required_observations.val,
        opts->training_algorithm, opts->activation_functions);
    SOL_INT_CHECK(r, < 0, r);

    r = init_machine_learning(mdata);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!opts->data_dir)
        return 0;

    r = copy_str(&mdata->sml_data_dir, opts->data_dir);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!sml_load(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Could not load the sml data at:%s", mdata->sml_data_dir);

    return 0;

err:
    sml_free(mdata->sml);
    return r;
}

static void
machine_learning_close(struct sol_flow_node *node, void *data)
{
    struct machine_learning_data *mdata = data;
    struct machine_learning_output_var *output_var;
    uint16_t i;
    int error;

    sol_vector_clear(&mdata->input_vec);
    sol_vector_clear(&mdata->input_id_vec);
    SOL_VECTOR_FOREACH_IDX (&mdata->output_vec, output_var, i)
        free(output_var->tag);
    sol_vector_clear(&mdata->output_vec);
    SOL_VECTOR_FOREACH_IDX (&mdata->output_id_vec, output_var, i)
        free(output_var->tag);
    sol_vector_clear(&mdata->output_id_vec);
    if ((error = pthread_mutex_destroy(&mdata->process_lock)))
        SOL_WRN("Error %d when destroying pthread mutex lock", error);
    if ((error = pthread_mutex_destroy(&mdata->read_lock)))
        SOL_WRN("Error %d when destroying pthread mutex lock", error);
    if (mdata->sml_data_dir && !sml_save(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Failed to save SML data at:%s", mdata->sml_data_dir);
    sml_free(mdata->sml);
    free(mdata->sml_data_dir);
    free(mdata->debug_file);
}

static int
input_var_connect(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id)
{
    struct sol_drange initial_value = { NAN, NAN, NAN, NAN };
    struct machine_learning_data *mdata = data;
    struct machine_learning_input_var *input_var;
    struct sml_variable *sml_variable;
    char name[12];

    snprintf(name, sizeof(name), "InVar%d", conn_id);
    sml_variable = sml_new_input(mdata->sml, name);
    SOL_NULL_CHECK(sml_variable, -EBADR);

    if (port == SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__IN__IN_VAR)
        input_var = sol_vector_append(&mdata->input_vec);
    else {
        sml_fuzzy_variable_set_is_id(mdata->sml, sml_variable, true);
        input_var = sol_vector_append(&mdata->input_id_vec);
    }
    if (!input_var) {
        SOL_WRN("Failed to append variable");
        sml_remove_variable(mdata->sml, sml_variable);
        return -ENOMEM;
    }

    input_var->base.sml_variable = sml_variable;
    input_var->base.value = initial_value;
    input_var->base.range_changed = false;

    SOL_DBG("Input variable %s added", name);

    return 0;
}

static int
output_var_connect(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id)
{
    struct sol_drange initial_value = { NAN, NAN, NAN, NAN };
    struct machine_learning_data *mdata = data;
    struct machine_learning_output_var *output_var;
    struct sml_variable *sml_variable;
    char name[12];

    snprintf(name, sizeof(name), "OutVar%d", conn_id);
    sml_variable = sml_new_output(mdata->sml, name);
    SOL_NULL_CHECK(sml_variable, -EBADR);

    if (port == SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__IN__OUT_VAR)
        output_var = sol_vector_append(&mdata->output_vec);
    else {
        sml_fuzzy_variable_set_is_id(mdata->sml, sml_variable, true);
        output_var = sol_vector_append(&mdata->output_id_vec);
    }
    if (!output_var) {
        SOL_WRN("Failed to append variable");
        sml_remove_variable(mdata->sml, sml_variable);
        return -ENOMEM;
    }

    output_var->base.sml_variable = sml_variable;
    output_var->base.value = initial_value;
    output_var->base.range_changed = false;
    output_var->tag = NULL;
    output_var->predicted_value = NAN;

    SOL_DBG("Output variable %s added for %s", name, output_var->tag);

    return 0;
}

static int
input_var_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct sol_drange value;
    struct machine_learning_data *mdata = data;
    struct machine_learning_input_var *input_var;
    int r;

    r = sol_flow_packet_get_drange(packet, &value);
    SOL_INT_CHECK(r, < 0, r);

    if ((r = mutex_lock(&mdata->read_lock)))
        return r;

    if (port == SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__IN__IN_VAR)
        input_var = sol_vector_get(&mdata->input_vec, conn_id);
    else
        input_var = sol_vector_get(&mdata->input_id_vec, conn_id);
    if (!input_var) {
        SOL_WRN("Failed to get input var");
        pthread_mutex_unlock(&mdata->read_lock);
        return -EINVAL;
    }

    if ((!sol_drange_val_equal(input_var->base.value.min, value.min)) ||
        (!sol_drange_val_equal(input_var->base.value.max, value.max)))
        input_var->base.range_changed = true;
    input_var->base.value = value;
    pthread_mutex_unlock(&mdata->read_lock);

    return 0;
}

static int
output_var_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct sol_drange value;
    struct machine_learning_data *mdata = data;
    struct machine_learning_output_var *output_var;
    char *tag;
    int r;

    r = packet_get_tagged_float(packet, &value, &tag);
    SOL_INT_CHECK(r, < 0, r);

    if ((r = mutex_lock(&mdata->read_lock)))
        return r;

    if (port == SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__IN__OUT_VAR)
        output_var = sol_vector_get(&mdata->output_vec, conn_id);
    else
        output_var = sol_vector_get(&mdata->output_id_vec, conn_id);
    if (!output_var) {
        SOL_WRN("Failed to get output var");
        pthread_mutex_unlock(&mdata->read_lock);
        return -EINVAL;
    }

    if (!output_var->tag) {
        output_var->tag = strdup(tag);
        if (!output_var->tag) {
            SOL_WRN("Failed to get output var tag");
            pthread_mutex_unlock(&mdata->read_lock);
            return -ENOMEM;
        }
    }

    if ((!sol_drange_val_equal(output_var->base.value.min, value.min)) ||
        (!sol_drange_val_equal(output_var->base.value.max, value.max)))
        output_var->base.range_changed = true;
    output_var->base.value = value;
    output_var->predicted_value = NAN;
    pthread_mutex_unlock(&mdata->read_lock);

    return 0;
}

static bool
machine_learning_worker_thread_setup(void *data)
{
    struct machine_learning_data *mdata = data;

    mdata->run_process = true;
    return true;
}

static void
machine_learning_worker_thread_feedback_output(struct sol_flow_node *node,
    struct machine_learning_output_var *output_var)
{
    int r;

    if (isnan(output_var->predicted_value))
        return;

    output_var->base.value.val = output_var->predicted_value;
    output_var->predicted_value = NAN;
    r = send_tagged_float_packet(node,
        SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__OUT__OUT,
        &output_var->base.value, output_var->tag);
    if (r < 0)
        SOL_WRN("Failed to send packet %s %f", output_var->tag,
            output_var->base.value.val);
}

static void
machine_learning_worker_thread_feedback(void *data)
{
    struct machine_learning_data *mdata = data;
    struct machine_learning_output_var *output_var;
    uint16_t i;

    if (mutex_lock(&mdata->read_lock))
        return;

    SOL_VECTOR_FOREACH_IDX (&mdata->output_vec, output_var, i)
        machine_learning_worker_thread_feedback_output(mdata->node, output_var);
    SOL_VECTOR_FOREACH_IDX (&mdata->output_id_vec, output_var, i)
        machine_learning_worker_thread_feedback_output(mdata->node, output_var);
    pthread_mutex_unlock(&mdata->read_lock);

    if (mutex_lock(&mdata->process_lock))
        return;

    if (mdata->send_process_finished) {
        if (!sol_flow_send_empty_packet(mdata->node,
            SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY__OUT__PROCESS_FINISHED))
            mdata->send_process_finished = false;
    }

    pthread_mutex_unlock(&mdata->process_lock);
}

static int worker_schedule(struct machine_learning_data *mdata);

static void
machine_learning_worker_thread_finished(void *data)
{
    struct machine_learning_data *mdata = data;

    mdata->worker = NULL;
    machine_learning_worker_thread_feedback(data);

    //No lock needed because worker thread is dead
    if (mdata->process_needed || mdata->predict_needed)
        worker_schedule(mdata);
}

static bool
machine_learning_worker_thread_iterate(void *data)
{
    struct machine_learning_data *mdata = data;
    bool process_needed, predict_needed, save_needed;

    if (mutex_lock(&mdata->process_lock))
        return false;

    process_needed = mdata->process_needed;
    predict_needed = mdata->predict_needed;
    save_needed = mdata->save_needed;

    if (mdata->debug_file) {
        if (!sml_set_debug_log_file(mdata->sml, mdata->debug_file))
            SOL_WRN("Failed to set debug log file at : %s", mdata->debug_file);
        free(mdata->debug_file);
        mdata->debug_file = NULL;
    }

    pthread_mutex_unlock(&mdata->process_lock);

    if (!process_needed && !predict_needed && !save_needed)
        return false;

    if (save_needed && !sml_save(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Failed to save SML data at: %s", mdata->sml_data_dir);

    if ((mdata->run_process && process_needed) || !predict_needed) {
        //Execute process
        if (sml_process(mdata->sml) < 0)
            SOL_WRN("Process failed.");
        mdata->run_process = false;
    } else {
        //Execute predict
        read_state_cb(mdata->sml, mdata);
        if (sml_predict(mdata->sml))
            output_state_changed_cb(mdata->sml, NULL, mdata);
        mdata->run_process = true;
    }

    if (mutex_lock(&mdata->process_lock))
        return false;
    if (mdata->run_process) {
        //Predict has run
        mdata->predict_needed = false;
    } else {
        //Process has run
        mdata->process_needed = false;
        mdata->send_process_finished = true;
    }
    mdata->save_needed = false;
    pthread_mutex_unlock(&mdata->process_lock);

    sol_worker_thread_feedback(mdata->worker);

    return true;
}

static int
worker_schedule(struct machine_learning_data *mdata)
{
    struct sol_worker_thread_spec spec = {
        .api_version = SOL_WORKER_THREAD_SPEC_API_VERSION,
        .setup = machine_learning_worker_thread_setup,
        .cleanup = NULL,
        .iterate = machine_learning_worker_thread_iterate,
        .finished = machine_learning_worker_thread_finished,
        .feedback = machine_learning_worker_thread_feedback,
        .data = mdata
    };

    mdata->worker = sol_worker_thread_new(&spec);
    SOL_NULL_CHECK(mdata->worker, -errno);

    return 0;
}

static int
trigger_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct machine_learning_data *mdata = data;
    int r;

    r = mutex_lock(&mdata->process_lock);
    SOL_INT_CHECK(r, < 0, r);
    mdata->process_needed = true;
    pthread_mutex_unlock(&mdata->process_lock);

    if (!mdata->worker)
        return worker_schedule(mdata);

    return 0;
}

static int
save_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct machine_learning_data *mdata = data;
    int r;

    if (!mdata->sml_data_dir) {
        SOL_ERR("Could not save the SML data. The data dir is NULL !");
        return -EINVAL;
    }

    r = mutex_lock(&mdata->process_lock);
    SOL_INT_CHECK(r, < 0, r);
    mdata->save_needed = true;
    pthread_mutex_unlock(&mdata->process_lock);
    if (!mdata->worker)
        return worker_schedule(mdata);
    return 0;
}

static int
prediction_trigger_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    struct machine_learning_data *mdata = data;
    int r;

    r = mutex_lock(&mdata->process_lock);
    SOL_INT_CHECK(r, < 0, r);
    mdata->predict_needed = true;
    pthread_mutex_unlock(&mdata->process_lock);

    if (!mdata->worker)
        return worker_schedule(mdata);

    return 0;
}

static void
packet_type_sml_data_packet_dispose(
    const struct sol_flow_packet_type *packet_type, void *mem)
{
    struct packet_type_sml_data_packet_data *packet_type_sml_data = mem;

    free(packet_type_sml_data->inputs);
    free(packet_type_sml_data->outputs);
    free(packet_type_sml_data->input_ids);
    free(packet_type_sml_data->output_ids);
}

static int
packet_type_sml_data_packet_init(const struct sol_flow_packet_type *packet_type,
    void *mem, const void *input)
{
    const struct packet_type_sml_data_packet_data *in = input;
    struct packet_type_sml_data_packet_data *sml_data = mem;
    uint16_t i;

    SOL_NULL_CHECK(in->inputs, -EINVAL);
    SOL_NULL_CHECK(in->outputs, -EINVAL);
    SOL_NULL_CHECK(in->inputs_len, -EINVAL);
    SOL_NULL_CHECK(in->outputs_len, -EINVAL);

    if ((in->output_ids_len == 0) != (in->output_ids == NULL)) {
        SOL_WRN("output_ids and output_ids_len values are inconsistent\n");
        return -EINVAL;
    }

    if ((in->input_ids_len == 0) != (in->input_ids == NULL)) {
        SOL_WRN("input_ids and input_ids_len values are inconsistent\n");
        return -EINVAL;
    }

    sml_data->inputs_len = in->inputs_len;
    sml_data->outputs_len = in->outputs_len;
    sml_data->input_ids_len = in->input_ids_len;
    sml_data->output_ids_len = in->output_ids_len;

    sml_data->input_ids = NULL;
    sml_data->output_ids = NULL;
    sml_data->inputs = malloc(in->inputs_len * sizeof(struct sol_drange));
    SOL_NULL_CHECK(sml_data->inputs, -ENOMEM);
    for (i = 0; i < in->inputs_len; i++)
        sml_data->inputs[i] = in->inputs[i];

    sml_data->outputs = malloc(in->outputs_len * sizeof(struct sol_drange));
    SOL_NULL_CHECK_GOTO(sml_data->outputs, err);
    for (i = 0; i < in->outputs_len; i++)
        sml_data->outputs[i] = in->outputs[i];

    if (sml_data->input_ids_len) {
        sml_data->input_ids = malloc(sml_data->input_ids_len *
            sizeof(bool));
        SOL_NULL_CHECK_GOTO(sml_data->input_ids, err);
        for (i = 0; i < sml_data->input_ids_len; i++)
            sml_data->input_ids[i] = in->input_ids[i];
    }

    if (sml_data->output_ids_len) {
        sml_data->output_ids = malloc(sml_data->output_ids_len *
            sizeof(bool));
        SOL_NULL_CHECK_GOTO(sml_data->output_ids, err);
        for (i = 0; i < sml_data->output_ids_len; i++)
            sml_data->output_ids[i] = in->output_ids[i];
    }

    return 0;

err:
    free(sml_data->input_ids);
    free(sml_data->outputs);
    free(sml_data->inputs);
    return -ENOMEM;
}

#define PACKET_TYPE_SML_DATA_PACKET_TYPE_API_VERSION (1)

static const struct sol_flow_packet_type _PACKET_TYPE_SML_DATA = {
    .api_version = PACKET_TYPE_SML_DATA_PACKET_TYPE_API_VERSION,
    .name = "PACKET_TYPE_SML_DATA",
    .data_size = sizeof(struct packet_type_sml_data_packet_data),
    .init = packet_type_sml_data_packet_init,
    .dispose = packet_type_sml_data_packet_dispose,
};
SOL_API const struct sol_flow_packet_type *PACKET_TYPE_SML_DATA =
    &_PACKET_TYPE_SML_DATA;

#undef PACKET_TYPE_SML_DATA_PACKET_TYPE_API_VERSION

SOL_API struct sol_flow_packet *
sml_data_new_packet(struct packet_type_sml_data_packet_data *sml_data)
{
    SOL_NULL_CHECK(sml_data, NULL);
    return sol_flow_packet_new(PACKET_TYPE_SML_DATA, sml_data);
}

SOL_API int
sml_data_get_packet(const struct sol_flow_packet *packet,
    struct packet_type_sml_data_packet_data *sml_data)
{
    SOL_NULL_CHECK(packet, -EINVAL);
    if (sol_flow_packet_get_type(packet) != PACKET_TYPE_SML_DATA)
        return -EINVAL;

    return sol_flow_packet_get(packet, sml_data);
}

SOL_API int
sml_data_send_packet(struct sol_flow_node *src, uint16_t src_port,
    struct packet_type_sml_data_packet_data *sml_data)
{
    struct sol_flow_packet *packet;

    packet = sml_data_new_packet(sml_data);
    SOL_NULL_CHECK(packet, -ENOMEM);

    return sol_flow_send_packet(src, src_port, packet);
}

static void
packet_type_sml_output_data_packet_dispose(
    const struct sol_flow_packet_type *packet_type, void *mem)
{
    struct packet_type_sml_output_data_packet_data *packet_type_sml_output_data = mem;

    free(packet_type_sml_output_data->outputs);
}

static int
packet_type_sml_output_data_packet_init(
    const struct sol_flow_packet_type *packet_type, void *mem,
    const void *input)
{
    const struct packet_type_sml_output_data_packet_data *in = input;
    struct packet_type_sml_output_data_packet_data *sml_output_data = mem;
    uint16_t i;

    SOL_NULL_CHECK(in->outputs, -EINVAL);
    SOL_NULL_CHECK(in->outputs_len, -EINVAL);

    sml_output_data->outputs_len = in->outputs_len;

    sml_output_data->outputs = malloc(in->outputs_len *
        sizeof(struct sol_drange));
    SOL_NULL_CHECK(sml_output_data->outputs, -ENOMEM);
    for (i = 0; i < in->outputs_len; i++)
        sml_output_data->outputs[i] = in->outputs[i];

    return 0;
}

#define PACKET_TYPE_SML_OUTPUT_DATA_PACKET_TYPE_API_VERSION (1)

static const struct sol_flow_packet_type _PACKET_TYPE_SML_OUTPUT_DATA = {
    .api_version = PACKET_TYPE_SML_OUTPUT_DATA_PACKET_TYPE_API_VERSION,
    .name = "PACKET_TYPE_SML_OUTPUT_DATA",
    .data_size = sizeof(struct packet_type_sml_output_data_packet_data),
    .init = packet_type_sml_output_data_packet_init,
    .dispose = packet_type_sml_output_data_packet_dispose,
};
SOL_API const struct sol_flow_packet_type *PACKET_TYPE_SML_OUTPUT_DATA =
    &_PACKET_TYPE_SML_OUTPUT_DATA;

#undef PACKET_TYPE_SML_OUTPUT_DATA_PACKET_TYPE_API_VERSION

SOL_API struct sol_flow_packet *
sml_output_data_new_packet(
    struct packet_type_sml_output_data_packet_data *sml_output_data)
{
    return sol_flow_packet_new(PACKET_TYPE_SML_OUTPUT_DATA, sml_output_data);
}

SOL_API int
sml_output_data_get_packet(const struct sol_flow_packet *packet,
    struct packet_type_sml_output_data_packet_data *sml_output_data)
{
    SOL_NULL_CHECK(packet, -EINVAL);
    if (sol_flow_packet_get_type(packet) != PACKET_TYPE_SML_OUTPUT_DATA)
        return -EINVAL;

    return sol_flow_packet_get(packet, sml_output_data);
}

SOL_API int
sml_output_data_send_packet(struct sol_flow_node *src, uint16_t src_port,
    struct packet_type_sml_output_data_packet_data *sml_output_data)
{
    struct sol_flow_packet *packet;

    packet = sml_output_data_new_packet(sml_output_data);
    SOL_NULL_CHECK(packet, -ENOMEM);

    return sol_flow_send_packet(src, src_port, packet);
}

static int
debug_file_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    struct machine_learning_data *mdata = data;
    const char *str;

    r = sol_flow_packet_get_string(packet, &str);
    SOL_INT_CHECK(r, < 0, r);

    r = mutex_lock(&mdata->process_lock);
    SOL_INT_CHECK(r, < 0, r);

    if (mdata->debug_file)
        free(mdata->debug_file);

    if (str)
        mdata->debug_file = strdup(str);
    else {
        mdata->debug_file = malloc(1);
        mdata->debug_file[0] = 0;
    }

    pthread_mutex_unlock(&mdata->process_lock);

    return 0;
}

struct machine_learning_sync_data {
    //Used only in open method or/and worker thread. No need to lock
    struct sml_object *sml;
    uint16_t number_of_terms;
    struct sml_data_priv *cur_sml_data;
    double *output_steps;
    uint16_t output_steps_len;
    char *sml_data_dir;

    //Used only in main thread. No need to lock
    struct sol_flow_node *node;

    //Used by main thread and process thread. Need to be locked
    struct sol_ptr_vector input_queue;
    struct sol_ptr_vector output_queue;
    char *debug_file;
    bool save_needed;

    struct sol_worker_thread *worker;
    pthread_mutex_t queue_lock;
};

struct sml_data_priv {
    struct packet_type_sml_data_packet_data base;
    bool predict;
};

#define VARIABLE_INPUT_PREFIX "INPUT"
#define VARIABLE_OUTPUT_PREFIX "OUTPUT"

static int
machine_learning_sync_update_variables(struct machine_learning_sync_data *mdata,
    bool output_variable)
{
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    float max, min, width;
    struct sml_variable *var = NULL;
    struct sol_drange *val;
    uint16_t i, len, array_len, array_ids_len;
    struct sml_variables_list *list;
    struct sol_drange *array;
    bool *array_ids;
    const char *prefix;
    struct sml_variable *(*new_variable)
        (struct sml_object *sml, const char *name);

    if (output_variable) {
        list = sml_get_output_list(mdata->sml);
        array = mdata->cur_sml_data->base.outputs;
        array_len = mdata->cur_sml_data->base.outputs_len;
        array_ids = mdata->cur_sml_data->base.output_ids;
        array_ids_len = mdata->cur_sml_data->base.output_ids_len;
        prefix = VARIABLE_OUTPUT_PREFIX;
        new_variable = sml_new_output;
    } else {
        list = sml_get_input_list(mdata->sml);
        array = mdata->cur_sml_data->base.inputs;
        array_len = mdata->cur_sml_data->base.inputs_len;
        array_ids = mdata->cur_sml_data->base.input_ids;
        array_ids_len = mdata->cur_sml_data->base.input_ids_len;
        prefix = VARIABLE_INPUT_PREFIX;
        new_variable = sml_new_input;
    }

    len = sml_variables_list_get_length(mdata->sml, list);
    if (output_variable) {
        mdata->output_steps = realloc(mdata->output_steps,
            sizeof(double) * array_len);
        SOL_NULL_CHECK(mdata->output_steps, -ENOMEM);
        mdata->output_steps_len = len;
    }
    for (i = 0; i < array_len; i++) {
        val = &array[i];
        if (i >= len) {
            snprintf(var_name, sizeof(var_name), "%s%d", prefix, i);
            var = new_variable(mdata->sml, var_name);
        } else
            var = sml_variables_list_index(mdata->sml, list, i);
        SOL_NULL_CHECK(var, -EINVAL);

        if (!sml_variable_get_range(mdata->sml, var, &min, &max))
            return -EINVAL;

        if ((!sol_drange_val_equal(min, val->min)) ||
            (!sol_drange_val_equal(max, val->max))) {
            width = fmax((val->max - val->min + 1) /
                mdata->number_of_terms, val->step);

            if (sml_is_fuzzy(mdata->sml) &&
                !sml_fuzzy_variable_set_default_term_width(mdata->sml, var,
                width))
                return -EINVAL;
            if (!sml_variable_set_range(mdata->sml, var, val->min, val->max))
                return -EINVAL;
        }

        if (output_variable)
            mdata->output_steps[i] = val->step;
    }

    if (var && array_ids)
        for (i = 0; i < array_ids_len; i++)
            if (!sml_fuzzy_variable_set_is_id(mdata->sml, var, array_ids[i]))
                return -EINVAL;

    return 0;
}

#undef VARIABLE_INPUT_PREFIX
#undef VARIABLE_OUTPUT_PREFIX

static int
machine_learning_sync_pre_process(struct machine_learning_sync_data *mdata)
{
    int r;

    r  = machine_learning_sync_update_variables(mdata, false);
    SOL_INT_CHECK(r, < 0, r);

    return machine_learning_sync_update_variables(mdata, true);

}

static bool sync_read_state_cb(struct sml_object *sml, void *data);

static void sync_output_state_changed_cb(struct sml_object *sml, struct sml_variables_list *changed, void *data);

static bool
machine_learning_sync_worker_thread_iterate(void *data)
{
    struct machine_learning_sync_data *mdata = data;
    struct sml_data_priv *sml_data;
    int r;

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK(r, < 0, false);

    if (mdata->debug_file) {
        if (!sml_set_debug_log_file(mdata->sml, mdata->debug_file))
            SOL_WRN("Failed to set debug log file at : %s", mdata->debug_file);
        free(mdata->debug_file);
        mdata->debug_file = NULL;
    }

    if (mdata->save_needed && !sml_save(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Failed to save the SML data at:%s", mdata->sml_data_dir);
    mdata->save_needed = false;

    if (mdata->input_queue.base.len == 0)
        goto end;

    sml_data = sol_ptr_vector_get(&mdata->input_queue, 0);
    SOL_NULL_CHECK_GOTO(sml_data, end);

    r = sol_ptr_vector_del(&mdata->input_queue, 0);
    SOL_INT_CHECK_GOTO(r, < 0, end);

    pthread_mutex_unlock(&mdata->queue_lock);

    mdata->cur_sml_data = sml_data;

    r = machine_learning_sync_pre_process(mdata);
    SOL_INT_CHECK_GOTO(r, < 0, next);

    if (mdata->cur_sml_data->predict) {
        sync_read_state_cb(mdata->sml, mdata);
        if (sml_predict(mdata->sml))
            sync_output_state_changed_cb(mdata->sml, NULL, mdata);
        else
            SOL_WRN("Predict failed.");
    } else {
        if (sml_process(mdata->sml) < 0)
            SOL_WRN("Process failed.");
    }

next:
    packet_type_sml_data_packet_dispose(NULL, &sml_data->base);
    free(sml_data);

    sol_worker_thread_feedback(mdata->worker);
    return true;

end:
    pthread_mutex_unlock(&mdata->queue_lock);
    return false;
}

static void
machine_learning_sync_worker_thread_feedback(void *data)
{
    int r;
    uint16_t i;
    struct packet_type_sml_output_data_packet_data *sml_output_data;
    struct machine_learning_sync_data *mdata = data;

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK(r, < 0);
    if (mdata->output_queue.base.len == 0)
        goto end;

    for (i = 0; i < mdata->output_queue.base.len; i++) {
        sml_output_data = sol_ptr_vector_get(&mdata->output_queue, i);
        SOL_NULL_CHECK_GOTO(sml_output_data, end);
        r = sml_output_data_send_packet(mdata->node,
            SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY_SYNC__OUT__OUT,
            sml_output_data);
        packet_type_sml_output_data_packet_dispose(NULL, sml_output_data);
        free(sml_output_data);
    }
    sol_ptr_vector_clear(&mdata->output_queue);

end:
    pthread_mutex_unlock(&mdata->queue_lock);
}

static int machine_learning_sync_worker_schedule(struct machine_learning_sync_data *mdata);

static void
machine_learning_sync_worker_thread_finished(void *data)
{
    struct machine_learning_sync_data *mdata = data;

    mdata->worker = NULL;
    machine_learning_sync_worker_thread_feedback(data);

    //No lock needed because worker thread is dead
    if (mdata->input_queue.base.len > 0)
        machine_learning_sync_worker_schedule(mdata);
}

static int
machine_learning_sync_worker_schedule(struct machine_learning_sync_data *mdata)
{
    struct sol_worker_thread_spec spec = {
        .api_version = SOL_WORKER_THREAD_SPEC_API_VERSION,
        .cleanup = NULL,
        .iterate = machine_learning_sync_worker_thread_iterate,
        .finished = machine_learning_sync_worker_thread_finished,
        .feedback = machine_learning_sync_worker_thread_feedback,
        .data = mdata
    };

    mdata->worker = sol_worker_thread_new(&spec);
    SOL_NULL_CHECK(mdata->worker, -errno);

    return 0;
}

static void
machine_learning_sync_close(struct sol_flow_node *node, void *data)
{
    struct machine_learning_sync_data *mdata = data;
    struct packet_type_sml_output_data_packet_data *output_data;
    struct sml_data_priv *sml_data;
    int error;
    uint16_t i;

    if (mdata->sml_data_dir && !sml_save(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Failed to save SML data at:%s", mdata->sml_data_dir);
    if ((error = pthread_mutex_destroy(&mdata->queue_lock)))
        SOL_WRN("Error %d when destroying pthread mutex lock", error);
    SOL_PTR_VECTOR_FOREACH_IDX (&mdata->output_queue, output_data, i)
        free(output_data);
    SOL_PTR_VECTOR_FOREACH_IDX (&mdata->input_queue, sml_data, i)
        free(sml_data);
    sml_free(mdata->sml);
    sol_ptr_vector_clear(&mdata->input_queue);
    sol_ptr_vector_clear(&mdata->output_queue);
    free(mdata->output_steps);
    free(mdata->sml_data_dir);
    free(mdata->debug_file);
}

static int
sml_data_priv_create(struct packet_type_sml_data_packet_data *sml_data,
    struct sml_data_priv **new_sml_data)
{
    int r;
    struct sml_data_priv *new;

    new = malloc(sizeof(struct sml_data_priv));
    SOL_NULL_CHECK(new, -ENOMEM);

    r = packet_type_sml_data_packet_init(NULL, &new->base, sml_data);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    *new_sml_data = new;
    return 0;

err:
    free(new);
    return r;
}

static int
sml_data_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    struct machine_learning_sync_data *mdata = data;
    struct packet_type_sml_data_packet_data sml_data;
    struct sml_data_priv *new_sml_data = NULL;

    r = sml_data_get_packet(packet, &sml_data);
    SOL_INT_CHECK(r, < 0, r);

    r = sml_data_priv_create(&sml_data, &new_sml_data);
    SOL_INT_CHECK(r, < 0, r);

    if (port == SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FUZZY_SYNC__IN__IN)
        new_sml_data->predict = false;
    else
        new_sml_data->predict = true;

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    r = sol_ptr_vector_append(&mdata->input_queue, new_sml_data);
    SOL_INT_CHECK_GOTO(r, < 0, err_mutex);

    pthread_mutex_unlock(&mdata->queue_lock);

    if (!mdata->worker)
        return machine_learning_sync_worker_schedule(mdata);
    return 0;

err_mutex:
    pthread_mutex_unlock(&mdata->queue_lock);
err:
    free(new_sml_data);
    return r;
}

static int
sml_data_save_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct machine_learning_sync_data *mdata = data;
    int r;

    if (!mdata->sml_data_dir) {
        SOL_ERR("Could not save the SML data. The data dir is NULL !");
        return -EINVAL;
    }

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK(r, < 0, r);
    mdata->save_needed = true;
    pthread_mutex_unlock(&mdata->queue_lock);
    if (!mdata->worker)
        return machine_learning_sync_worker_schedule(mdata);
    return 0;
}

static bool
sync_fill_variables(struct sml_object *sml, struct sml_variables_list *list,
    struct sol_drange *array, uint16_t array_len)
{
    uint16_t i, len;
    struct sml_variable *var;

    len = sml_variables_list_get_length(sml, list);
    for (i = 0; i < len && i < array_len; i++) {
        var = sml_variables_list_index(sml, list, i);
        if (!sml_variable_set_value(sml, var, array[i].val))
            return false;
    }

    return true;
}

static bool
sync_read_state_cb(struct sml_object *sml, void *data)
{
    struct machine_learning_sync_data *mdata = data;

    if (!sync_fill_variables(sml, sml_get_input_list(sml),
        mdata->cur_sml_data->base.inputs, mdata->cur_sml_data->base.inputs_len))
        return false;

    if (!sync_fill_variables(sml, sml_get_output_list(sml),
        mdata->cur_sml_data->base.outputs,
        mdata->cur_sml_data->base.outputs_len))
        return false;

    return true;
}

static void
sync_output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
    int r;
    uint16_t i, len;
    float min, max;
    struct sml_variables_list *list;
    struct sml_variable *var;
    struct packet_type_sml_output_data_packet_data *sml_output_data;
    struct machine_learning_sync_data *mdata = data;

    list = sml_get_output_list(sml);
    len = sml_variables_list_get_length(sml, list);

    sml_output_data =
        malloc(sizeof(struct packet_type_sml_output_data_packet_data));
    SOL_NULL_CHECK(sml_output_data);
    sml_output_data->outputs = malloc(sizeof(struct sol_drange) * len);
    SOL_NULL_CHECK_GOTO(sml_output_data->outputs, err);
    sml_output_data->outputs_len = len;

    for (i = 0; i < len; i++) {
        var = sml_variables_list_index(sml, list, i);
        SOL_NULL_CHECK_GOTO(var, err);
        if (!changed || sml_variables_list_contains(sml, changed, var)) {
            if (!sml_variable_get_range(sml, var, &min, &max))
                goto err;

            sml_output_data->outputs[i].val = sml_variable_get_value(sml, var);
            sml_output_data->outputs[i].min = min;
            sml_output_data->outputs[i].max = max;
            if (i < mdata->output_steps_len)
                sml_output_data->outputs[i].step = mdata->output_steps[i];
            else
                sml_output_data->outputs[i].step = NAN;
        } else {
            sml_output_data->outputs[i].val = NAN;
            sml_output_data->outputs[i].min = NAN;
            sml_output_data->outputs[i].max = NAN;
            sml_output_data->outputs[i].step = NAN;
        }
    }

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    r = sol_ptr_vector_append(&mdata->output_queue, sml_output_data);

    pthread_mutex_unlock(&mdata->queue_lock);
    SOL_INT_CHECK_GOTO(r, < 0, err);
    return;

err:
    free(sml_output_data->outputs);
    free(sml_output_data);
}

int
init_machine_learning_sync(struct machine_learning_sync_data *mdata)
{
    int r;

    if (!sml_set_read_state_callback(mdata->sml, sync_read_state_cb, mdata)) {
        SOL_WRN("Failed to set read callback");
        return -EINVAL;
    }

    if (!sml_set_output_state_changed_callback(mdata->sml,
        sync_output_state_changed_cb, mdata)) {
        SOL_WRN("Failed to set change state callback");
        return -EINVAL;
    }

    sol_ptr_vector_init(&mdata->input_queue);
    sol_ptr_vector_init(&mdata->output_queue);

    if ((r = pthread_mutex_init(&mdata->queue_lock, NULL))) {
        SOL_WRN("Failed to initialize pthread mutex lock");
        return -r;
    }
    return 0;
}

static int
fuzzy_sync_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    const struct sol_flow_node_type_machine_learning_fuzzy_options *opts;
    struct machine_learning_sync_data *mdata = data;
    int r;

    opts = (const struct
        sol_flow_node_type_machine_learning_fuzzy_options *)options;

    mdata->node = node;
    if (opts->number_of_terms.val >= 0)
        mdata->number_of_terms = opts->number_of_terms.val;
    else {
        SOL_WRN("Number of fuzzy terms (%d) must be a positive value. "
            "Assuming %d.", opts->number_of_terms.val, AUTOMATIC_TERMS);
        mdata->number_of_terms = AUTOMATIC_TERMS;
    }

    r = create_sml_fuzzy(&mdata->sml, opts->stabilization_hits.val);
    SOL_INT_CHECK(r, < 0, r);

    r = init_machine_learning_sync(mdata);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!opts->data_dir)
        return 0;

    r = copy_str(&mdata->sml_data_dir, opts->data_dir);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!sml_load(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Could not load the sml data at:%s", mdata->sml_data_dir);

    return 0;

err:
    sml_free(mdata->sml);
    return r;
}

static int
neural_network_sync_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    int r;
    struct machine_learning_sync_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_neural_network_options
    *opts;

    opts = (const struct
        sol_flow_node_type_machine_learning_neural_network_options *)options;

    mdata->node = node;
    r = create_sml_ann(&mdata->sml, opts->stabilization_hits.val,
        opts->mse.val, opts->initial_required_observations.val,
        opts->training_algorithm, opts->activation_functions);
    SOL_INT_CHECK(r, < 0, r);

    r = init_machine_learning_sync(mdata);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!opts->data_dir)
        return 0;

    r = copy_str(&mdata->sml_data_dir, opts->data_dir);
    SOL_INT_CHECK_GOTO(r, < 0, err);

    if (!sml_load(mdata->sml, mdata->sml_data_dir))
        SOL_WRN("Could not load the sml data at:%s", mdata->sml_data_dir);

    return 0;

err:
    sml_free(mdata->sml);
    return r;
}

static int
sml_data_debug_file(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    struct machine_learning_sync_data *mdata = data;
    const char *str;

    r = sol_flow_packet_get_string(packet, &str);
    SOL_INT_CHECK(r, < 0, r);

    r = mutex_lock(&mdata->queue_lock);
    SOL_INT_CHECK(r, < 0, r);

    if (mdata->debug_file)
        free(mdata->debug_file);

    if (str)
        mdata->debug_file = strdup(str);
    else {
        mdata->debug_file = malloc(1);
        mdata->debug_file[0] = 0;
    }

    pthread_mutex_unlock(&mdata->queue_lock);

    return 0;
}

static int
filter_sync_process(struct sol_flow_node *node, void *data, uint16_t port,
    uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    uint16_t i, out_port;
    struct packet_type_sml_output_data_packet_data sml_output_data;

    out_port  = SOL_FLOW_NODE_TYPE_MACHINE_LEARNING_FILTER_SYNC__OUT__OUT_0;

    sml_output_data_get_packet(packet, &sml_output_data);
    for (i = 0; i < sml_output_data.outputs_len; i++) {
        r = sol_flow_send_drange_packet(node, out_port + i,
            &sml_output_data.outputs[i]);
        SOL_INT_CHECK(r, < 0, r);
    }

    return 0;
}

#undef MAX_FUNCTIONS
#undef AUTOMATIC_TERMS

#include "machine_learning_gen.c"
