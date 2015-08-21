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

#include <sml_ann.h>
#include <string.h>
#include "sml_ann_variable_list.h"
#include <macros.h>
#include <math.h>
#include <errno.h>
#include "sml_cache.h"
#include <sml_engine.h>
#include <sol-vector.h>
#include <sml_log.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include "sml_ann_bridge.h"
#include "sml_util.h"
#include <sys/stat.h>
#include <config.h>

#define DEFAULT_EPOCHS (300)
#define DEFAULT_CANDIDATE_GROUPS (6)
#define DEFAULT_DESIRED_ERROR (0.01)
#define MIN_TRESHOLD (0.5)
#define INITIAL_REQUIRED_OBSERVATIONS (2500)
#define DEFAULT_CACHE_SIZE (30)
#define ANN_FILE_PREFIX "ann_"
#define ANN_FILE_EXTENSION "net"
#define CI_FILE_PREFIX "ann_ci_"
#define CI_FILE_EXTENSION "ci"
#define ANN_PSEUDOREHEARSAL_PREFIX "pseudo_rehearsal_ann"
#define CFG_FILE_PREFIX "ann_cfg_"
#define CFG_FILE_EXTENSION "cfg"
#define ANN_MAGIC (0xa22)
#define EXPAND_FACTOR (3)

struct sml_ann_engine {
    struct sml_engine engine;

    struct sml_variables_list *inputs;
    struct sml_variables_list *outputs;
    bool first_run;
    bool use_pseudorehearsal;

    unsigned int required_observations;
    unsigned int train_epochs;
    float train_error;
    unsigned int max_neurons;
    unsigned int candidate_groups;

    enum sml_ann_training_algorithm train_algorithm;

    struct sol_ptr_vector pending_add;
    struct sol_ptr_vector pending_remove;
    struct sol_vector activation_functions;

    struct sml_cache *anns_cache;
};

//FIXME: Is this a good approuch?
static struct sml_variables_list *
_sml_ann_output_has_significant_changes(struct sml_variables_list *outputs)
{
    uint16_t i, size;
    struct sml_variable *var;
    float neural_value, read_value;
    struct sml_variables_list *changed = sml_ann_variable_list_new();

    if (!changed) {
        sml_critical("Could not alloc the output changed list !");
        return NULL;
    }

    size = sml_ann_variables_list_get_length(outputs);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(outputs, i);

        neural_value = sml_ann_variable_get_value(var);
        read_value = sml_ann_variable_get_previous_value(var);
        /* Neural value will never be NAN at this point! */
        if (isnan(read_value) ||
            fabs(read_value - neural_value) >= MIN_TRESHOLD) {
            if (sml_ann_variable_list_add_variable(changed, var)) {
                sml_ann_variable_list_free(changed, false);
                sml_critical("Could not add a variable to the changed list!");
                return NULL;
            }
        } else /* If no significant changes happened, set the old value. */
            sml_ann_variable_set_value(var, read_value);
    }
    sml_debug("Changed list size:%d",
        sml_ann_variables_list_get_length(changed));
    return changed;
}

//FIXME: Is this a good approuch?
static bool
_sml_ann_variable_list_has_significant_changes(
    struct sml_variables_list *inputs)
{

    uint16_t i, size;
    struct sml_variable *var;
    float last, stable;

    size = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(inputs, i);
        last = sml_ann_variable_get_value(var);
        stable = sml_ann_variable_get_last_stable_value(var);

        if (!isnan(stable) && !isnan(last)) {
            if (fabs(last - stable) >= MIN_TRESHOLD)
                return true;
        } else if (!(isnan(stable) && isnan(last)))
            return true;
    }
    return false;
}

static unsigned int
_sml_ann_get_observations_length(struct sml_ann_engine *ann_engine)
{
    struct sml_variable *var;

    var = sml_ann_variables_list_index(ann_engine->inputs, 0);

    if (!var)
        return 0;
    return sml_ann_variable_get_observations_length(var);
}

static bool
_sml_ann_can_alloc_memory_for_observations(unsigned int total_variables,
    unsigned int observations_size,
    unsigned int max_memory_size)
{
    if (!max_memory_size)
        return true;

    unsigned int final_size = observations_size *
        sizeof(float)  * total_variables;
    return final_size <= max_memory_size;
}

static int
_sml_ann_train(struct sml_ann_engine *ann_engine, struct sml_ann_bridge *iann,
    unsigned int observations_size)
{
    int error;
    unsigned int required_observations_suggestion;
    bool retrain, can_realloc;
    uint16_t in_size, out_size, pending_size, i;
    struct sml_variable *var;

    error = sml_ann_bridge_train(iann, ann_engine->inputs, ann_engine->outputs,
        ann_engine->train_error,
        observations_size,
        ann_engine->max_neurons,
        &required_observations_suggestion,
        ann_engine->use_pseudorehearsal);
    if (error)
        return error;

    can_realloc = true;
    retrain = false;
    if (required_observations_suggestion > ann_engine->required_observations) {
        in_size = sml_ann_variables_list_get_length(ann_engine->inputs);
        out_size = sml_ann_variables_list_get_length(ann_engine->outputs);
        pending_size = sol_ptr_vector_get_len(&ann_engine->pending_add);
        if (!_sml_ann_can_alloc_memory_for_observations(
            in_size + out_size + pending_size,
            required_observations_suggestion,
            ann_engine->engine.obs_max_size)) {
            sml_warning("Can not alloc more memory for observations,"   \
                "obs_max_size has been reached."       \
                "Considering the network trained");
            can_realloc = false;
            error = sml_ann_bridge_consider_trained(iann, ann_engine->inputs,
                ann_engine->required_observations,
                ann_engine->use_pseudorehearsal);
        }
    } else if (required_observations_suggestion <
        ann_engine->required_observations)
        retrain = true;
    else
        can_realloc = false;

    if (!sml_ann_bridge_is_trained(iann)) {
        if (can_realloc) {
            ann_engine->required_observations =
                required_observations_suggestion;
            if ((error = sml_ann_variables_list_realloc_observations_array(
                    ann_engine->inputs, ann_engine->required_observations)))
                return error;
            if ((error = sml_ann_variables_list_realloc_observations_array(
                    ann_engine->outputs, ann_engine->required_observations)))
                return error;
            SOL_PTR_VECTOR_FOREACH_IDX (&ann_engine->pending_add, var, i) {
                if ((error = sml_ann_variable_realloc_observation_array(
                        var, ann_engine->required_observations)))
                    return error;
            }
        }
        if (retrain) {
            error = sml_ann_bridge_train(iann, ann_engine->inputs,
                ann_engine->outputs,
                ann_engine->train_error,
                ann_engine->required_observations,
                ann_engine->max_neurons,
                NULL,
                ann_engine->use_pseudorehearsal);
        }
    }
    return error;
}

static bool
_sml_ann_remove_variable_from_sml(struct sml_ann_engine *ann_engine,
    struct sml_variable *var_to_remove, bool input)
{
    struct sml_variable *var;
    struct sml_variables_list *list;
    uint16_t i, size;

    if (input)
        list = ann_engine->inputs;
    else
        list = ann_engine->outputs;

    size = sml_ann_variables_list_get_length(list);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(list, i);
        if (var == var_to_remove) {
            sml_ann_variable_list_remove(list, i);
            return true;
        }
    }
    return false;
}

static struct sml_variable *
_sml_ann_find_variable_by_name(struct sml_ann_engine *ann_engine,
    const char *name, bool input)
{
    struct sml_variables_list *list;
    struct sml_variable *var;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    uint16_t i, size;

    if (input)
        list = ann_engine->inputs;
    else
        list = ann_engine->outputs;

    size = sml_ann_variables_list_get_length(list);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(list, i);
        if (sml_ann_variable_get_name(var, var_name, sizeof(var_name))) {
            sml_warning("Failed to get name of var %p", var);
            continue;
        }
        if (!strcmp(name, var_name))
            return var;
    }
    return NULL;
}

static bool
_sml_ann_remove_variable_from_lists(struct sml_ann_engine *ann_engine,
    struct sml_variable *var)
{
    if (!_sml_ann_remove_variable_from_sml(ann_engine, var, true))
        return _sml_ann_remove_variable_from_sml(ann_engine, var, false);
    return true;
}

static struct sml_variable *
_sml_ann_add_variable(struct sml_ann_engine *ann_engine, const char *name,
    bool input)
{
    struct sml_variable *var;
    struct sml_variables_list *list;
    unsigned int total;

    if (input)
        list = ann_engine->inputs;
    else
        list = ann_engine->outputs;

    var = sml_ann_variable_new(name, input);

    if (!var)
        return NULL;

    total = sml_ann_variables_list_get_length(ann_engine->inputs) +
        sml_ann_variables_list_get_length(ann_engine->outputs) +
        sol_ptr_vector_get_len(&ann_engine->pending_add) + 1;
    if (!_sml_ann_can_alloc_memory_for_observations(total,
        ann_engine->required_observations,
        ann_engine->engine.obs_max_size) ||
        sml_ann_variable_realloc_observation_array(var,
        ann_engine->required_observations)) {
        sml_critical("Could not alloc the observation array!");
        goto err_exit;
    }
    sml_ann_variable_set_observations_array_index(var,
        _sml_ann_get_observations_length(ann_engine));

    if (!sml_cache_get_size(ann_engine->anns_cache) &&
        sml_ann_variable_list_add_variable(list, var)) {
        sml_critical("Could not add the variable to the list");
        goto err_exit;
    } else if (sml_cache_get_size(ann_engine->anns_cache) &&
        sol_ptr_vector_append(&ann_engine->pending_add, var)) {
        sml_critical("Could not add the variable to the add pending list");
        goto err_exit;
    }

    return var;
err_exit:
    sml_ann_variable_free(var);
    return NULL;
}

static struct sml_ann_bridge *
_sml_ann_new(struct sml_ann_engine *ann_engine, int *error_code)
{

    if (ann_engine->use_pseudorehearsal &&
        sml_cache_get_size(ann_engine->anns_cache) == 1) {
        sml_debug("Returning previous created ANN - pseudorehearsal");
        return sml_cache_get_element(ann_engine->anns_cache, 0);
    }

    sml_debug("Creating a new ANN!");
    struct sml_ann_bridge *iann = sml_ann_bridge_new(
        sml_ann_variables_list_get_length(ann_engine->inputs),
        sml_ann_variables_list_get_length(ann_engine->outputs),
        ann_engine->candidate_groups,
        ann_engine->train_epochs,
        ann_engine->train_algorithm,
        &ann_engine->activation_functions, error_code);

    if (!iann)
        return NULL;

    if (!sml_cache_put(ann_engine->anns_cache, iann)) {
        sml_ann_bridge_free(iann);
        if (error_code)
            *error_code = -ENOMEM;
        return NULL;
    }
    return iann;
}

static int
_sml_ann_change_ann_layout_if_needed(struct sml_ann_engine *ann_engine)
{
    struct sml_ann_bridge *iann;
    struct sml_variable *var;
    struct sml_variables_list *list;
    unsigned int inputs, outputs;
    uint16_t i;
    bool changed = false;
    int error;

    if (!sol_ptr_vector_get_len(&ann_engine->pending_add) &&
        !sol_ptr_vector_get_len(&ann_engine->pending_remove))
        return 0;

    inputs = sml_ann_variables_list_get_length(ann_engine->inputs);
    outputs = sml_ann_variables_list_get_length(ann_engine->outputs);

    /* Check the new number of inputs/outputs */
    SOL_PTR_VECTOR_FOREACH_IDX (&ann_engine->pending_add, var, i) {
        if (sml_ann_variable_is_input(var)) {
            inputs++;
            list = ann_engine->inputs;
            sml_debug("Adding input variable");
        } else {
            outputs++;
            list = ann_engine->outputs;
            sml_debug("Adding output variable");
        }
        changed = true;
        sml_ann_variable_list_add_variable(list, var);
    }

    SOL_PTR_VECTOR_FOREACH_IDX (&ann_engine->pending_remove, var, i) {
        if (sml_ann_variable_is_input(var)) {
            if (inputs > 0)
                inputs--;
            sml_debug("Removing input variable");
        } else {
            if (outputs > 0)
                outputs--;
            sml_debug("Removing output variable");
        }
        changed = true;
        _sml_ann_remove_variable_from_lists(ann_engine, var);
    }

    sol_ptr_vector_clear(&ann_engine->pending_remove);
    sol_ptr_vector_clear(&ann_engine->pending_add);

    if (changed) {
        sml_cache_clear(ann_engine->anns_cache);
        iann = _sml_ann_new(ann_engine, &error);
        if (!iann) {
            sml_critical("Could not create a new ANN");
            return error;
        }
        ann_engine->max_neurons = 0;
        if ((error = _sml_ann_train(ann_engine, iann,
                ann_engine->required_observations)))
            return error;
        sml_ann_variables_list_reset_observations(ann_engine->inputs);
        sml_ann_variables_list_reset_observations(ann_engine->outputs);
    }
    return 0;
}

static int
_sml_ann_alloc_arrays_if_needed(struct sml_ann_engine *ann_engine)
{
    unsigned int inputs, outputs;
    int error;

    /* Already allocated */
    if (!ann_engine->first_run)
        return 0;

    inputs = sml_ann_variables_list_get_length(ann_engine->inputs);
    outputs = sml_ann_variables_list_get_length(ann_engine->outputs);

    while (!_sml_ann_can_alloc_memory_for_observations(inputs + outputs,
        ann_engine->required_observations,
        ann_engine->engine.obs_max_size))
        ann_engine->required_observations /= 2;

    if (!ann_engine->required_observations) {
        sml_critical("Can not alloc %d bytes for observations",
            ann_engine->engine.obs_max_size);
        return -ENOMEM;
    }

    error = sml_ann_variables_list_realloc_observations_array(
        ann_engine->inputs, ann_engine->required_observations);
    if (error) {
        sml_critical("Could not alloc the input observation arrays");
        return error;
    }

    error = sml_ann_variables_list_realloc_observations_array(
        ann_engine->outputs, ann_engine->required_observations);
    if (error) {
        sml_critical("Could not alloc the output observation arrays");
        return error;
    }

    return 0;
}

static struct sml_ann_bridge *
_sml_ann_get_best_ann_for_latest_observations(struct sml_ann_engine *ann_engine)
{
    struct sol_ptr_vector *anns;
    struct sml_ann_bridge *iann, *best_ann;
    unsigned int i;
    float sum_iann, sum_best, distance, min;

    anns = sml_cache_get_elements(ann_engine->anns_cache);
    best_ann = NULL;
    min = FLT_MAX;
    sum_best = FLT_MAX;

    sml_debug("Selecting best ANN. Neural networks size: %d",
        sol_ptr_vector_get_len(anns));

    SOL_PTR_VECTOR_FOREACH_IDX (anns, iann, i) {
        sml_debug("Neural network:%d", i);
        if (!sml_ann_bridge_is_trained(iann)) {
            sml_debug("ANN is not trained, skip");
            continue;
        }
        distance = sml_ann_bridge_confidence_intervals_distance_sum(iann,
            ann_engine->inputs);
        sml_debug("ANN:%d distance:%f", i, distance);
        if (distance < min) {
            min = distance;
            best_ann = iann;
            sum_best = sml_ann_bridge_get_confidence_interval_sum(iann);
        } else if (distance == min) {
            sum_iann = sml_ann_bridge_get_confidence_interval_sum(iann);
            if (sum_iann < sum_best) {
                best_ann = iann;
                sum_best = sum_iann;
            }
        }
    }

    if (best_ann)
        sml_cache_hit(ann_engine->anns_cache, best_ann);
    return best_ann;
}

static void
_sml_ann_engine_free(struct sml_engine *engine)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    uint16_t i;
    struct sml_variable *var;

    sml_ann_variable_list_free(ann_engine->inputs, true);
    sml_ann_variable_list_free(ann_engine->outputs, true);

    sml_cache_free(ann_engine->anns_cache);
    sol_ptr_vector_clear(&ann_engine->pending_remove);

    SOL_PTR_VECTOR_FOREACH_IDX (&ann_engine->pending_add, var, i)
        sml_ann_variable_free(var);

    sol_ptr_vector_clear(&ann_engine->pending_add);
    sol_vector_clear(&ann_engine->activation_functions);
    free(ann_engine);
}

API_EXPORT bool
sml_ann_set_training_algorithm(struct sml_object *sml,
    enum sml_ann_training_algorithm algorithm)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;

    ann_engine->train_algorithm = algorithm;
    return true;
}

API_EXPORT bool
sml_ann_set_training_epochs(struct sml_object *sml,
    unsigned int training_epochs)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;

    ann_engine->train_epochs = training_epochs;
    return true;
}

API_EXPORT bool
sml_ann_set_desired_error(struct sml_object *sml, float desired_error)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;

    ann_engine->train_error = desired_error;
    return true;
}

static int
_sml_ann_pseudorehearsal_train(struct sml_ann_engine *ann_engine,
    struct sml_ann_bridge *iann)
{

    int r;
    unsigned int total_size, diff, i, j, len, old_size;
    struct sml_variable *var;

    len = sml_ann_variables_list_get_length(ann_engine->inputs);
    old_size = ann_engine->required_observations;
    total_size = ann_engine->required_observations * EXPAND_FACTOR;
    diff = total_size - ann_engine->required_observations;

    if (!sml_ann_bridge_is_trained(iann)) {
        sml_debug("ANN is not trained yet, training with the usual way");
        return _sml_ann_train(ann_engine, iann,
            ann_engine->required_observations);
    }

    if (sml_ann_bridge_get_error(iann, ann_engine->inputs, ann_engine->outputs,
        ann_engine->required_observations) <= ann_engine->train_error) {
        sml_debug("Not retraining the ANN. Error is good enought");
        return 0;
    }

    if ((r = sml_ann_variables_list_realloc_observations_array(
            ann_engine->inputs, total_size))) {
        sml_debug("Could not expand the input array");
        return r;
    }

    if ((r = sml_ann_variables_list_realloc_observations_array(
            ann_engine->outputs, total_size))) {
        sml_debug("Could not expand the output array");
        return r;
    }

    //Generate inputs
    for (i = 0; i < len; i++) {
        var = sml_ann_variables_list_index(ann_engine->inputs, i);
        sml_ann_variable_fill_with_random_values(var, diff);
    }

    //Predict the outputs
    for (i = 0, j = ann_engine->required_observations; i < diff; i++, j++) {
        sml_ann_bridge_predict_output_by_index(iann, ann_engine->inputs,
            ann_engine->outputs, j);
    }

    //Now train!
    if ((r = _sml_ann_train(ann_engine, iann, total_size))) {
        sml_debug("Could not retrain the ANN!");
        return r;
    }

    len = sml_ann_variables_list_get_length(ann_engine->inputs);
    for (i = 0; i < len; i++) {
        var = sml_ann_variables_list_index(ann_engine->inputs, i);
        sml_ann_variable_set_observations_array_index(var, old_size);
    }

    //reset values
    sml_ann_variables_list_realloc_observations_array(ann_engine->inputs,
        old_size);
    sml_ann_variables_list_realloc_observations_array(ann_engine->outputs,
        old_size);
    return 0;
}

static int
_sml_ann_store_observations(struct sml_ann_engine *ann_engine)
{
    struct sol_ptr_vector *anns;
    struct sml_ann_bridge *iann, *to_train;
    unsigned int hits, i, input_len;
    int r;
    bool use_common_pool;

    if (ann_engine->engine.learn_disabled) {
        sml_debug("Learn is disabled, not storing values");
        return 0;
    }

    use_common_pool = true;
    to_train = NULL;
    if (!ann_engine->use_pseudorehearsal) {
        anns = sml_cache_get_elements(ann_engine->anns_cache);
        input_len = sml_ann_variables_list_get_length(ann_engine->inputs);
        sml_debug("Total ANNS:%d", sol_ptr_vector_get_len(anns));
        SOL_PTR_VECTOR_FOREACH_IDX (anns, iann, i) {
            if (!sml_ann_bridge_is_trained(iann)) {
                sml_debug("ANN is not trained, skip");
                to_train = iann;
                continue;
            }
            hits = sml_ann_bridge_inputs_in_confidence_interval_hits(iann,
                ann_engine->inputs);
            if (hits == input_len) {
                use_common_pool = false;
                sml_ann_bridge_add_observation(iann, ann_engine->inputs,
                    ann_engine->outputs);
                sml_debug("Adding current observation to ANN:%d", i);
            }
        }
    }

    if (use_common_pool) {
        sml_debug("Storing observation in the common pool %d",
            _sml_ann_get_observations_length(ann_engine));
        sml_ann_variables_list_add_last_value_to_observation(
            ann_engine->inputs);
        sml_ann_variables_list_add_last_value_to_observation(
            ann_engine->outputs);

        if (_sml_ann_get_observations_length(ann_engine) ==
            ann_engine->required_observations) {
            if (!to_train) {
                iann = _sml_ann_new(ann_engine, &r);
                if (!iann) {
                    sml_critical("Could not create a new neural network");
                    return r;
                }
            } else {
                iann = to_train;
                sml_debug("Trying to train a previous created ANN.");
            }

            if (!ann_engine->use_pseudorehearsal) {
                if ((r = _sml_ann_train(ann_engine, iann,
                        ann_engine->required_observations))) {
                    sml_critical("Could not train the neural network");
                    sml_cache_remove(ann_engine->anns_cache, iann);
                    return r;
                }
            } else {
                if ((r = _sml_ann_pseudorehearsal_train(ann_engine, iann))) {
                    sml_critical("Could not train the neural network");
                    return r;
                }
            }

            if (sml_ann_bridge_is_trained(iann)) {
                sml_debug("ANN is trained, reseting variable observations.");
                sml_ann_variables_list_reset_observations(ann_engine->inputs);
                sml_ann_variables_list_reset_observations(ann_engine->outputs);
            }
        }
    }
    return 0;
}

static int
_sml_ann_process(struct sml_engine *engine)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    bool r, significant_changes, should_act;
    int error = 0;
    struct sml_ann_bridge *iann;
    struct sml_variables_list *changed;

    if (!ann_engine->engine.read_state_cb) {
        sml_critical("There is not read callback registered");
        return -EINVAL;
    }

    if ((error = _sml_ann_alloc_arrays_if_needed(ann_engine))) {
        sml_critical("Could not alloc observation arrays123! %d", error);
        return error;
    }

    /* Changes the ANN layout if variables were added/removed */
    if ((error = _sml_ann_change_ann_layout_if_needed(ann_engine))) {
        sml_critical("Could not change the ANN layout");
        return error;
    }

    if (!ann_engine->engine.read_state_cb(
        (struct sml_object *)&ann_engine->engine,
        ann_engine->engine.read_state_cb_data)) {
        sml_debug("Read cb returned false");
        return -EAGAIN;
    }

    should_act = false;
    if (ann_engine->first_run) {
        sml_ann_variables_list_set_current_value_as_stable(
            ann_engine->inputs);
        sml_ann_variables_list_set_current_value_as_stable(
            ann_engine->outputs);
        ann_engine->first_run = false;
    } else {
        significant_changes =
            _sml_ann_variable_list_has_significant_changes(ann_engine->inputs);
        if (significant_changes)
            ann_engine->engine.output_state_changed_called = false;
        significant_changes |=
            _sml_ann_variable_list_has_significant_changes(ann_engine->outputs);
        if (significant_changes) {
            ann_engine->engine.hits = 0;
            sml_ann_variables_list_set_current_value_as_stable(
                ann_engine->inputs);
            sml_ann_variables_list_set_current_value_as_stable(
                ann_engine->outputs);
        }
    }

    if (ann_engine->engine.hits == ann_engine->engine.stabilization_hits) {
        ann_engine->engine.hits = 0;
        should_act = true;
        _sml_ann_store_observations(ann_engine);
        sml_debug("Reads are stabilized!");
    } else
        ann_engine->engine.hits++;

    if (ann_engine->engine.output_state_changed_cb &&
        should_act && !ann_engine->engine.output_state_changed_called) {

        if (!ann_engine->use_pseudorehearsal)
            iann = _sml_ann_get_best_ann_for_latest_observations(ann_engine);
        else
            iann = sml_cache_get_element(ann_engine->anns_cache, 0);

        if (iann && sml_ann_bridge_is_trained(iann)) {
            sml_debug("Trying to predict output");
            r = sml_ann_bridge_predict_output(iann, ann_engine->inputs,
                ann_engine->outputs);
            if (r) {
                changed = _sml_ann_output_has_significant_changes(
                    ann_engine->outputs);
                if (changed && sml_ann_variables_list_get_length(changed)) {
                    ann_engine->engine.output_state_changed_cb(
                        (struct sml_object *)&ann_engine->engine, changed,
                        ann_engine->engine.output_state_changed_cb_data);
                    if (should_act)
                        ann_engine->engine.output_state_changed_called = true;
                } else
                    sml_debug("Not calling changed cb.");
                if (changed)
                    sml_ann_variable_list_free(changed, false);
            } else
                sml_critical("Could not predict the output");
        } else
            sml_critical("Could not select the best ann");
    }

    return error;
}

static bool
_sml_ann_predict(struct sml_engine *engine)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    struct sml_ann_bridge *iann;

    if (!ann_engine->use_pseudorehearsal)
        iann = _sml_ann_get_best_ann_for_latest_observations(ann_engine);
    else
        iann = sml_cache_get_element(ann_engine->anns_cache, 0);

    if (!iann || !sml_ann_bridge_is_trained(iann)) {
        sml_critical("Could not select the best ann");
        return false;
    }

    if (!sml_ann_bridge_predict_output(iann, ann_engine->inputs,
        ann_engine->outputs)) {
        sml_critical("Could not predict the output");
        return false;
    }

    return true;
}

static bool
_sml_ann_save(struct sml_engine *engine, const char *path)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    struct sml_ann_bridge *iann;
    char ann_path[SML_PATH_MAX], cfg_path[SML_PATH_MAX];
    struct sol_ptr_vector *anns;
    uint16_t i, ann_idx;
    bool exists;

    if (!sml_cache_get_size(ann_engine->anns_cache)) {
        sml_critical("Could not save the neural network." \
            "The neural network is NULL");
        return false;
    }

    exists = file_exists(path);
    if (exists && !is_dir(path)) {
        sml_critical("Failed to save sml: %s is not a directory\n", path);
        return false;
    } else if (!exists && !create_dir(path)) {
        sml_critical("Could not create the directory:%s", path);
        return false;
    }

    if (!clean_dir(path, ANN_FILE_PREFIX) ||
        !clean_dir(path, CFG_FILE_PREFIX)) {
        sml_critical("Failed to clear %s to save sml\n", path);
        return false;
    }

    if (ann_engine->use_pseudorehearsal) {
        snprintf(ann_path, sizeof(ann_path), "%s/%s.%s", path,
            ANN_PSEUDOREHEARSAL_PREFIX, ANN_FILE_EXTENSION);
        iann = sml_cache_get_element(ann_engine->anns_cache, 0);
        if (iann && sml_ann_bridge_is_trained(iann)) {
            if (!sml_ann_bridge_save_with_no_cfg(iann, ann_path)) {
                sml_critical("Could not save the ANN at:%s", ann_path);
                return false;
            }
        } else
            sml_debug("Not saving ANN. Not trained or does not exist yet.");
    } else {
        anns = sml_cache_get_elements(ann_engine->anns_cache);
        ann_idx = 0;
        SOL_PTR_VECTOR_FOREACH_IDX (anns, iann, i) {
            if (!sml_ann_bridge_is_trained(iann)) {
                sml_debug("Not saving untrained ANN.");
                continue;
            }
            snprintf(ann_path, sizeof(ann_path), "%s/%s%d.%s",
                path, ANN_FILE_PREFIX, ann_idx, ANN_FILE_EXTENSION);
            snprintf(cfg_path, sizeof(cfg_path), "%s/%s%d.%s",
                path, CFG_FILE_PREFIX, ann_idx, CFG_FILE_EXTENSION);
            ann_idx++;
            if (!sml_ann_bridge_save(iann, ann_path, cfg_path)) {
                sml_critical("Could not save the neural network at:%s", path);
                continue;
            }
        }
    }
    sml_debug("Neural network saved at:%s", path);
    return true;
}

API_EXPORT bool
sml_ann_set_activation_function_candidates(struct sml_object *sml,
    enum sml_ann_activation_function *functions,
    unsigned int size)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;
    unsigned int i;
    enum sml_ann_activation_function *func;

    for (i = 0; i < size; i++) {
        func = sol_vector_append(&ann_engine->activation_functions);
        if (!func) {
            sml_critical("Could append an activation function");
            continue;
        }
        *func = functions[i];
    }
    return true;
}

API_EXPORT bool
sml_ann_set_max_neurons(struct sml_object *sml, unsigned int max_neurons)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;

    ann_engine->max_neurons = max_neurons;
    return true;
}

API_EXPORT bool
sml_ann_set_candidate_groups(struct sml_object *sml,
    unsigned int candidate_groups)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;;

    ann_engine->candidate_groups = candidate_groups;
    return true;
}

static bool
_sml_ann_load(struct sml_engine *engine, const char *path)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    struct sml_ann_bridge *iann;
    char ann_path[SML_PATH_MAX], cfg_path[SML_PATH_MAX];
    unsigned int i = 0;

    if (sml_cache_get_size(ann_engine->anns_cache)) {
        sml_cache_clear(ann_engine->anns_cache);
        sml_warning("Destroying a previously created neural network");
    }

    if (!is_dir(path)) {
        sml_critical("Failed to load sml in directory %s\n", path);
        return false;
    }

    if (ann_engine->use_pseudorehearsal) {
        snprintf(ann_path, sizeof(ann_path), "%s/%s.%s", path,
            ANN_PSEUDOREHEARSAL_PREFIX, ANN_FILE_EXTENSION);
        iann = sml_ann_bridge_load_from_file_with_no_cfg(ann_path);
        if (!iann) {
            sml_critical("Could not load the ann at:%s", ann_path);
            return false;
        }
        if (!sml_cache_put(ann_engine->anns_cache, iann)) {
            sml_critical(
                "Could not add the struct sml_ann_bridge to the cache");
            sml_ann_bridge_free(iann);
            return false;
        }
    } else {
        while (1) {
            snprintf(ann_path, sizeof(ann_path), "%s/%s%d.%s", path,
                ANN_FILE_PREFIX, i, ANN_FILE_EXTENSION);
            snprintf(cfg_path, sizeof(cfg_path), "%s/%s%d.%s",
                path, CFG_FILE_PREFIX, i, CFG_FILE_EXTENSION);
            if (!is_file(ann_path)) {
                sml_warning("The path:%s is not an ANN file", ann_path);
                break;
            } else if (!is_file(cfg_path)) {
                sml_warning("The path:%s is not a cfg file",
                    cfg_path);
                break;
            }

            iann = sml_ann_bridge_load_from_file(ann_path, cfg_path);
            if (!iann)
                break;
            if (!sml_cache_put(ann_engine->anns_cache, iann)) {
                sml_ann_bridge_free(iann);
                sml_cache_clear(ann_engine->anns_cache);
                return false;
            }
            i++;
        }
    }
    sml_debug("Neural network loaded");
    return true;
}

static struct sml_variable *
_sml_ann_new_input(struct sml_engine *engine, const char *name)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return _sml_ann_add_variable(ann_engine, name, true);
}

static struct sml_variable *
_sml_ann_new_output(struct sml_engine *engine, const char *name)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return _sml_ann_add_variable(ann_engine, name, false);
}

static bool
_sml_ann_remove_variable(struct sml_engine *engine, struct sml_variable *var)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    if (sml_cache_get_size(ann_engine->anns_cache)) {
        if (sol_ptr_vector_append(&ann_engine->pending_remove, var)) {
            sml_critical("Could not add the variable to the pending remove" \
                " list");
            return false;
        }
        return true;
    }
    return _sml_ann_remove_variable_from_lists(ann_engine, var);
}

static struct sml_variable *
_sml_ann_get_input(struct sml_engine *engine, const char *name)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return _sml_ann_find_variable_by_name(ann_engine, name, true);
}

static struct sml_variable *
_sml_ann_get_output(struct sml_engine *engine, const char *name)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return _sml_ann_find_variable_by_name(ann_engine, name, false);
}

static struct sml_variables_list *
_sml_ann_get_input_list(struct sml_engine *engine)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return ann_engine->inputs;
}

static struct sml_variables_list *
_sml_ann_get_output_list(struct sml_engine *engine)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;

    return ann_engine->outputs;
}

static int
_sml_ann_variable_set_enabled(struct sml_engine *engine,
    struct sml_variable *var, bool enabled)
{
    return sml_ann_variable_set_enabled(var, enabled);
}

static void
_sml_ann_print_variables_list(struct sml_variables_list *list)
{
    struct sml_variable *var;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    uint16_t i, len;

    len = sml_ann_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        var = sml_ann_variables_list_index(list, i);
        if (sml_ann_variable_get_name(var, var_name, sizeof(var_name))) {
            sml_warning("Failed to get name of var %p", var);
            continue;
        }
        sml_debug("\t%s: %g", var_name, sml_ann_variable_get_value(var));
    }
}

static void
_sml_ann_print_debug(struct sml_engine *engine, bool full)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)engine;
    struct sol_ptr_vector *elems;
    struct sml_ann_bridge *ann;
    uint16_t i;

    sml_debug("Inputs (%d) {",
        sml_ann_variables_list_get_length(ann_engine->inputs));
    _sml_ann_print_variables_list(ann_engine->inputs);
    sml_debug("}");
    sml_debug("Outputs (%d) {",
        sml_ann_variables_list_get_length(ann_engine->outputs));
    _sml_ann_print_variables_list(ann_engine->outputs);
    sml_debug("}");

    if (full) {
        sml_debug("Required observations: %d",
            ann_engine->required_observations);
        sml_debug("Train epochs: %d", ann_engine->train_epochs);
        sml_debug("Train error: %g", ann_engine->train_error);
        sml_debug("Max neurons: %d", ann_engine->max_neurons);
        sml_debug("Candidate groups: %d", ann_engine->candidate_groups);
        sml_debug("Observations max size: %d",
            ann_engine->engine.obs_max_size);

        sml_debug("ANNs (%d) {",
            sml_cache_get_size(ann_engine->anns_cache));
        elems = sml_cache_get_elements(ann_engine->anns_cache);
        SOL_PTR_VECTOR_FOREACH_IDX (elems, ann, i) {
            sml_debug("{");
            sml_ann_bridge_print_debug(ann);
            sml_debug("}");
        }
        sml_debug("}");
#ifdef Debug
        sml_debug("Total ANNs created: (%d)",
            sml_cache_get_total_elements_inserted(ann_engine->anns_cache));
#endif
    }
}

static void
_sml_ann_cache_element_free(void *element, void *data)
{
    sml_ann_bridge_free(element);
}

API_EXPORT struct sml_object *
sml_ann_new(void)
{
    struct sml_ann_engine *ann_engine = calloc(1,
        sizeof(struct sml_ann_engine));

    if (!ann_engine) {
        sml_critical("Could not create a sml");
        return NULL;
    }
    ann_engine->inputs = sml_ann_variable_list_new();

    if (!ann_engine->inputs) {
        sml_critical("Could not create the input variable list");
        goto err_inputs;
    }
    ann_engine->outputs = sml_ann_variable_list_new();
    if (!ann_engine->outputs) {
        sml_critical("Could not create the output variable list");
        goto err_outputs;
    }

    ann_engine->anns_cache = sml_cache_new(DEFAULT_CACHE_SIZE,
        _sml_ann_cache_element_free, NULL);

    if (!ann_engine->anns_cache) {
        sml_critical("Could not create the ANN cache");
        goto err_cache;
    }

    ann_engine->train_algorithm = SML_ANN_TRAINING_ALGORITHM_QUICKPROP;
    ann_engine->train_epochs = DEFAULT_EPOCHS;
    ann_engine->train_error = DEFAULT_DESIRED_ERROR;
    ann_engine->candidate_groups = DEFAULT_CANDIDATE_GROUPS;
    ann_engine->required_observations = INITIAL_REQUIRED_OBSERVATIONS;
    ann_engine->first_run = true;
    ann_engine->use_pseudorehearsal = true;
    sol_vector_init(&ann_engine->activation_functions,
        sizeof(enum sml_ann_activation_function));
    sol_ptr_vector_init(&ann_engine->pending_remove);
    sol_ptr_vector_init(&ann_engine->pending_add);

    ann_engine->engine.free = _sml_ann_engine_free;
    ann_engine->engine.process = _sml_ann_process;
    ann_engine->engine.predict = _sml_ann_predict;
    ann_engine->engine.save = _sml_ann_save;
    ann_engine->engine.load = _sml_ann_load;
    ann_engine->engine.get_input_list = _sml_ann_get_input_list;
    ann_engine->engine.get_output_list = _sml_ann_get_output_list;
    ann_engine->engine.new_input = _sml_ann_new_input;
    ann_engine->engine.new_output = _sml_ann_new_output;
    ann_engine->engine.get_input = _sml_ann_get_input;
    ann_engine->engine.get_output = _sml_ann_get_output;
    ann_engine->engine.get_name = sml_ann_variable_get_name;
    ann_engine->engine.set_value = sml_ann_variable_set_value;
    ann_engine->engine.get_value = sml_ann_variable_get_value;
    ann_engine->engine.variable_set_enabled = _sml_ann_variable_set_enabled;
    ann_engine->engine.variable_is_enabled = sml_ann_variable_is_enabled;
    ann_engine->engine.remove_variable = _sml_ann_remove_variable;
    ann_engine->engine.variables_list_get_length =
        sml_ann_variables_list_get_length;
    ann_engine->engine.variables_list_index = sml_ann_variables_list_index;
    ann_engine->engine.variable_set_range = sml_ann_variable_set_range;
    ann_engine->engine.variable_get_range = sml_ann_variable_get_range;
    ann_engine->engine.print_debug = _sml_ann_print_debug;
    ann_engine->engine.magic_number = ANN_MAGIC;

    return (struct sml_object *)&ann_engine->engine;
err_cache:
    sml_ann_variable_list_free(ann_engine->outputs, true);
err_outputs:
    sml_ann_variable_list_free(ann_engine->inputs, true);
err_inputs:
    free(ann_engine);
    return NULL;
}

API_EXPORT bool
sml_is_ann(struct sml_object *sml)
{
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;

    ON_NULL_RETURN_VAL(sml, false);
    return ann_engine->engine.magic_number == ANN_MAGIC;
}

API_EXPORT bool
sml_ann_supported(void)
{
    return true;
}

API_EXPORT bool
sml_ann_set_cache_max_size(struct sml_object *sml, unsigned int max_size)
{
    if (!sml_is_ann(sml))
        return false;
    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;
    sml_debug("Setting ann cache to max size:%d", max_size);
    return sml_cache_set_max_size(ann_engine->anns_cache, max_size);
}

API_EXPORT bool
sml_ann_set_initial_required_observations(struct sml_object *sml,
    unsigned int required_observations)
{
    if (!sml_is_ann(sml))
        return false;

    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;
    if (!ann_engine->first_run) {
        sml_warning("Initial required observation must be set before first call"
            " of sml_process");
        return false;
    }
    ann_engine->required_observations = required_observations;
    return true;
}

API_EXPORT bool
sml_ann_use_pseudorehearsal_strategy(struct sml_object *sml,
    bool use_pseudorehearsal)
{
    if (!sml_is_ann(sml))
        return false;

    struct sml_ann_engine *ann_engine = (struct sml_ann_engine *)sml;
    if (!ann_engine->first_run) {
        sml_warning("Pseudorehearsal strategy can only be set before the first"
            " call of sml_process");
        return false;
    }
    ann_engine->use_pseudorehearsal = use_pseudorehearsal;
    return true;
}
