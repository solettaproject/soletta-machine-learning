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

#include <fl/Headers.h>
#include <stdio.h>
#include <string.h>
#include "sml_fuzzy_bridge.h"
#include "sml_measure.h"
#include <macros.h>
#include <errno.h>
#include <sol-vector.h>
#include <sml_log.h>
#include <float.h>
#include <new>
#include <stdexcept>
#include <algorithm>

#define DEFAULT_ACCUMULATION (SML_FUZZY_SNORM_MAXIMUM)

struct terms_width {
    bool is_id;
    float value;
};

template<class Variable>
static uint16_t
_calc_terms_count(const std::vector<Variable*> &vec)
{
    uint16_t i, count = 0;
    for (i = 0; i < vec.size(); i++)
        count += vec[i]->numberOfTerms();
    return count;
}

/* remove all the rules from the first rule block
   and remove all the other ruleblocks, since
   all the rules we'll be added on the first one */
static void
_remove_rule_blocks(fl::Engine *engine)
{
    uint16_t rule_blocks_size, rules_size, i;
    fl::RuleBlock *rule_block;

    sml_debug("Removing rules");

    rule_blocks_size = engine->numberOfRuleBlocks();
    if (rule_blocks_size == 0)
        return;

    rule_block = engine->getRuleBlock(0);
    rules_size = rule_block->numberOfRules();
    for (i = 0; i < rules_size; i++)
        delete rule_block->removeRule(0);

    for (i = 1; i < rule_blocks_size; i++)
        delete engine->removeRuleBlock(1);
}

static bool
_is_in_list(struct sml_variables_list *list, fl::Variable *var,
    uint16_t *index)
{
    uint16_t i =0;
    std::vector<fl::Variable*> *variables =
        static_cast<std::vector<fl::Variable*> *>((void *)list);
    for (std::vector<fl::Variable*>::const_iterator it =
         variables->begin();
         it != variables->end(); ++it) {
        if (*it == var) {
            if (index)
                *index = i;
            return true;
        }
        i++;
    }
    return false;
}

/* only create rule block if it doesn't exists */
static bool
_create_rule_block(fl::Engine *engine)
{
    fl::RuleBlock *rule_block;
    fl::TNorm *tnorm;
    fl::TNorm *activation;

    if (engine->numberOfRuleBlocks() > 0)
        return true;

    rule_block = new (std::nothrow) fl::RuleBlock();

    if (!rule_block)
        return false;

    tnorm = new (std::nothrow) fl::Minimum();
    if (!tnorm)
        goto err_tnorm;

    activation = new (std::nothrow) fl::Minimum();
    if (!activation)
        goto err_activation;

    rule_block->setEnabled(true);
    rule_block->setName("");
    rule_block->setConjunction(tnorm);
    rule_block->setActivation(activation);

    engine->addRuleBlock(rule_block);
    return true;

err_activation:
    delete tnorm;
err_tnorm:
    delete rule_block;
    return false;
}

struct sml_fuzzy *
sml_fuzzy_bridge_new(void)
{
    struct sml_fuzzy *fuzzy = (struct sml_fuzzy *) calloc(1, sizeof(struct sml_fuzzy));
    fl::Engine *engine = NULL;

    if (!fuzzy) {
        sml_critical("Failed to create fuzzy");
        return NULL;
    }

    engine = new (std::nothrow) fl::Engine();
    if (!engine)
        goto new_error;

    if (!_create_rule_block(engine)) {
        sml_critical("Could not alloc the rule block");
        goto new_error;
    }

    engine->setName("EngineDefault");
    fuzzy->engine = engine;
    fuzzy->input_list = (struct sml_variables_list*)
        &(engine->inputVariables());
    fuzzy->output_list = (struct sml_variables_list*)
        &(engine->outputVariables());
    sol_vector_init(&fuzzy->input_terms_width, sizeof(struct terms_width));
    sol_vector_init(&fuzzy->output_terms_width, sizeof(struct terms_width));

    return fuzzy;

new_error:
    free(fuzzy);
    delete engine;
    return NULL;
}

bool
sml_fuzzy_save_file(struct sml_fuzzy *fuzzy, const char *filename)
{
    fl::FllExporter exporter;
    fl::RuleBlock *block, *new_block;
    fl::Engine *engine = (fl::Engine *) fuzzy->engine;
    FILE *f;

    if (!engine)
        return false;

    f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", filename);
        return false;
    }

    block = engine->removeRuleBlock(0);
    new_block = new fl::RuleBlock();

    new_block->setActivation(block->getActivation()->clone());
    new_block->setConjunction(block->getConjunction()->clone());
    engine->addRuleBlock(new_block);
    fprintf(f, "%s\n", exporter.toString(engine).c_str());
    if (fclose(f))
        return false;

    delete engine->removeRuleBlock(0);
    engine->addRuleBlock(block);
    return true;
}

bool
sml_fuzzy_load_file(struct sml_fuzzy *fuzzy, const char *filename)
{
    fl::Engine *engine;

    try {
        fl::FllImporter importer;
        engine = importer.fromFile(filename);

    } catch (fl::Exception e) {
        sml_critical("%s", e.getWhat().c_str());
        return false;
    }

    if (engine->numberOfInputVariables() == 0 ||
        engine->numberOfOutputVariables() == 0) {
        sml_critical("Input and output variables must be provided!");
        goto error;
    }

    if (engine->numberOfRuleBlocks() == 0) {
      sml_critical("Rule blocks must be provided!");
      goto error;
    }


    if (fuzzy->engine)
        delete (fl::Engine*) fuzzy->engine;

    fuzzy->engine = engine;
    fuzzy->input_list = (struct sml_variables_list*)&(engine->inputVariables());
    fuzzy->output_list = (struct sml_variables_list*)&(engine->outputVariables());
    fuzzy->input_terms_count = _calc_terms_count(engine->inputVariables());
    fuzzy->output_terms_count = _calc_terms_count(engine->outputVariables());
    _remove_rule_blocks(engine);

    return true;

error:
    delete engine;
    return false;
}

static int
_sml_fuzzy_fill_membership_values(struct sml_matrix *variables,
                                  struct sml_variables_list *list)
{
    uint16_t i, j, len;
    float f, *ptr;

    len = sml_fuzzy_variables_list_get_length(list);
    try {
        for (i = 0; i < len; i++) {
            struct sml_variable *variable = sml_fuzzy_variables_list_index(list, i);
            uint16_t terms_len = sml_fuzzy_variable_terms_count(variable);
            for (j = 0; j < terms_len; j++) {
                fl::Term *term = (fl::Term *)
                    sml_fuzzy_variable_get_term(variable, j);
                f = term->membership(sml_fuzzy_variable_get_value(variable));
                ptr = (float*) sml_matrix_insert(variables, i, j);
                if (!ptr) {
                    sml_critical("Could not fill membership to variable");
                    return -ENOMEM;
                }
                *ptr = f;
            }
        }
    } catch (fl::Exception e) {
        sml_critical("%s", e.getWhat().c_str());
        return SML_INTERNAL_ERROR;
    }
    return 0;
}

struct sml_measure *
sml_fuzzy_get_membership_values(struct sml_fuzzy *fuzzy)
{
    struct sml_measure *measure = sml_measure_new(sizeof(float), sizeof(float));
    if (!measure)
        return NULL;

    if (_sml_fuzzy_fill_membership_values(&measure->inputs,
                                           fuzzy->input_list))
        goto membership_error;

    if (_sml_fuzzy_fill_membership_values(&measure->outputs,
                                           fuzzy->output_list))
        goto membership_error;

    return measure;

membership_error:
    sml_measure_free(measure);
    return NULL;
}

int
sml_fuzzy_get_membership_values_output(struct sml_fuzzy *fuzzy,
                                       struct sml_matrix *output_membership)
{
    int error;
    if ((error = _sml_fuzzy_fill_membership_values(output_membership,
                                                   fuzzy->output_list))) {
        sml_matrix_clear(output_membership);
        return error;
    }
    return 0;
}

int
sml_fuzzy_process_output(struct sml_fuzzy *fuzzy)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;

    try {
        engine->process();
    } catch (fl::Exception e) {
        sml_critical("%s", e.getWhat().c_str());
        return SML_INTERNAL_ERROR;
    }

    return 0;
}

void
sml_fuzzy_destroy(struct sml_fuzzy *fuzzy)
{
    delete (fl::Engine*) fuzzy->engine;
    sol_vector_clear(&fuzzy->input_terms_width);
    sol_vector_clear(&fuzzy->output_terms_width);
    free(fuzzy);
}

static fl::TNorm *
_get_tnorm(enum sml_fuzzy_tnorm norm) {
    fl::TNorm *fl_norm;

    switch (norm) {
    case SML_FUZZY_TNORM_ALGEBRAIC_PRODUCT:
        sml_debug("Conjunction set to algebraic product");
        fl_norm =  new (std::nothrow) fl::AlgebraicProduct();
        break;
    case SML_FUZZY_TNORM_BOUNDED_DIFFERENCE:
        sml_debug("Conjunction set to bounded difference");
        fl_norm =  new (std::nothrow) fl::BoundedDifference();
        break;
    case SML_FUZZY_TNORM_DRASTIC_PRODUCT:
        sml_debug("Conjunction set to drastic product");
        fl_norm =  new (std::nothrow) fl::DrasticProduct();
        break;
    case SML_FUZZY_TNORM_EINSTEIN_PRODUCT:
        sml_debug("Conjunction set to einstein product");
        fl_norm =  new (std::nothrow) fl::EinsteinProduct();
        break;
    case SML_FUZZY_TNORM_HAMACHER_PRODUCT:
        sml_debug("Conjunction set to hamacher product");
        fl_norm =  new (std::nothrow) fl::HamacherProduct();
        break;
    case SML_FUZZY_TNORM_MINIMUM:
        sml_debug("Conjunction set to minimum");
        fl_norm =  new (std::nothrow) fl::Minimum();
        break;
    case SML_FUZZY_TNORM_NILPOTENT_MINIMUM:
        sml_debug("Conjunction set to nilpotent minimum");
        fl_norm = new (std::nothrow) fl::NilpotentMinimum();
        break;
    default:
        sml_critical("Unknown TNorm %d", norm);
        return NULL;
    }

    if (!fl_norm)
        sml_critical("Could not alloc the tnorm");

    return fl_norm;
}

bool
sml_fuzzy_bridge_conjunction_set(struct sml_fuzzy *fuzzy, enum sml_fuzzy_tnorm norm)
{
    fl::TNorm *fl_norm = _get_tnorm(norm);
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    if (!fl_norm)
        return false;

    if (!_create_rule_block(engine)) {
        sml_critical("Could not alloc the rule block");
        return false;
    }

    engine->getRuleBlock(0)->setConjunction(fl_norm);
    return true;
}

static fl::SNorm *
_get_snorm(enum sml_fuzzy_snorm norm)
{
    fl::SNorm *fl_norm;

    switch (norm) {
    case SML_FUZZY_SNORM_ALGEBRAIC_SUM:
        sml_debug("SNorm is algebraic sum");
        fl_norm = new (std::nothrow) fl::AlgebraicSum();
        break;
    case SML_FUZZY_SNORM_BOUNDED_SUM:
        sml_debug("SNorm is bounded sum");
        fl_norm = new (std::nothrow) fl::BoundedSum();
        break;
    case SML_FUZZY_SNORM_DRASTIC_SUM:
        sml_debug("SNorm is drastic sum");
        fl_norm = new (std::nothrow) fl::DrasticSum();
        break;
    case SML_FUZZY_SNORM_EINSTEIN_SUM:
        sml_debug("SNorm is einstein sum");
        fl_norm = new (std::nothrow) fl::EinsteinSum();
        break;
    case SML_FUZZY_SNORM_HAMACHER_SUM:
        sml_debug("SNorm is hamacher sum");
        fl_norm = new (std::nothrow) fl::HamacherSum();
        break;
    case SML_FUZZY_SNORM_MAXIMUM:
        sml_debug("SNorm is maximum");
        fl_norm = new (std::nothrow) fl::Maximum();
        break;
    case SML_FUZZY_SNORM_NILPOTENT_MAXIMUM:
        sml_debug("SNorm is nilpotent maximum");
        fl_norm = new (std::nothrow) fl::NilpotentMaximum();
        break;
    case SML_FUZZY_SNORM_NORMALIZED_SUM:
        sml_debug("SNorm is normalized sum");
        fl_norm = new (std::nothrow) fl::NormalizedSum();
        break;
    default:
        sml_critical("Unknown SNorm %d", norm);
        return NULL;
    }

    if (!fl_norm)
        sml_critical("Could not alloc the snorm");

    return fl_norm;
}

uint16_t
sml_fuzzy_variables_list_get_length(struct sml_variables_list *list)
{
    std::vector<fl::Variable*> *variables =
        static_cast<std::vector<fl::Variable*> *>((void *)list);
    return variables->size();
}

struct sml_variable *
sml_fuzzy_variables_list_index(struct sml_variables_list *list, uint16_t index)
{
    std::vector<fl::Variable*> *variables =
        static_cast<std::vector<fl::Variable*> *>((void *)list);
    return (struct sml_variable *) variables->at(index);
}

int
sml_fuzzy_variable_get_name(struct sml_variable *variable, char *var_name, size_t var_name_size)
{
    fl::Variable *fl_var = (fl::Variable*) variable;

    if ((!var_name) || var_name_size == 0) {
        sml_warning("Invalid parameters");
        return -EINVAL;
    }

    if (var_name_size <= fl_var->getName().length()) {
        sml_warning("Not enough space to get name %s(%d)",
            fl_var->getName().c_str(), fl_var->getName().length());
        return -EINVAL;
    }

    strcpy(var_name, fl_var->getName().c_str());
    return 0;
}

uint16_t
sml_fuzzy_variable_terms_count(struct sml_variable *variable)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    return fl_var->numberOfTerms();
}

struct sml_fuzzy_term *
sml_fuzzy_variable_get_term(struct sml_variable *variable, uint16_t index)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    return (struct sml_fuzzy_term *)fl_var->getTerm(index);
}

int
sml_fuzzy_term_get_name(struct sml_fuzzy_term *term, char *term_name, size_t term_name_size)
{
    fl::Term *fl_term = (fl::Term*) term;

    if ((!term_name) || term_name_size == 0) {
        sml_warning("Invalid parameters");
        return -EINVAL;
    }

    if (term_name_size <= fl_term->getName().length()) {
        sml_warning("Not enough space to get name %s(%d)",
            fl_term->getName().c_str(), fl_term->getName().length());
        return -EINVAL;
    }

    strcpy(term_name, fl_term->getName().c_str());
    return 0;
}

static float
_var_get_val_in_range(fl::Variable *fl_var, float value)
{
    if (value < fl_var->getMinimum())
        return fl_var->getMinimum();
    if (value > fl_var->getMaximum())
        return fl_var->getMaximum();

    return value;
}

float
sml_fuzzy_variable_get_value(struct sml_variable *variable)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::OutputVariable *output_var;
    fl::InputVariable *input_var;

    input_var = dynamic_cast<fl::InputVariable*>(fl_var);
    if (input_var)
        return _var_get_val_in_range(fl_var, input_var->getInputValue());

    output_var = dynamic_cast<fl::OutputVariable*>(fl_var);
    if (output_var)
        return _var_get_val_in_range(fl_var, output_var->getOutputValue());

    sml_warning("Trying to use unkown class of variable");
    return NAN;
}

void
sml_fuzzy_variable_set_value(struct sml_variable *variable, float value)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::OutputVariable *output_var;
    fl::InputVariable *input_var;

    input_var = dynamic_cast<fl::InputVariable*>(fl_var);
    if (input_var) {
        input_var->setInputValue(value);
        return;
    }

    output_var = dynamic_cast<fl::OutputVariable*>(fl_var);
    if (output_var) {
        output_var->setOutputValue(value);
        return;
    }

    sml_warning("Trying to use unkown class of variable");
}

struct sml_variable *
sml_fuzzy_new_input(struct sml_fuzzy *fuzzy, const char *name)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    struct terms_width *width;

    if (engine->hasInputVariable(name))
        return NULL;

    fl::InputVariable *variable = new (std::nothrow) fl::InputVariable();
    if (!variable)
        return NULL;

    variable->setEnabled(true);
    variable->setName(name);
    variable->setRange(-FLT_MAX, FLT_MAX);
    engine->addInputVariable(variable);

    width = (struct terms_width*)sol_vector_append(&fuzzy->input_terms_width);
    width->is_id = false;
    width->value = NAN;
    return (struct sml_variable *)variable;
}

struct sml_variable *
sml_fuzzy_new_output(struct sml_fuzzy *fuzzy, const char *name)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    struct terms_width *width;

    if (engine->hasOutputVariable(name))
        return NULL;

    fl::OutputVariable *variable = new (std::nothrow) fl::OutputVariable();
    fl::Defuzzifier *fl_defuzzifier =
        new (std::nothrow) fl::Centroid();

    if (!variable) return NULL;

    if (!fl_defuzzifier) {
        delete variable;
        return NULL;
    }

    variable->setEnabled(true);
    variable->setName(name);
    variable->setRange(-FLT_MAX, FLT_MAX);
    variable->setDefaultValue(NAN);
    variable->setDefuzzifier(fl_defuzzifier);
    variable->fuzzyOutput()->setAccumulation(_get_snorm(DEFAULT_ACCUMULATION));
    engine->addOutputVariable(variable);

    width = (struct terms_width*)sol_vector_append(&fuzzy->output_terms_width);
    width->is_id = false;
    width->value = NAN;
    return (struct sml_variable *)variable;
}

struct sml_variable *
sml_fuzzy_get_input(struct sml_fuzzy *fuzzy, const char *name)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    fl::Variable *variable = engine->getInputVariable(name);
    return (struct sml_variable *)variable;
}

struct sml_variable *
sml_fuzzy_get_output(struct sml_fuzzy *fuzzy, const char *name)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    fl::Variable *variable = engine->getOutputVariable(name);
    return (struct sml_variable *)variable;
}

void
sml_fuzzy_variable_set_enabled(struct sml_variable *variable, bool enabled)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl_var->setEnabled(enabled);
    sml_debug("Variable %s %s", fl_var->getName().c_str(),
            enabled ? "enabled" : "disabled");
}

bool
sml_fuzzy_variable_is_enabled(struct sml_variable *variable)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    return fl_var->isEnabled();
}

bool
sml_fuzzy_bridge_output_set_defuzzifier(struct sml_variable *variable,
                                        enum sml_fuzzy_defuzzifier defuzzifier,
                                        int defuzzifier_resolution)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Defuzzifier *fl_defuzzifier = NULL;
    fl::OutputVariable *output_var = dynamic_cast<fl::OutputVariable*>(fl_var);
    if (!output_var) {
        sml_critical("Not a output variable!");
        return false;
    }

    switch (defuzzifier) {
    case SML_FUZZY_DEFUZZIFIER_BISECTOR:
        fl_defuzzifier =
            new (std::nothrow) fl::Bisector(defuzzifier_resolution);
        break;
    case SML_FUZZY_DEFUZZIFIER_CENTROID:
        fl_defuzzifier =
            new (std::nothrow) fl::Centroid(defuzzifier_resolution);
        break;
    case SML_FUZZY_DEFUZZIFIER_LARGEST_OF_MAXIMUM:
        fl_defuzzifier =
            new (std::nothrow) fl::LargestOfMaximum(defuzzifier_resolution);
        break;
    case SML_FUZZY_DEFUZZIFIER_MEAN_OF_MAXIMUM:
        fl_defuzzifier =
            new (std::nothrow) fl::MeanOfMaximum(defuzzifier_resolution);
        break;
    case SML_FUZZY_DEFUZZIFIER_SMALLEST_OF_MAXIMUM:
        fl_defuzzifier =
            new (std::nothrow) fl::SmallestOfMaximum(defuzzifier_resolution);
        break;
    case SML_FUZZY_DEFUZZIFIER_WEIGHTED_AVERAGE:
        fl_defuzzifier = new (std::nothrow) fl::WeightedAverage();
        break;
    case SML_FUZZY_DEFUZZIFIER_WEIGHTED_SUM:
        fl_defuzzifier = new (std::nothrow) fl::WeightedSum();
        break;
    default:
        sml_critical("Unknown defuzzifier %d", defuzzifier);
    }

    if (!fl_defuzzifier) {
        sml_critical("Failed to create defuzzifier");
        return false;
    }
    output_var->setDefuzzifier(fl_defuzzifier);
    return true;
}

bool
sml_fuzzy_bridge_output_set_accumulation(struct sml_variable *variable,
                                         enum sml_fuzzy_snorm accumulation)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::OutputVariable *output_var = dynamic_cast<fl::OutputVariable*>(fl_var);
    if (!output_var) {
        sml_critical("Not a output variable!");
        return false;
    }

    output_var->fuzzyOutput()->setAccumulation(_get_snorm(accumulation));

    return true;
}

void
sml_fuzzy_bridge_variable_set_range(struct sml_variable *variable, float min,
    float max)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl_var->setRange(min, max);
}

bool
sml_fuzzy_variable_get_range(struct sml_variable *variable, float *min, float *max) {
    fl::Variable *fl_var = (fl::Variable*) variable;
    if (min)
        *min = fl_var->getMinimum();
    if (max)
        *max = fl_var->getMaximum();
    return true;
}

static bool
_find_variable(std::vector<fl::Variable*> *list, fl::Variable *var,
               uint16_t *index)
{
    uint16_t len, i;
    len = list->size();
    for (i = 0; i < len; i++)
        if (list->at(i) == var) {
            if (index)
                *index = i;
            return true;
        }
    return false;
}

static void
_debug_variables(struct sml_variables_list *list)
{
    float val;
    struct sml_variable *var;
    struct sml_fuzzy_term *term;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    char term_name[SML_TERM_NAME_MAX_LEN + 1];
    uint16_t i, len, j, len_j;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(list, i);
        val = sml_fuzzy_variable_get_value(var);

        if (sml_fuzzy_variable_get_name(var, var_name, sizeof(var_name)))
            sml_warning("Failed to get name of variable %p", var);
        else
            sml_debug("\t\t%s: %g", var_name, val);

        len_j = sml_fuzzy_variable_terms_count(var);
        for (j = 0; j < len_j; j++) {
            term = sml_fuzzy_variable_get_term(var, j);
            if (sml_fuzzy_term_get_name(term, term_name, sizeof(term_name))) {
                sml_warning("Failed to get name of term %p", term);
                continue;
            }
            sml_debug("\t\t\t%s: %g", term_name,
                      ((fl::Term *)term)->membership(val));
        }
    }
}

bool
sml_fuzzy_remove_variable(struct sml_fuzzy *fuzzy, struct sml_variable *variable)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Variable *removed_var = NULL;
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    uint16_t index;
    bool ret = true;

    if (_find_variable((std::vector<fl::Variable*> *) fuzzy->input_list,
                fl_var, &index)) {
        removed_var = engine->removeInputVariable(index);
        fuzzy->input_terms_count -= removed_var->numberOfTerms();
        if (sol_vector_del(&fuzzy->input_terms_width, index))
            ret = false;
    } else if (_find_variable((std::vector<fl::Variable*> *)
               fuzzy->output_list, fl_var, &index)) {
        removed_var = engine->removeOutputVariable(index);
        fuzzy->output_terms_count -= removed_var->numberOfTerms();
        if (sol_vector_del(&fuzzy->output_terms_width, index))
            ret = false;
    } else {
        sml_critical("Variable is not a valid input or output");
        return false;
    }

    delete removed_var;
    return ret;
}

bool
sml_fuzzy_find_variable(struct sml_variables_list *list, struct sml_variable *var,
                        uint16_t *index)
{
    return _find_variable((std::vector<fl::Variable*> *) list,
                          (fl::Variable*) var, index);
}

bool
sml_fuzzy_variable_find_term(struct sml_variable *var, struct sml_fuzzy_term *term,
                             uint16_t *index)
{
    uint16_t i, len;

    len = sml_fuzzy_variable_terms_count(var);
    for (i = 0; i < len; i++)
        if (sml_fuzzy_variable_get_term(var, i) == term) {
            if (index)
                *index = i;
            return true;
        }

    return false;
}

static bool
_check_name(const char *name)
{
    size_t name_len;

    if (!name) {
        sml_warning("Name must be provided to add term");
        return false;
    }

    name_len = strlen(name);
    if (name_len == 0 || name_len >= SML_TERM_NAME_MAX_LEN) {
        sml_warning("Invalid name size (%d) for variable %s", name_len,
            name);
        return false;
    }

    return true;
}

struct sml_fuzzy_term*
sml_fuzzy_bridge_variable_add_term_rectangle(struct sml_fuzzy *fuzzy,
                                             struct sml_variable *variable,
                                             const char *name, float start,
                                             float end)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Term *term;

    if (!_check_name(name))
        return NULL;

    term = new (std::nothrow) fl::Rectangle(name, start, end, 1.0);

    if (!term) {
        sml_critical("Failed to create term");
        return NULL;
    }

    if (sml_fuzzy_is_input(fuzzy, variable, NULL))
        fuzzy->input_terms_count++;
    else if (sml_fuzzy_is_output(fuzzy, variable, NULL))
        fuzzy->output_terms_count++;
    else {
        delete term;
        return NULL;
    }

    fl_var->addTerm(term);
    return (struct sml_fuzzy_term*) term;
}

struct sml_fuzzy_term*
sml_fuzzy_bridge_variable_add_term_triangle(struct sml_fuzzy *fuzzy,
                                            struct sml_variable *variable,
                                            const char *name, float vertex_a,
                                            float vertex_b, float vertex_c)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Term *term;

    if (!_check_name(name))
        return NULL;

    term = new (std::nothrow) fl::Triangle(name, vertex_a, vertex_b, vertex_c,
                                          1.0);

    if (!term) {
        sml_critical("Failed to create term");
        return NULL;
    }

    if (sml_fuzzy_is_input(fuzzy, variable, NULL))
        fuzzy->input_terms_count++;
    else if (sml_fuzzy_is_output(fuzzy, variable, NULL))
        fuzzy->output_terms_count++;
    else {
        delete term;
        return NULL;
    }

    fl_var->addTerm(term);
    return (struct sml_fuzzy_term *) term;
}

struct sml_fuzzy_term*
sml_fuzzy_bridge_variable_add_term_cosine(struct sml_fuzzy *fuzzy,
                                          struct sml_variable *variable,
                                          const char *name, float center,
                                          float width)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Term *term;

    if (!_check_name(name))
        return NULL;

    term = new (std::nothrow) fl::Cosine(name, center, width, 1.0);
    if (!term) {
        sml_critical("Failed to create term");
        return NULL;
    }

    if (sml_fuzzy_is_input(fuzzy, variable, NULL))
        fuzzy->input_terms_count++;
    else if (sml_fuzzy_is_output(fuzzy, variable, NULL))
        fuzzy->output_terms_count++;
    else {
        delete term;
        return NULL;
    }

    fl_var->addTerm(term);
    return (struct sml_fuzzy_term *) term;
}

struct sml_fuzzy_term*
sml_fuzzy_bridge_variable_add_term_gaussian(struct sml_fuzzy *fuzzy,
                                            struct sml_variable *variable,
                                            const char *name, float mean,
                                            float standard_deviation)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Term *term;

    if (!_check_name(name))
        return NULL;

    term = new (std::nothrow) fl::Gaussian(name, mean, standard_deviation,
                                           1.0);
    if (!term) {
        sml_critical("Failed to create term");
        return NULL;
    }

    if (sml_fuzzy_is_input(fuzzy, variable, NULL))
        fuzzy->input_terms_count++;
    else if (sml_fuzzy_is_output(fuzzy, variable, NULL))
        fuzzy->output_terms_count++;
    else {
        delete term;
        return NULL;
    }

    fl_var->addTerm(term);
    return (struct sml_fuzzy_term *) term;
}

struct sml_fuzzy_term*
sml_fuzzy_bridge_variable_add_term_ramp(struct sml_fuzzy *fuzzy,
                                        struct sml_variable *variable,
                                        const char *name, float start,
                                        float end)
{
    fl::Variable *fl_var = (fl::Variable*) variable;
    fl::Term *term;

    if (!_check_name(name))
        return NULL;

    term = new (std::nothrow) fl::Ramp(name, start, end, 1.0);
    if (!term) {
        sml_critical("Failed to create term");
        return NULL;
    }

    if (sml_fuzzy_is_input(fuzzy, variable, NULL))
        fuzzy->input_terms_count++;
    else if (sml_fuzzy_is_output(fuzzy, variable, NULL))
        fuzzy->output_terms_count++;
    else {
        delete term;
        return NULL;
    }

    fl_var->addTerm(term);
    return (struct sml_fuzzy_term *) term;
}

bool
sml_fuzzy_is_input(struct sml_fuzzy *fuzzy, struct sml_variable *variable,
    uint16_t *index)
{
    fl::InputVariable *input_var;

    input_var = dynamic_cast<fl::InputVariable*>((fl::Variable*) variable);
    if (!input_var)
        return false;

    return _is_in_list(fuzzy->input_list, input_var, index);
}

bool
sml_fuzzy_is_output(struct sml_fuzzy *fuzzy, struct sml_variable *variable,
    uint16_t *index)
{
    fl::OutputVariable *output_var;

    output_var = dynamic_cast<fl::OutputVariable*>((fl::Variable*) variable);
    if (!output_var)
        return false;

    return _is_in_list(fuzzy->output_list, output_var, index);

}

bool
sml_fuzzy_is_rule_block_empty(struct sml_fuzzy *fuzzy)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;

    if (engine->numberOfRuleBlocks() == 0)
        return true;

    return engine->getRuleBlock(0)->numberOfRules() == 0;
}

struct sml_fuzzy_rule *
sml_fuzzy_rule_add(struct sml_fuzzy *fuzzy, const char *rule)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    fl::RuleBlock *block = engine->getRuleBlock(0);
    fl::Rule *rule_obj;

    try {
        rule_obj = fl::Rule::parse(rule, engine);
        block->addRule(rule_obj);
    } catch (fl::Exception e) {
        sml_critical("%s", e.getWhat().c_str());
        return NULL;
    }

    return (struct sml_fuzzy_rule *) rule_obj;
}

bool
sml_fuzzy_rule_free(struct sml_fuzzy *fuzzy, struct sml_fuzzy_rule *rule)
{
    uint16_t index = 0;
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    fl::RuleBlock *block = engine->getRuleBlock(0);
    std::vector<fl::Rule*>& rules = block->rules();

    for (std::vector<fl::Rule*>::const_iterator it =
            rules.begin();
            it != rules.end(); ++it, index++) {
        if (*it == (void *)rule) {
            delete block->removeRule(index);
            return true;
        }
    }

    return false;
}

bool
sml_fuzzy_term_get_range(struct sml_fuzzy_term *term, float *min, float *max) {
    fl::Term *fl_term = (fl::Term*) term;
    fl::Rectangle *rect;
    fl::Triangle *triangle;
    fl::Ramp *ramp;

    rect = dynamic_cast<fl::Rectangle*>(fl_term);
    if (rect) {
        if (min)
            *min = rect->getStart();
        if (max)
            *max = rect->getEnd();
        return true;
    }

    triangle = dynamic_cast<fl::Triangle*>(fl_term);
    if (triangle) {
        if (min)
            *min = triangle->getVertexA();
        if (max)
            *max = triangle->getVertexC();
        return true;
    }

    ramp = dynamic_cast<fl::Ramp*>(fl_term);
    if (ramp) {
        if (min)
            *min = fmin(ramp->getStart(), ramp->getEnd());
        if (max)
            *max = fmax(ramp->getStart(), ramp->getEnd());
        return true;
    }

    return false;
}

bool
sml_fuzzy_term_set_range(struct sml_fuzzy_term *term, float min, float max) {
    fl::Term *fl_term = (fl::Term*) term;
    fl::Rectangle *rect;
    fl::Triangle *triangle;
    fl::Ramp *ramp;

    rect = dynamic_cast<fl::Rectangle*>(fl_term);
    if (rect) {
        rect->setStart(min);
        rect->setEnd(max);
        return true;
    }

    triangle = dynamic_cast<fl::Triangle*>(fl_term);
    if (triangle) {
        triangle->setVertexA(min);
        triangle->setVertexB((min + max) / 2);
        triangle->setVertexC(max);
        return true;
    }

    ramp = dynamic_cast<fl::Ramp*>(fl_term);
    if (ramp) {
        if (ramp->direction() == fl::Ramp::POSITIVE) {
            ramp->setStart(min);
            ramp->setEnd(max);
        } else {
            ramp->setStart(max);
            ramp->setEnd(min);
        }
        return true;
    }

    return false;
}

int
sml_fuzzy_bridge_variable_remove_term(struct sml_variable *variable,
                                      uint16_t term_num)
{
    fl::Term *term;
    fl::Variable *fl_var = (fl::Variable*) variable;

    term = fl_var->removeTerm(term_num);
    if (term) {
        delete term;
        return 0;
    }

    return -EINVAL;
}

void
sml_fuzzy_debug(struct sml_fuzzy *fuzzy)
{
    sml_debug("Fuzzy Bridge:");
    sml_debug("\tInputs(%d) {",
            sml_fuzzy_variables_list_get_length(fuzzy->input_list));
    _debug_variables(fuzzy->input_list);
    sml_debug("\t}");
    sml_debug("\tOutputs(%d) {",
              sml_fuzzy_variables_list_get_length(fuzzy->output_list));
    _debug_variables(fuzzy->output_list);
    sml_debug("\t}");

}

struct sml_variables_list *
sml_fuzzy_variables_list_new(struct sml_fuzzy *fuzzy, struct sol_vector *indexes)
{
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    uint16_t i, *idx;
    void *v;
    std::vector<fl::OutputVariable*> *changed =
        new (std::nothrow) std::vector<fl::OutputVariable*>();

    if (!changed) {
        sml_critical("Could not alloc the variables list!");
        return NULL;
    }

    if (indexes) {
        try {
            SOL_VECTOR_FOREACH_IDX(indexes, v, i) {
                idx = (uint16_t *)v;
                changed->push_back(engine->getOutputVariable(*idx));
            }
        } catch (const std::out_of_range &oor) {
            sml_critical("Array out of bounds. error:%s", oor.what());
            goto err_exit;
        }
    }
    return (struct sml_variables_list *)changed;

err_exit:
    delete changed;
    return NULL;
}

void
sml_fuzzy_variables_list_free(struct sml_variables_list *list) {
    std::vector<fl::OutputVariable*> *vector =
        (std::vector<fl::OutputVariable*> *)list;
    delete vector;
}

bool
sml_fuzzy_set_read_values(struct sml_fuzzy *fuzzy, struct sml_variables_list *outputs) {
    fl::Engine *engine = (fl::Engine*)fuzzy->engine;
    std::vector<fl::OutputVariable*> *vector =
        (std::vector<fl::OutputVariable*> *)outputs;
    std::vector<fl::OutputVariable*> all_outputs = engine->outputVariables();

    for (std::vector<fl::OutputVariable*>::const_iterator it =
             all_outputs.begin(); it != all_outputs.end(); ++it) {
        if (std::find(vector->begin(), vector->end(), *it) == vector->end())
          (*it)->setOutputValue((*it)->getPreviousOutputValue());
    }
    return true;
}

struct terms_width*
_get_terms_width(struct sml_fuzzy *fuzzy, struct sml_variable *var)
{
    uint16_t i;

    if (sml_fuzzy_is_input(fuzzy, var, &i))
        return (struct terms_width *)sol_vector_get(&fuzzy->input_terms_width,
            i);
    if (sml_fuzzy_is_output(fuzzy, var, &i))
        return (struct terms_width *)sol_vector_get(&fuzzy->output_terms_width,
            i);

    return NULL;
}

bool
sml_fuzzy_bridge_variable_set_default_term_width(struct sml_fuzzy *fuzzy,
    struct sml_variable *var, float width_val)
{
    struct terms_width *width;

    width = _get_terms_width(fuzzy, var);

    if (width) {
        width->value = width_val;
        return true;
    }

    return false;
}

float
sml_fuzzy_bridge_variable_get_default_term_width(struct sml_fuzzy *fuzzy,
    struct sml_variable *var)
{
    struct terms_width *width;

    width = _get_terms_width(fuzzy, var);

    if (width)
        return width->value;

    return NAN;
}

bool
sml_fuzzy_bridge_variable_set_is_id(struct sml_fuzzy *fuzzy,
    struct sml_variable *var, bool is_id)
{
    struct terms_width *width;

    width = _get_terms_width(fuzzy, var);

    if (width) {
        width->is_id = is_id;
        return true;
    }

    return false;
}

bool
sml_fuzzy_bridge_variable_get_is_id(struct sml_fuzzy *fuzzy,
    struct sml_variable *var)
{
    struct terms_width *width;

    width = _get_terms_width(fuzzy, var);

    if (width)
        return width->is_id;

    return false;
}

bool
sml_fuzzy_bridge_variable_term_triangle_update(struct sml_fuzzy_term *term,
    float vertex_a, float vertex_b, float vertex_c)
{
    fl::Term *fl_term = (fl::Term*) term;
    fl::Triangle *triangle;

    triangle = dynamic_cast<fl::Triangle*>(fl_term);
    if (!triangle)
        return false;

    if (!isnan(vertex_a))
        triangle->setVertexA(vertex_a);

    if (!isnan(vertex_b))
        triangle->setVertexB(vertex_b);

    if (!isnan(vertex_c))
        triangle->setVertexA(vertex_c);

    return true;
}
