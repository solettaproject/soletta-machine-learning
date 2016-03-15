/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sml_ann_bridge.h"
#include "sml_ann_variable_list.h"
#include <sml_log.h>
#include <sml_util.h>
#include <stdlib.h>
#include <float.h>
#include <stdio.h>
#include <math.h>
#include <floatfann.h>
#include <errno.h>

#define REPORTS_BETWEEN_EPOCHS (100)
#define MAX_NEURONS_MULTIPLIER (5)
#define MAX_EPOCHS (500)

struct sml_ann_bridge {
    bool trained;
    float last_train_error;
    struct fann *ann;
    struct sol_vector confidence_intervals;
    struct fann_train_data *observations;
    unsigned int required_observations;
    unsigned int observation_idx;
    unsigned int max_neurons;

    float ci_length_sum;
};

typedef struct _Confidence_Interval {
    float lower_limit;
    float upper_limit;
} Confidence_Interval;

static struct sml_ann_bridge *
_sml_ann_bridge_new(struct fann *ann, bool trained)
{
    struct sml_ann_bridge *iann = calloc(1, sizeof(struct sml_ann_bridge));

    if (!iann) {
        sml_critical("Could not create an struct sml_ann_bridge");
        return NULL;
    }
    sol_vector_init(&iann->confidence_intervals, sizeof(Confidence_Interval));
    iann->ann = ann;
    iann->trained = trained;
    iann->last_train_error = NAN;
    if (iann->trained)
        fann_set_training_algorithm(iann->ann, FANN_TRAIN_INCREMENTAL);
    return iann;
}

static float
_sml_ann_bridge_get_variable_value(struct sml_variables_list *list,
    uint16_t i, bool scale)
{
    struct sml_variable *var = sml_ann_variables_list_index(list, i);
    float v = sml_ann_variable_get_value(var);

    if (isnan(v))
        sml_ann_variable_get_range(var, &v, NULL);
    if (scale)
        return sml_ann_variable_scale_value(var, v);
    return v;
}

static bool
_sml_ann_bridge_calculate_confidence_interval(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    unsigned int observations)
{
    unsigned int i, j, inputs_len;
    float mean, sd, value;
    struct sml_variable *var;
    Confidence_Interval *ci;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];

    sml_debug("Calculating confidence interval");
    inputs_len = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < inputs_len; i++) {
        mean = sd = 0.0;
        var = sml_ann_variables_list_index(inputs, i);
        ci = sol_vector_append(&iann->confidence_intervals);
        if (!ci) {
            sml_critical("Could not alloc the Confidence_Interval");
            goto err_exit;
        }

        for (j = 0; j < observations; j++) {
            value = sml_ann_variable_get_value_by_index(var, j);
            if (isnan(value))
                sml_ann_variable_get_range(var, &value, NULL);
            mean += value;
        }
        mean /= observations;

        //Now the standard deviation
        for (j = 0; j < observations; j++) {
            value = sml_ann_variable_get_value_by_index(var, j);
            if (isnan(value))
                sml_ann_variable_get_range(var, &value, NULL);
            sd += pow(value - mean, 2.0);
        }
        sd /= observations;
        sd = sqrt(sd);

        //Confidence interval of 95%
        ci->lower_limit = mean - (1.96 * (sd / sqrt(observations)));
        ci->upper_limit = mean + (1.96 * (sd / sqrt(observations)));
        iann->ci_length_sum += (ci->upper_limit - ci->lower_limit);

        if (sml_ann_variable_get_name(var, var_name, sizeof(var_name))) {
            sml_warning("Failed to get variable name for %p", var);
            continue;
        }

        sml_debug("Variable:%s mean:%f sd:%f lower:%f upper:%f",
            var_name, mean, sd, ci->lower_limit,
            ci->upper_limit);
    }

    return true;
err_exit:
    sol_vector_clear(&iann->confidence_intervals);
    return false;
}

static enum fann_activationfunc_enum
_sml_ann_bridge_translate_to_fann_activation_func(
    enum sml_ann_activation_function activation)
{
    switch (activation) {
    case SML_ANN_ACTIVATION_FUNCTION_SIGMOID:
        return FANN_SIGMOID;
    case SML_ANN_ACTIVATION_FUNCTION_SIGMOID_SYMMETRIC:
        return FANN_SIGMOID_SYMMETRIC;
    case SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN:
        return FANN_GAUSSIAN;
    case SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN_SYMMETRIC:
        return FANN_GAUSSIAN_SYMMETRIC;
    case SML_ANN_ACTIVATION_FUNCTION_ELLIOT:
        return FANN_ELLIOT;
    case SML_ANN_ACTIVATION_FUNCTION_ELLIOT_SYMMETRIC:
        return FANN_ELLIOT_SYMMETRIC;
    case SML_ANN_ACTIVATION_FUNCTION_COS:
        return FANN_COS;
    case SML_ANN_ACTIVATION_FUNCTION_COS_SYMMETRIC:
        return FANN_COS_SYMMETRIC;
    case SML_ANN_ACTIVATION_FUNCTION_SIN:
        return FANN_SIN;
    default:
        return FANN_SIN_SYMMETRIC;
    }
}

static enum fann_train_enum
_sml_ann_bridge_translate_to_fann_train_enum(
    enum sml_ann_training_algorithm train)
{
    switch (train) {
    case SML_ANN_TRAINING_ALGORITHM_QUICKPROP:
        return FANN_TRAIN_QUICKPROP;
    default:
        return FANN_TRAIN_RPROP;
    }
}

static bool
_sml_ann_bridge_translate_to_fann_activation_vector(
    struct sol_vector *sml_funcs, struct sol_vector *fann_funcs)
{
    uint16_t i;
    enum sml_ann_activation_function *sml_func;
    enum fann_activationfunc_enum *fann_func;

    SOL_VECTOR_FOREACH_IDX (sml_funcs, sml_func, i) {
        fann_func = sol_vector_append(fann_funcs);
        if (!fann_func)
            return false;
        *fann_func = _sml_ann_bridge_translate_to_fann_activation_func(
            *sml_func);
    }
    return true;
}

static void
_sml_ann_bridge_fill_train_data_array(struct sml_variables_list *inputs,
    struct sml_variables_list *outputs,
    struct fann_train_data *train_data,
    unsigned int observations)
{
    uint16_t i, j, size;
    struct sml_variable *var;
    float value, min;

    size = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(inputs, i);
        for (j = 0; j < observations; j++) {
            value = sml_ann_variable_get_value_by_index(var, j);
            if (isnan(value) || !sml_ann_variable_is_enabled(var)) {
                sml_ann_variable_get_range(var, &min, NULL);
                value = min;
            }
            train_data->input[j][i] =
                sml_ann_variable_scale_value(var, value);
        }
    }

    size = sml_ann_variables_list_get_length(outputs);
    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(outputs, i);
        for (j = 0; j < observations; j++) {
            value = sml_ann_variable_get_value_by_index(var, j);
            if (isnan(value) || !sml_ann_variable_is_enabled(var)) {
                sml_ann_variable_get_range(var, &min, NULL);
                value = min;
            }
            train_data->output[j][i] =
                sml_ann_variable_scale_value(var, value);
        }
    }
}

static int
_sml_really_train(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs,
    float *err, unsigned int required_observations,
    unsigned int max_neurons,
    float desired_train_error)
{

    struct fann_train_data *train_data;
    unsigned int in_size, out_size;
    float train_error;

    fann_randomize_weights(iann->ann, -0.2, 0.2);

    in_size = sml_ann_variables_list_get_length(inputs);
    out_size = sml_ann_variables_list_get_length(outputs);

    train_data = fann_create_train(required_observations, in_size,
        out_size);
    if (!train_data) {
        sml_critical("Could not create the train data");
        return -ENOMEM;
    }

    _sml_ann_bridge_fill_train_data_array(inputs, outputs, train_data,
        required_observations);

    sml_debug("Observations size: %d", required_observations);

    if (!max_neurons)
        max_neurons = (in_size + out_size) +
            ((in_size + out_size) * MAX_NEURONS_MULTIPLIER);
    iann->max_neurons = max_neurons;
    fann_shuffle_train_data(train_data);
    if (!iann->trained) {
        fann_cascadetrain_on_data(iann->ann, train_data,
            max_neurons,
            REPORTS_BETWEEN_EPOCHS, desired_train_error);
    } else {
        fann_train_on_data(iann->ann, train_data, MAX_EPOCHS,
            REPORTS_BETWEEN_EPOCHS, desired_train_error);
    }

    train_error = fann_get_MSE(iann->ann);
    sml_debug("MSE error on test data: %f\n", train_error);
    fann_destroy_train(train_data);
    *err = train_error;
    return 0;
}

static int
_sml_ann_bridge_setup_ci_and_observations(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    unsigned int required_observations,
    bool use_pseudorehearsal)
{
    if (use_pseudorehearsal)
        return 0;

    if (!_sml_ann_bridge_calculate_confidence_interval(iann, inputs,
        required_observations))
        return -ENOMEM;

    iann->required_observations = required_observations;
    iann->observations =
        fann_create_train(iann->required_observations,
        fann_get_num_input(iann->ann),
        fann_get_num_output(iann->ann));
    if (!iann->observations) {
        sml_critical("Could not create the observations array for"      \
            " retraining");
        return -ENOMEM;
    }
    return 0;
}

int
sml_ann_bridge_train(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs, float desired_train_error,
    unsigned int required_observations,
    unsigned int max_neurons,
    unsigned int *required_observations_suggestion,
    bool use_pseudorehearsal)
{
    float train_error;
    int error;

    if ((error = _sml_really_train(iann, inputs, outputs, &train_error,
            required_observations, max_neurons,
            desired_train_error)))
        return error;

    if (train_error <= desired_train_error) {
        iann->trained = true;
        sml_debug("Error is good enough. Desired:%f current error:%f",
            desired_train_error, train_error);
    } else if (!isnan(iann->last_train_error) &&
        iann->last_train_error < train_error) {
        /* Less data gave us better results, stop trying to increase
           the required number of observations. */
        required_observations /= 2;
        iann->trained = true;
        sml_debug("Decreasing the observations data set. Current error:%f" \
            " last error:%f", train_error,
            iann->last_train_error);
    } else {
        required_observations *= 2;
        sml_debug("We still need more that to train the ann. Current"   \
            " error:%f desired error:%f", train_error,
            desired_train_error);

    }
    iann->last_train_error = train_error;

    if (required_observations_suggestion)
        *required_observations_suggestion = required_observations;
    if (iann->trained) {
        error = sml_ann_bridge_consider_trained(iann, inputs,
            required_observations,
            use_pseudorehearsal);
    }
    return error;
}

unsigned int
sml_ann_bridge_inputs_in_confidence_interval_hits(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs)
{
    unsigned int hits = 0;
    uint16_t i, len;
    Confidence_Interval *ci;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    float v;

    len = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < len; i++) {
        v = _sml_ann_bridge_get_variable_value(inputs, i, false);
        ci = sol_vector_get(&iann->confidence_intervals, i);
        if (!ci) {
            sml_debug("Confidence interval for idx:%d is NULL!", i);
            continue;
        }

        if (sml_ann_variable_get_name(sml_ann_variables_list_index(inputs, i),
            var_name, sizeof(var_name))) {
            sml_warning("Failed to get variable name for %p",
                sml_ann_variables_list_index(inputs, i));
            continue;
        }

        sml_debug("variable:%s value:%f lower:%f upper:%f",
            var_name, v, ci->lower_limit, ci->upper_limit);
        if (v >= ci->lower_limit && v <= ci->upper_limit)
            hits++;
    }
    return hits;
}

float
sml_ann_bridge_confidence_intervals_distance_sum(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs)
{
    Confidence_Interval *ci;
    uint16_t i, len;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    float v, distance = 0.0;

    len = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < len; i++) {
        v = _sml_ann_bridge_get_variable_value(inputs, i, false);
        ci = sol_vector_get(&iann->confidence_intervals, i);
        if (v < ci->lower_limit)
            distance += fabsf(ci->lower_limit - v);
        else if (v > ci->upper_limit)
            distance += fabsf(ci->upper_limit - v);

        if (sml_ann_variable_get_name(sml_ann_variables_list_index(inputs, i),
            var_name, sizeof(var_name))) {
            sml_warning("Failed to get variable name for %p",
                sml_ann_variables_list_index(inputs, i));
            continue;
        }

        sml_debug("variable:%s value:%f lower:%f upper:%f distance:%f",
            var_name, v, ci->lower_limit, ci->upper_limit, distance);
    }
    return distance;
}

void
sml_ann_bridge_add_observation(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs)
{
    uint16_t i, len;

    if (!iann->observations) {
        sml_warning("The bridge observation vector is not created");
        return;
    }

    len = sml_ann_variables_list_get_length(inputs);
    for (i = 0; i < len; i++)
        iann->observations->input[iann->observation_idx][i] =
            _sml_ann_bridge_get_variable_value(inputs, i, true);

    len = sml_ann_variables_list_get_length(outputs);
    for (i = 0; i < len; i++)
        iann->observations->output[iann->observation_idx][i] =
            _sml_ann_bridge_get_variable_value(outputs, i, true);

    iann->observation_idx++;
    sml_debug("ANN:%p observation_idx:%d", iann, iann->observation_idx);
    if (iann->observation_idx == iann->required_observations) {
        sml_debug("Retraining the ANN !");
        fann_train_on_data(iann->ann, iann->observations, MAX_EPOCHS,
            REPORTS_BETWEEN_EPOCHS, iann->last_train_error);
        iann->observation_idx = 0;
    }
}

float
sml_ann_bridge_get_confidence_interval_sum(struct sml_ann_bridge *iann)
{
    return iann->ci_length_sum;
}

int
sml_ann_bridge_consider_trained(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    unsigned int observations,
    bool use_pseudorehearsal)
{
    int error = _sml_ann_bridge_setup_ci_and_observations(iann, inputs,
        observations,
        use_pseudorehearsal);

    if (error != 0)
        return error;
    iann->trained = true;
    fann_set_training_algorithm(iann->ann, FANN_TRAIN_INCREMENTAL);
    return 0;
}

static bool
_sml_ann_bridge_really_predict_output(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs,
    unsigned int idx,
    bool set_current_value)
{
    uint16_t size, i;
    fann_type *out, *in;
    struct sml_variable *var;
    float value;
    bool r = false;

    size = sml_ann_variables_list_get_length(inputs);
    in = malloc(sizeof(fann_type) * size);

    if (!in) {
        sml_critical("Could not alloc fann input vector");
        return r;
    }

    for (i = 0; i < size; i++) {
        var = sml_ann_variables_list_index(inputs, i);
        if (!set_current_value)
            value = sml_ann_variable_get_value_by_index(var, idx);
        else //the last observation
            value = sml_ann_variable_get_value(var);
        if (isnan(value))
            sml_ann_variable_get_range(var, &value, NULL);
        in[i] = sml_ann_variable_scale_value(var, value);
    }

    out = fann_run(iann->ann, in);

    if (out) {
        char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];

        size = sml_ann_variables_list_get_length(outputs);

        for (i = 0; i < size; i++) {
            var = sml_ann_variables_list_index(outputs, i);

            if (isnan(out[i]))
                sml_ann_variable_get_range(var, &value, NULL);
            else
                value = sml_ann_variable_descale_value(var, out[i]);

            if (!set_current_value)
                sml_ann_variable_set_value_by_index(var, value, idx);
            else
                sml_ann_variable_set_value(var, value);

            if (sml_ann_variable_get_name(var, var_name, sizeof(var_name))) {
                sml_warning("Failed to get variable name for %p", var);
                continue;
            }

            sml_debug("Predicted value:%f current value:%f variable:%s", value,
                sml_ann_variable_get_previous_value(var), var_name);
        }
        r = true;
    } else
        sml_critical("fann_run() returned NULL!");
    free(in);
    return r;
}

bool
sml_ann_bridge_predict_output_by_index(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs,
    unsigned int idx)
{
    return _sml_ann_bridge_really_predict_output(iann, inputs, outputs, idx,
        false);
}

bool
sml_ann_bridge_predict_output(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs, struct sml_variables_list *outputs)
{
    return _sml_ann_bridge_really_predict_output(iann, inputs, outputs, 0,
        true);
}

void
sml_ann_bridge_free(struct sml_ann_bridge *iann)
{
    fann_destroy_train(iann->observations);
    fann_destroy(iann->ann);
    sol_vector_clear(&iann->confidence_intervals);
    free(iann);
}

bool
sml_ann_bridge_is_trained(struct sml_ann_bridge *iann)
{
    return iann->trained;
}

struct sml_ann_bridge *
sml_ann_bridge_new(unsigned int inputs, unsigned int outputs,
    unsigned int candidate_groups, unsigned int epochs,
    enum sml_ann_training_algorithm train_algorithm,
    struct sol_vector *activation_functions, int *error_code)
{
    struct fann *ann;
    struct sml_ann_bridge *iann;
    struct sol_vector fann_funcs;
    bool err;

    if (!inputs || !outputs) {
        sml_critical("Inputs/Outputs size. Inputs:%d Outputs:%d", inputs,
            outputs);
        if (error_code)
            *error_code = -EINVAL;
        return NULL;
    }

    ann = fann_create_shortcut(2, inputs, outputs);

    if (!ann) {
        sml_critical("Could not create the neural network");
        if (error_code)
            *error_code = -ENOMEM;
        return NULL;
    }

    /* These activation functions might change during the cascade training. */
    fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
    fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);
    fann_set_training_algorithm(ann,
        _sml_ann_bridge_translate_to_fann_train_enum(
        train_algorithm));
    fann_set_train_error_function(ann, FANN_ERRORFUNC_LINEAR);

    if (activation_functions->len) {
        sol_vector_init(&fann_funcs, sizeof(enum fann_activationfunc_enum));
        err = _sml_ann_bridge_translate_to_fann_activation_vector(
            activation_functions, &fann_funcs);
        if (!err) {
            sml_critical("Could not convert to activation functions to " \
                "fann enum");
            goto err_exit;
        }
        fann_set_cascade_activation_functions(ann,
            fann_funcs.data,
            fann_funcs.len);
        sol_vector_clear(&fann_funcs);
    }

    fann_set_cascade_max_cand_epochs(ann, epochs);
    fann_set_cascade_max_out_epochs(ann, epochs);
    fann_set_cascade_num_candidate_groups(ann, candidate_groups);

    iann = _sml_ann_bridge_new(ann, false);
    if (!iann)
        goto err_exit;
    return iann;
err_exit:
    fann_destroy(ann);
    if (error_code)
        *error_code = ENOMEM;
    return NULL;
}

bool
sml_ann_bridge_save_with_no_cfg(struct sml_ann_bridge *iann,
    const char *ann_path)
{
    sml_debug("Saving ann at:%s", ann_path);
    if (fann_save(iann->ann, ann_path) < 0) {
        sml_critical("Could not save the ANN at:%s", ann_path);
        return false;
    }
    return true;
}

struct sml_ann_bridge *
sml_ann_bridge_load_from_file_with_no_cfg(const char *ann_path)
{
    struct fann *ann;
    struct sml_ann_bridge *iann;

    ann = fann_create_from_file(ann_path);

    if (!ann) {
        sml_critical("Could not load the ann from path:%s", ann_path);
        return NULL;
    }

    iann = _sml_ann_bridge_new(ann, true);
    if (!iann) {
        sml_critical("Could not create an struct sml_ann_bridge");
        fann_destroy(ann);
        return NULL;
    }
    return iann;
}

bool
sml_ann_bridge_save(struct sml_ann_bridge *iann, const char *ann_path,
    const char *cfg_path)
{
    uint16_t i;
    Confidence_Interval *ci;
    FILE *f;
    bool r = false;

    sml_debug("Saving ann at:%s and CFGs at:%s", ann_path, cfg_path);
    if (fann_save(iann->ann, ann_path) < 0)
        return false;

    f = fopen(cfg_path, "wb");
    if (!f) {
        sml_critical("Could not create the confidence intervals file");
        return false;
    }

    if (fwrite(&iann->confidence_intervals.len, sizeof(uint16_t), 1, f) < 1) {
        sml_critical("Could not write the confidence intervals len");
        goto exit;
    }

    SOL_VECTOR_FOREACH_IDX (&iann->confidence_intervals, ci, i) {
        if (fwrite(&ci->lower_limit, sizeof(float), 1, f) < 1) {
            sml_critical("Could not save the lower limit");
            goto exit;
        }
        if (fwrite(&ci->upper_limit, sizeof(float), 1, f) < 1) {
            sml_critical("Could not save the upper limit");
            goto exit;
        }
    }

    if (fwrite(&iann->max_neurons, sizeof(unsigned int), 1, f) < 1) {
        sml_critical("Could not save the max neurons");
        goto exit;
    }

    if (fwrite(&iann->required_observations, sizeof(unsigned int), 1, f) < 1) {
        sml_critical("Could not save the required observations");
        goto exit;
    }

    r = true;
exit:
    if (fclose(f) == EOF) {
        sml_critical("Could not correctly close the confidence intervals file");
        r = false;
    }
    if (!r) {
        delete_file(ann_path);
        delete_file(cfg_path);
    }
    return r;
}

struct sml_ann_bridge *
sml_ann_bridge_load_from_file(const char *ann_path, const char *cfg_path)
{
    struct fann *ann;
    struct sml_ann_bridge *iann;
    Confidence_Interval *ci;
    uint16_t count, i;
    FILE *f;

    sml_debug("Load ann:%s and CI:%s", ann_path, cfg_path);

    ann = fann_create_from_file(ann_path);
    if (!ann)
        return NULL;

    iann = _sml_ann_bridge_new(ann, true);
    if (!iann) {
        fann_destroy(ann);
        return NULL;
    }

    f = fopen(cfg_path, "rb");
    if (!f) {
        sml_critical("Could not open file:%s", cfg_path);
        goto err_exit;
    }

    if (fread(&count, sizeof(uint16_t), 1, f) < 1) {
        sml_critical("Could not read the confidence interval len");
        goto err_close_file;
    }

    for (i = 0; i < count; i++) {
        ci = sol_vector_append(&iann->confidence_intervals);
        if (!ci) {
            sml_critical("Could not alloc the confidence interval");
            goto err_close_file;
        }
        if (fread(&ci->lower_limit, sizeof(float), 1, f) < 1) {
            sml_critical("Could not read the lower limit");
            goto err_close_file;
        }
        if (fread(&ci->upper_limit, sizeof(float), 1, f) < 1) {
            sml_critical("Could not read the upper limit");
            goto err_close_file;
        }
        iann->ci_length_sum += (ci->upper_limit - ci->lower_limit);
    }

    if (fread(&iann->max_neurons, sizeof(unsigned int), 1, f) < 1) {
        sml_critical("Could not read the max neurons");
        goto err_close_file;
    }

    if (fread(&iann->required_observations, sizeof(unsigned int), 1, f) < 1) {
        sml_critical("Could not read the required observations");
        goto err_close_file;
    }

    iann->observations = fann_create_train(iann->required_observations,
        fann_get_num_input(iann->ann),
        fann_get_num_output(iann->ann));
    if (!iann->observations) {
        sml_critical("Could not alloc observations array");
        goto err_close_file;
    }

    if (fclose(f) == EOF) {
        sml_critical("Could not correctly close the CI file");
        goto err_exit;
    }

    return iann;
err_close_file:
    fclose(f);
err_exit:
    sml_ann_bridge_free(iann);
    return NULL;
}

void
sml_ann_bridge_print_debug(struct sml_ann_bridge *ann)
{
    uint16_t i;
    Confidence_Interval *interval;

    sml_debug("\ttrained: %s", ann->trained ? "true" : "false");
    sml_debug("\tlast_train_error: %g", ann->last_train_error);
    sml_debug("\tConfidence Intervals (%d) {", ann->confidence_intervals.len);
    SOL_VECTOR_FOREACH_IDX (&ann->confidence_intervals, interval, i) {
        sml_debug("\t\t{%g - %g}", interval->lower_limit,
            interval->upper_limit);
    }
    sml_debug("\t}");
}

float
sml_ann_bridge_get_error(struct sml_ann_bridge *iann,
    struct sml_variables_list *input, struct sml_variables_list *output,
    unsigned int observations)
{
    struct fann_train_data *test_data;
    float err = NAN;

    test_data = fann_create_train(observations,
        sml_ann_variables_list_get_length(input),
        sml_ann_variables_list_get_length(output));
    if (!test_data) {
        sml_critical("Could not create the test data");
        return err;
    }

    _sml_ann_bridge_fill_train_data_array(input, output, test_data,
        observations);
    err = fann_test_data(iann->ann, test_data);
    sml_debug("ANN current error:%f", err);
    fann_destroy_train(test_data);
    return err;
}
