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

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @mainpage Soletta Machine Learning Project Documentation
 *
 * Soletta Machine Learning (SML) is an open source machine learning library
 * focused on development of IoT projects.
 *
 * It provides API to handle with client side AI and an easy to use
 * flow-based Soletta module. Supports neural networks and fuzzy logic
 * learning and can be easily extended to support others.
 *
 * SML is divided in the following groups:
 *
 * @li @ref Engine
 * @li @ref Fuzzy_Engine
 * @li @ref Log
 * @li @ref Mainloop
 * @li @ref Ann_Engine
 * @li @ref Variables
 */

/**
 * @file
 * @defgroup Engine Engine
 * @brief The SML common functions.
 *
 * The functions here are common to all engines.
 * They are used to free the engines,
 * create variables, save/load SML state and more.
 *
 * The SML main flow consist in the following steps:
 * - Choose an engine (ANN or Fuzzy).
 * - Create the inputs and outputs variables.
 * - Register the read callback.
 * - Register the change callback.
 * - Read the variable value, set it in SML and call process.
 *
 * As a simple example imagine that one wants to control an indoor light that
 * turns on automatically if a person enters in the room and goes off if
 * the person leaves. For that, this person has a presence sensor.
 *
 * Choosing engines and creating variables is straight-forward after
 * the solution is modeled. Let's say fuzzy was the chosen one.
 * Registering callbacks is simple as well:
 *
 * @dontinclude example_doc.c
 * @skip main
 * @until sml_set_output_state_changed_callback
 *
 * Terms are a way to split the values in the range in meaningful parts.
 * Fuzzy supports some functions to describe them, as ramps, triangles
 * and others.
 *
 * Although we have the possibility to define each term to describe your
 * problem, for this example we will let the fuzzy engine do this for you.
 * To improve the quality of the automatically created terms, we need to give
 * fuzzy engine some hints about the variable being used. As both variables are
 * boolean values, lets set the default term width to 0.5 and the range from
 * 0 to 1. So we will have 2 terms, one for on and another for off.
 *
 * After everything is set up, it's time to trigger the processing.
 * It will be done in a simple loop, making 150 sml_process() calls,
 * but more elaborated ways to call process() can be implemented,
 * using mainloops.
 *
 * @until }
 * @until }
 * @until }
 *
 * Read and change callbacks implementation varies a lot depending
 * on machine learning usage, since they're the interface with the
 * rest of the world.
 *
 * Read callback is a function that will be called at each processing cycle
 * to update variables values.
 *
 * So in this function the values of presence sensor and light must
 * to be fetched. It could be done via GPIO, OIC, or any other way,
 * depending in your product. It may be synchronous or asynchronous.
 *
 * To keep it simple, yet illustrative, we're going to simulate the values
 * in a function read_state_cb() Variable sensor_state represents presence
 * sensor reading, switch_state represents light state.
 *
 * This variable will be global since its going to be used by the callback
 * of state changes (you could use a struct with this variable and pass them
 * to callbacks).
 *
 * To simulate a daily based used, a day would be a cycle of 15 readings,
 * user will be present on last 5 readings of each "day".
 * When she leaves she remembers to turn lights off... most of the time.
 * This is simulated by off_count . When she forgets lights on and leaves,
 * machine learning should suggest to turn lights off.
 *
 * After states are "fetch", they're set on sml variables (input and output).
 *
 * @dontinclude example_doc.c
 * @skip static int switch_state
 * @until return true;
 * @until }
 *
 * To fetch predicted output values, it's required to set a
 * callback function using sml_set_output_state_changed_callback()
 *
 * This callback will be called when SML makes a prediction for at
 * least one output variable.
 *
 * Change state callback only will be called on output value changes.
 * But most of the time, this prediction should matches current state of
 * output variable. On this case no action must to be taken.
 * Also, sometimes SML doesn't have enough information to make a prediction,
 * setting output variable value to NaN.
 *
 * So in change state callback we're going to check first if it was able
 * to predict a value, then check if we need to act, in case prediction
 * and current light state diverges.
 *
 * In this example we'll just print a message informing the light state
 * should be changed.
 *
 * @skip static void
 * @until printf("Light should be turned OFF.\n");
 * @until }
 * @until }
 *
 * A few tips to obtain good predictions.
 *  - Do not try to control the whole world with one SML object.
 *  - If you want to control a light and an air-conditioning and they
 *    are independent of each other. Create two SML objects,
 *    one will control the light and another one the air-conditioning.
 *  - Try to avoid adding unnecessary inputs/outputs to SML (or forget
 *    to add relevant inputs/outputs), this may lead the poor
 *    predictions.
 *  - Test both engines and check which one has the best results
 *    for your problem.
 * @{
 */

#define SML_INTERNAL_ERROR 3 /**< SML error code. Could not complete an operation */

#define SML_VARIABLE_NAME_MAX_LEN (127) /**< Maximum size of variables name */

/**
 * @struct sml_object
 *
 * Instance of the chosen machine learning engine, it may be created with
 * sml_fuzzy_new() or sml_ann_new() and should be deleted after usage with
 * sml_free().
 */
struct sml_object;

/**
 * @struct sml_variable
 * @ingroup Variables
 *
 * A input or output variable.
 */
struct sml_variable;

/**
 * @struct sml_variables_list
 * @ingroup Variables
 *
 * A list of input or output variables (::sml_variable)
 */
struct sml_variables_list;

/**
 * @brief A user defined callback to read the variables values.
 *
 * @param sml The ::sml_object object
 * @param data The user defined data
 * @return @c true on success.
 * @return @c false if no reads were done.
 * @see ::sml_set_read_state_callback
 */
typedef bool (*sml_read_state_cb) (struct sml_object *sml, void *data);

/**
 * @brief Called every time the SML made a prediction.
 * @param sml The ::sml_object Object.
 * @param changed A ::sml_variables_list with the predicted variables.
 * @param data User defined data.
 * @see ::sml_set_output_state_changed_callback
 */
typedef void (*sml_change_cb)(struct sml_object *sml, struct sml_variables_list *changed, void *data);


/**
 * @brief Reads a FLL file.
 *
 * FLL stands for Fuzzylite language. It can be used to create/change the fuzzy terms/defuzzifiers
 * without using the SML apis.
 *
 * @remarks If the function is used on an already running SML engine, all the previous
 * knowledge will be lost!
 *
 * @param sml The ::sml_object object.
 * @param filename A fll file path.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_load_fll_file(struct sml_object *sml, const char *filename);

/**
 * @brief Frees the SML engine.
 *
 * @param sml The engine to be freed.
 */
void sml_free(struct sml_object *sml);

/**
 * @brief Register a read callblack.
 *
 * It should be used to set a callback function to read variables
 * values. This callback must return true if it was able to
 * read all variables or false on error. If an error happens
 * ::sml_process will be aborted, returning an error value.
 * Otherwise, ::sml_process will proceed with updated variables values.
 *
 * @remarks If read_state_cb is null previous callback will be unset.
 * @param sml The ::sml_object object.
 * @param read_state_cb A ::sml_read_state_cb.
 * @param data User data to ::sml_read_state_cb.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_read_state_callback(struct sml_object *sml, sml_read_state_cb read_state_cb, void *data);

/**
 * @brief Register a change callback
 *
 * It should be used to set a callback function to fetch the predicted
 * output variables. This callback will be called when the SML makes a prediction
 * for at least on output variable.
 *
 * @remarks If output_state_changed_cb is null previous callback will be unset.
 * @param sml The ::sml_object object.
 * @param output_state_changed_cb A ::sml_change_cb.
 * @param data User data to ::sml_read_state_cb.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_output_state_changed_callback(struct sml_object *sml, sml_change_cb output_state_changed_cb, void *data);


/**
 * @brief Disable the SML learning.
 *
 * All the reads will be ignore and they will not be used to learn new
 * patterns. However predictions can still be made.
 *
 * @remark Learning is enabled by default.
 *
 * @param sml The ::sml_object object.
 * @param disable @c true to disable learn, @c false otherwise
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_learn_disabled(struct sml_object *sml, bool disable);

/**
 * @brief Process variables and make predictions
 *
 * This function is used to read the variables via ::sml_read_state_cb, process them and
 * call ::sml_change_cb if necessary.
 *
 * @remarks It's required to set a read callback.
 *
 * @param sml The ::sml_object object.
 * @return ::0 on success
 * @return A negative value on failure.
 * @see ::sml_set_read_state_callback
 */
int sml_process(struct sml_object *sml);

/**
 * @brief Make a prediction based on the most recent observations.
 *
 * This is useful for making predictions without the normal SML flow,
 * without a mainloop and a registered ::sml_change_cb. Take a look
 * in the following example:
 *
 * @code{.c}
 *
 * //A simple example that will turn the light on if someone is in the room.
 * struct sml_variale PresenveVar, LightStateVar;
 * struct sml_object struct smlobject;
 * ARandomLightClass MyLight;
 *
 * ...
 *
 * sml_variable_set_value(struct smlobject, PresenceVar, 1.0); //User is present
 * sml_predict(struct smlobject);
 * if (sml_variable_get_value(struct smlobject, LightStateVar) == 1.0) //Should be on
 *  light_api_set_On(MyLight);
 * else
 *  light_api_set_Off(MyLight);
 * ...
 * @endcode
 *
 *
 * @remarks ::sml_change_cb and ::sml_read_state_cb won't be called !
 * @param sml The ::sml_object object.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_predict(struct sml_object *sml);

/**
 * @brief Set the stabilization hits.
 *
 * Amount of reads without input changes to consider input stable. Only stable
 * inputs are used to run predictions or to train the SML engine. This is
 * necessary to remove noises in read data that could lead to incorrect
 * previsions.
 *
 * @remarks It's defined as 5 reads by default. If set as 0, the input is
 * always considered stable.
 * @param sml The ::sml_object object.
 * @param hits The number of hits to be considered stable.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_stabilization_hits(struct sml_object *sml, uint16_t hits);

/**
 * @brief Erase all previous knowledge.
 *
 * Erases everything that the SML has learned, this will required that the SML
 * is trained again.
 *
 * @param sml The ::sml_object object.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_erase_knowledge(struct sml_object *sml);

/**
 * @brief Save the SML state on the disk.
 *
 * @param sml The ::sml_object object.
 * @param path A direcoty to save the engine files.
 * @return @c true on success.
 * @return @c false on failure.
 * @see ::sml_load
 */
bool sml_save(struct sml_object *sml, const char *path);

/**
 * @brief Load the SML state from the disk.
 *
 * @param sml The ::sml_object object.
 * @param path A direcoty to load the engine files.
 * @return @c true on success.
 * @return @c false on failure.
 * @see ::sml_save
 */
bool sml_load(struct sml_object *sml, const char *path);

/**
 * @brief Prints SML debug information.
 *
 * @param sml The ::sml_object object.
 * @param full @c true for full log, @c false otherwise.
 */
void sml_print_debug(struct sml_object *sml, bool full);

/**
 * @brief Set the file to be used to debug data changes in this engine
 *
 * This file will be used to log all calls to methods that changes the current
 * state of the sml engine data. This information may be used to reproduce the
 * execution of sml for debug purposes. Methods that configure the sml are not
 * logged.
 *
 * To use this feature, sml must be compiled with build type set to Debug.
 *
 * @param sml The ::sml_object object.
 * @param str A string with full path of file to be used to write debug data.
 * Use @c NULL or an empty string to disable data debug.
 * @return @ true if debug file was updated successfully. @c false if operation
 * failed or if sml was not compiled using Debug build type.
 *
 * @see ::sml_load_debug_log_file
 */
bool sml_set_debug_log_file(struct sml_object *sml, const char *str);

/**
 * @brief Load to current engine the debug data logged to a file
 *
 * Load all data logged in file set by ::sml_set_debug_log_file to current
 * engine. Used for debug purposes.
 *
 * To use this feature, sml must be compiled with build type set to Debug.
 *
 * @param sml The ::sml_object object.
 * @param str A string with full path of file to be used to load debug data.
 * @return @ true if loading debug was successful. @c false if operation
 * failed or if sml was not compiled using Debug build type.
 *
 * @see ::sml_set_debug_log_file
 *
 */
bool sml_load_debug_log_file(struct sml_object *sml, const char *str);

/**
 * @brief Set maximum memory that can be used to store observation history data.
 *
 * @remark max_size = 0 means infinite (it is also the default).
 * @param sml The ::sml_object object.
 * @param max_size Max memory size in bytes.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_max_memory_for_observations(struct sml_object *sml, unsigned int max_size);

/**
 * @}
 */
/**
 * @defgroup Variables Variables
 *
 * @brief SML Variable operations.
 * @{
 */

/**
 * @brief Return the input variables list.
 *
 * @param sml The ::sml_object object.
 * @return ::sml_variables_list on success.
 * @return @c NULL on failure.
 */
struct sml_variables_list *sml_get_input_list(struct sml_object *sml);

/**
 * @brief Return the output variables list.
 *
 * @param sml The ::sml_object object.
 * @return ::sml_variables_list on success.
 * @return @c NULL on failure.
 */
struct sml_variables_list *sml_get_output_list(struct sml_object *sml);

/**
 * @brief Create a new input variable
 *
 * New input variables start with NAN set as value.
 *
 * @param sml The ::sml_object object.
 * @param name The variable name. If length is greater
 * than ::SML_VARIABLE_NAME_MAX_LEN variable creation will fail.
 * @return ::sml_variable on success.
 * @return @c NULL on failure.
 */
struct sml_variable *sml_new_input(struct sml_object *sml, const char *name);

/**
 * @brief Create a new output variable
 *
 * If its value can't be calculated in a ::sml_process execution,
 * the value set on NAN will be used.
 *
 * @param sml The ::sml_object object.
 * @param name The variable name. If length is greater
 * than ::SML_VARIABLE_NAME_MAX_LEN variable creation will fail.
 * @return ::sml_variable on success.
 * @return @c NULL on failure.
 */
struct sml_variable *sml_new_output(struct sml_object *sml, const char *name);

/**
 * @brief Get input variable by name
 *
 * @param sml The ::sml_object object.
 * @param name The variable name.
 * @return ::sml_variable on success.
 * @return @c NULL on failure.
 */
struct sml_variable *sml_get_input(struct sml_object *sml, const char *name);

/**
 * @brief Get input variable by name
 *
 * @param sml The ::sml_object object.
 * @param name The variable name.
 * @return ::sml_variable on success.
 * @return @c NULL on failure.
 */
struct sml_variable *sml_get_output(struct sml_object *sml, const char *name);

/**
 * @brief Set the variable value.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable
 * @param value The desired value.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_variable_set_value(struct sml_object *sml, struct sml_variable *sml_variable, float value);

/**
 * @brief Get the variable current value.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable
 * @return @c true on success.
 * @return @c false on failure.
 */
float sml_variable_get_value(struct sml_object *sml, struct sml_variable *sml_variable);

/**
 * @brief Get ::sml_variable name.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param var_name Pointer to memory where name should be copied.
 * @param var_name_size Size of memory pointed by var_name.
 * @return @c 0 on success or error value.
 */
int sml_variable_get_name(struct sml_object *sml, struct sml_variable *sml_variable, char *var_name, size_t var_name_size);

/**
 * @brief Enable or disable a variable
 *
 * If a variable is disabled and is an input variable, its value will be ignored when trying to
 * predict the values or training the engine.
 * If it is an output variable, no prediction for this variable will be processed
 *
 * @remarks All variables are enabled by default.
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param enabled @c true to enable, @c false to disable.
 * @return @c true on success.
 * @return @c false on failure.
 */
int sml_variable_set_enabled(struct sml_object *sml, struct sml_variable *variable, bool enabled);

/**
 * @brief Check if a ::sml_variable is enabled.
 *
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_variable_is_enabled(struct sml_object *sml, struct sml_variable *variable);

/**
 * @brief Remove a variable from the engine.
 *
 * @remark Removing a variable from a running SML engine may
 * make it loose all previous knowledge. For temporary removals, use
 * ::sml_variable_set_enabled
 *
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable to be removed
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_remove_variable(struct sml_object *sml, struct sml_variable *variable);

/**
 * @brief Get the ::sml_variables_list size
 *
 * @param sml The ::sml_object Object.
 * @param list The ::sml_variables_list.
 * @return The list size
 */
uint16_t sml_variables_list_get_length(struct sml_object *sml, struct sml_variables_list *list);

/**
 * @brief Get ::sml_variable by index.
 *
 * @param sml The ::sml_object object.
 * @param list The ::sml_variables_list.
 * @param index The list index.
 * @return ::sml_variable on success.
 * @return @c NULL on failure..
 */
struct sml_variable *sml_variables_list_index(struct sml_object *sml, struct sml_variables_list *list, uint16_t index);

/**
 * @brief Check if ::sml_variable is present in a ::sml_variables_list
 *
 * @param sml The ::sml_object object.
 * @param list The ::sml_variables_list.
 * @param var The ::sml_variable.
 * @return @c true it is present.
 * @return @c false is is not present.
 */
bool sml_variables_list_contains(struct sml_object *sml, struct sml_variables_list *list, struct sml_variable *var);

/**
 * @brief Set variable range.
 *
 * If max is @c NAN, max value is not changed.
 * If min is @c NAN, min value is not changed.
 *
 * @remarks default value is -FLT_MAX for min and FLT_MAX for max.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param min Variable's min value.
 * @param max Variable's max value
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_variable_set_range(struct sml_object *sml, struct sml_variable *sml_variable, float min, float max);

/**
 * @brief Get variable range.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param min Variable where min value will be set. If @c NULL it is ignored.
 * @param max Variable where max value will be set. If @c NULL it is ignored.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_variable_get_range(struct sml_object *sml, struct sml_variable *sml_variable, float *min, float *max);

#define SML_VARIABLES_LIST_FOREACH(sml, list, len, var, i) \
    for (i = 0, len = sml_variables_list_get_length(sml, list); \
        i < len && ((var = sml_variables_list_index(sml, list, i))); \
        i++)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
