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

#include "machine_learning_gen.h"

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
    return error;
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
    int ret;

    packet = packet_new_tagged_float(value, tag);
    SOL_NULL_CHECK(packet, -ENOMEM);

    ret = sol_flow_send_packet(src, src_port, packet);
    if (ret != 0)
        sol_flow_packet_del(packet);

    return ret;
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

    //Used by main thread and process thread. Need to be locked
    struct sol_vector input_vec;
    struct sol_vector input_id_vec;
    struct sol_vector output_vec;
    struct sol_vector output_id_vec;
    bool process_needed, predict_needed, send_process_finished;

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
    sml_variable_set_range(mdata->sml, var->sml_variable, var->value.min,
        var->value.max);

    if (!sml_is_fuzzy(mdata->sml))
        return;

    width = fmax((var->value.max - var->value.min + 1) /
        mdata->number_of_terms, var->value.step);

    sml_fuzzy_variable_set_default_term_width(mdata->sml, var->sml_variable,
        width);
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
create_engine(struct machine_learning_data *mdata)
{
    int r;

    if (!mdata->sml) {
        SOL_WRN("Failed to create sml");
        return -EBADR;
    }

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

#define AUTOMATIC_TERMS (15)

static int
machine_learning_fuzzy_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    struct machine_learning_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_fuzzy_options *opts;
    int32_t stabilization_hits;
    int r;

    opts = (const struct
        sol_flow_node_type_machine_learning_fuzzy_options *)options;

    mdata->node = node;

    mdata->sml = sml_fuzzy_new();
    r = create_engine(mdata);
    SOL_INT_CHECK(r, < 0, r);

    if (opts->stabilization_hits.val >= 0)
        stabilization_hits = opts->stabilization_hits.val;
    else {
        SOL_WRN("Stabilization hits (%d) must be a positive value. Assuming 0.",
            opts->stabilization_hits.val);
        stabilization_hits = 0;
    }

    if (opts->number_of_terms.val >= 0)
        mdata->number_of_terms = opts->number_of_terms.val;
    else {
        SOL_WRN("Number of fuzzy terms (%d) must be a positive value. "
            "Assuming %d.", opts->number_of_terms.val, AUTOMATIC_TERMS);
        mdata->number_of_terms = AUTOMATIC_TERMS;
    }

    if (!sml_set_stabilization_hits(mdata->sml, stabilization_hits)) {
        SOL_WRN("Failed to set stabilization hits");
        return -EINVAL;
    }

    return 0;
}

#undef AUTOMATIC_TERMS

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

/* TODO should we have SML_ACTIVATION_FUNCTION_LAST? */
#define MAX_FUNCTIONS (SML_ANN_ACTIVATION_FUNCTION_SIN_SYMMETRIC + 1)

static int
machine_learning_neural_network_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    double mse;
    struct machine_learning_data *mdata = data;
    const struct sol_flow_node_type_machine_learning_neural_network_options
    *opts;
    enum sml_ann_activation_function functions[MAX_FUNCTIONS];
    int r;
    enum sml_ann_training_algorithm algorithm =
        SML_ANN_TRAINING_ALGORITHM_RPROP;
    unsigned int functions_size;
    int32_t stabilization_hits;

    opts = (const struct
        sol_flow_node_type_machine_learning_neural_network_options *)options;

    mdata->node = node;

    mdata->sml = sml_ann_new();
    r = create_engine(mdata);
    SOL_INT_CHECK(r, < 0, r);

    if (opts->stabilization_hits.val >= 0)
        stabilization_hits = opts->stabilization_hits.val;
    else {
        SOL_WRN("Stabilization hits (%d) must be a positive value. Assuming 0.",
            opts->stabilization_hits.val);
        stabilization_hits = 0;
    }

    if (!sml_set_stabilization_hits(mdata->sml, stabilization_hits)) {
        SOL_WRN("Failed to set stabilization hits");
        return -EINVAL;
    }

    if (opts->initial_required_observations.val > 0) {
        if (!sml_ann_set_initial_required_observations(mdata->sml,
            opts->initial_required_observations.val)) {
            SOL_WRN("Failed to set initial required observations");
            return -EINVAL;
        }
    }

    /* TODO: maybe we should have some kind of enum? */
    if (!strcmp(opts->training_algorithm, "quickprop"))
        algorithm = SML_ANN_TRAINING_ALGORITHM_QUICKPROP;
    else if (strcmp(opts->training_algorithm, "rprop"))
        SOL_WRN("Training algorithm %s not supported. Using rprop.",
            opts->training_algorithm);
    if (!sml_ann_set_training_algorithm(mdata->sml, algorithm)) {
        SOL_WRN("Failed to set training algorithm");
        return -EINVAL;
    }

    /* TODO: maybe we should have an array of strings option type? */
    if (!opts->activation_functions) {
        SOL_WRN("Activation functions is mandatory. Using all candidates");
        use_default_functions(functions, &functions_size);
    } else {
        r = parse_functions(opts->activation_functions, functions,
            &functions_size);
        SOL_INT_CHECK(r, < 0, r);

        if (functions_size == 0)
            use_default_functions(functions, &functions_size);
    }
    if (!sml_ann_set_activation_function_candidates(mdata->sml, functions,
        functions_size)) {
        SOL_WRN("Failed to set desired error");
        return -EINVAL;
    }

    if (isgreater(opts->mse.val, 0))
        mse = opts->mse.val;
    else {
        SOL_WRN("Desired mean squared error (%f) must be a positive value."
            "Assuming 0.1", opts->mse.val);
        mse = 0.1;
    }
    if (!sml_ann_set_desired_error(mdata->sml, mse)) {
        SOL_WRN("Failed to set desired error");
        return -EINVAL;
    }

    return 0;
}

#undef MAX_FUNCTIONS

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
    sml_free(mdata->sml);
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
    SOL_NULL_CHECK(input_var, -EINVAL);

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
    SOL_NULL_CHECK(output_var, -EINVAL);

    if (!output_var->tag) {
        output_var->tag = strdup(tag);
        SOL_NULL_CHECK(output_var->tag, -ENOMEM);
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
    bool process_needed, predict_needed;

    if (mutex_lock(&mdata->process_lock))
        return false;

    process_needed = mdata->process_needed;
    predict_needed = mdata->predict_needed;
    pthread_mutex_unlock(&mdata->process_lock);

    if (!process_needed && !predict_needed)
        return false;

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

    if (mutex_lock(&mdata->process_lock))
        return false;

    mdata->process_needed = true;
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

    if (mutex_lock(&mdata->process_lock))
        return false;

    mdata->predict_needed = true;
    pthread_mutex_unlock(&mdata->process_lock);

    if (!mdata->worker)
        return worker_schedule(mdata);

    return 0;
}

#include "machine_learning_gen.c"
