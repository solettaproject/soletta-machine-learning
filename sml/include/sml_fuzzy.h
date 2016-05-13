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
#include <sml.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @defgroup Fuzzy_Engine Fuzzy engine
 * @brief The SML naive fuzzy logic engine.
 *
 * The SML may use a modified fuzzy engine to predict values. The main difference
 * from a normal fuzzy engine, is that the SML engine will create the fuzzy rules by
 * itself.
 * A good start to know more about fuzzy logic is visiting
 * https://github.com/solettaproject/soletta/wiki/Soletta-Machine-Learning#fuzzy-logic
 * @{
 */

#define SML_TERM_NAME_MAX_LEN (127) /**< Maximum size of terms name */

/**
 * @enum sml_fuzzy_snorm
 * @brief SNorm rules are also known as accumulation.
 *
 * @see ::sml_fuzzy_output_set_accumulation
 */
enum sml_fuzzy_snorm {
    SML_FUZZY_SNORM_ALGEBRAIC_SUM,     /**< A or B ->  A+B - (A*B)*/
    SML_FUZZY_SNORM_BOUNDED_SUM,     /**< A or B -> min(A+B,1) */
    SML_FUZZY_SNORM_DRASTIC_SUM,     /**< A or B -> if A=0 then B else if B=0 then A else 0 */
    SML_FUZZY_SNORM_EINSTEIN_SUM,     /**< A or B -> A+B/1+A*B  */
    SML_FUZZY_SNORM_HAMACHER_SUM,     /**< A or B -> if A=B=0 then 0 else A*B/A+B-A*B */
    SML_FUZZY_SNORM_MAXIMUM,     /**< A or B -> max(A,B) */
    SML_FUZZY_SNORM_NILPOTENT_MAXIMUM,     /**< A or B -> if A+B < 1 then max(A,B) else 1 */
    SML_FUZZY_SNORM_NORMALIZED_SUM,     /**< A or B ->  A+B/max(1, max(A,B))*/
};     /**< A fuzzy SNorm type. */

/**
 * @enum sml_fuzzy_tnorm
 * @brief TNorm rules are also known as conjunction.
 *
 * @see ::sml_fuzzy_conjunction_set
 */
enum sml_fuzzy_tnorm {
    SML_FUZZY_TNORM_ALGEBRAIC_PRODUCT,     /**< A and B -> A*B */
    SML_FUZZY_TNORM_BOUNDED_DIFFERENCE,     /**< A and B -> max(A+B-1, 0)*/
    SML_FUZZY_TNORM_DRASTIC_PRODUCT,     /**< A and B -> if A=1 then B else if B=1 then A else 0*/
    SML_FUZZY_TNORM_EINSTEIN_PRODUCT,     /**< A and B -> A*B/2-(A+B-A*B)*/
    SML_FUZZY_TNORM_HAMACHER_PRODUCT,     /**< A and B -> A*B/A+B-A*B*/
    SML_FUZZY_TNORM_MINIMUM,     /**< A and B -> min(A,B)*/
    SML_FUZZY_TNORM_NILPOTENT_MINIMUM,     /**< A and B -> if A+B > 1 then min(A,B) else 0 */
};     /**< A fuzzy TNorm type. */

/**
 * @enum sml_fuzzy_defuzzifier
 *
 * @brief The types of defuzzifier
 *
 * @see ::sml_fuzzy_output_set_defuzzifier
 */
enum sml_fuzzy_defuzzifier {
    SML_FUZZY_DEFUZZIFIER_BISECTOR,     /**< The point that divide the curve into two equal sub-regions */
    SML_FUZZY_DEFUZZIFIER_CENTROID,     /**< The center of the area under the curve */
    SML_FUZZY_DEFUZZIFIER_LARGEST_OF_MAXIMUM,     /**< The largest value of the maximum degrees of membership */
    SML_FUZZY_DEFUZZIFIER_MEAN_OF_MAXIMUM,     /**< The mean of the maximum degrees of membership */
    SML_FUZZY_DEFUZZIFIER_SMALLEST_OF_MAXIMUM,     /**< The smallest value of the maximun degrees of membership */
    SML_FUZZY_DEFUZZIFIER_WEIGHTED_AVERAGE,     /**< The average of the activation degrees multiplied by a weight*/
    SML_FUZZY_DEFUZZIFIER_WEIGHTED_SUM,     /**< The sum of the activation degrees multiplied by a weight*/
};     /**< A fuzzy Defuzzifier type. */

/**
 * @struct sml_fuzzy_term
 *
 * In Fuzzy Logic Engine, fuzzy terms are created using mathematical functions.
 * The membership of a fuzzy value to a term is defined by the function
 * applied to that value.
 *
 * Fuzzy Logic Engine supports 5 different terms:
 *
 * @li Ramp
 * @li Triangle
 * @li Rectangle
 * @li Cosine
 * @li Gaussian
 */
struct sml_fuzzy_term;     /**< A fuzzy term object. */

/**
 * @brief Creates a SML fuzzy engine.
 *
 * @remark It must be freed with ::sml_free after usage is done.
 *
 * @return A ::sml_object object on success.
 * @return @c NULL on failure.
 *
 * @see ::sml_free
 * @see ::sml_object
 */
struct sml_object *sml_fuzzy_new(void);

/**
 * @brief Check if the SML object is a fuzzy engine.
 *
 * @param sml The ::sml_object object.
 * @return @c true If it is fuzzy.
 * @return @c false If it is not fuzzy.
 */
bool sml_is_fuzzy(struct sml_object *sml);

/**
 * @brief Check if SML was built with fuzzy support.
 *
 * @return @c true If it is supported.
 * @return @c false If not supported.
 */
bool sml_fuzzy_supported(void);

/**
 * @brief Set the conjunction fuzzy rule.
 *
 * The conjunction rule is equivalent to the boolean and operation.
 * Example:
 * If the fuzzy engine encounters the following expression in a fuzzy rule, "... A and B ...".
 * Given that A is 0.6 and B is 0.8 and the conjunction operator is ::SML_FUZZY_TNORM_MINIMUM,
 * the result of the operation will be 0.6
 *
 * @remark The default value is ::SML_FUZZY_TNORM_MINIMUM
 *
 * @param sml The ::sml_object object.
 * @param norm The desired ::sml_fuzzy_tnorm.
 *
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_fuzzy_tnorm
 */
bool sml_fuzzy_conjunction_set(struct sml_object *sml, enum sml_fuzzy_tnorm norm);

/**
 * @brief Rules below a given value will be ignored.
 *
 * Fuzzy rules created by fuzzy engine have weights associated with it, bigger
 * weights means bigger relevance. The range of weight value is from
 * @c zero (no relevance) to @c 1 (very relevant).
 *
 * Weight threshold property is used to select if rules with low relevance are
 * going to be used in output predictions. If weight_threshold is a value close
 * to @c zero, even low relevance rules are going to be used. If
 * weight_threshold is close to @c 1, only very relevant rules are going to be
 * used.
 *
 * Values greater than @c 1 or lower than @c 0 are not accepted and rules with
 * weight @c zero are always ignored.
 *
 * @remark The default weight threshold is 0.05
 *
 * @param sml The ::sml_object object.
 * @param weight_threshold The desired threshold.
 *
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_set_rule_weight_threshold(struct sml_object *sml, float weight_threshold);

/**
 * @brief Set the defuzzifier for an output variable.
 *
 * Fuzzy defuzzifiers are used to convert degrees of membership to a crisp value (a real number)
 * For example if from a given rule the fuzzy engine predict that the degrees of membership
 * for a given variable for the term Hot is 10% and for the term Warm is 90%. Using the
 * defuzzifier it will be possible the compute a real number to represent these degrees of membership.
 *
 * @remarks Defuzzifier only works for outputs variables, also the
 * defuzzifier_resulution will be ignore if ::sml_fuzzy_defuzzifier is
 * ::SML_FUZZY_DEFUZZIFIER_WEIGHTED_SUM or ::SML_FUZZY_DEFUZZIFIER_WEIGHTED_AVERAGE.
 * The default defuzzifier is ::SML_FUZZY_DEFUZZIFIER_CENTROID.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable to use the given defuzzifier.
 * @param defuzzifier The ::sml_fuzzy_defuzzifier object.
 * @param defuzzifier_resolution A constant that will be used by the defuzzifier to obtain the crisp value.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_output_set_defuzzifier(struct sml_object *sml, struct sml_variable *sml_variable, enum sml_fuzzy_defuzzifier defuzzifier, int defuzzifier_resolution);

/**
 * @brief Set the output accumulation
 *
 * The output accumulation rule is the strategy that will be used by fuzzy
 * lib to select the final result output value using the output values
 * calculated for each fuzzy rule.
 *
 * Example:
 * If the output value for variable A is 0.6 for Rule 1, the output value for
 * variable A is 0.4 for Rule 2 and the selected accumulator is
 * ::SML_FUZZY_SNORM_MAXIMUM, then the final output value will be 0.6
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param accumulation The ::sml_fuzzy_snorm rule.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_output_set_accumulation(struct sml_object *sml, struct sml_variable *sml_variable, enum sml_fuzzy_snorm accumulation);

/**
 * @brief Set the default term width used by fuzzy to create terms
 *
 * If ::sml_process is called and a variable has no terms fuzzy engine will
 * automatically create terms for this variable. Some properties are important
 * help fuzzy engine to improve the quality of the created terms.
 *
 * Width is the width of each created term. Other important properties are
 * is_id, set by ::sml_fuzzy_variable_set_is_id and range (min and max), set by
 * ::sml_variable_set_range.
 *
 * For example, for the variable created by the following code:
 * @code{.c}
 * struct sml_variable *var = sml_new_input(sml, "my_var");
 * sml_variable_set_range(sml, var, 0, 5);
 * sml_fuzzy_variable_set_default_term_width(sml, var, 1);
 * @endcode
 *
 * Fuzzy engine will create 5 terms:
 * - ramp from 0 to 1
 * - triangle from 1 to 2, with peak at 0.5
 * - triangle from 2 to 3, with peak at 1.5
 * - triangle from 3 to 4, with peak at 3.5
 * - ramp from 4 to 5.
 *
 * After that, a small overlap will be added between each term. The default
 * overlap value is 10% of width, in this case, 0.1. So at the end we would
 * have:
 * - ramp from 0 to 1.1
 * - triangle from 0.9 to 2.1, with peak at 0.5
 * - triangle from 1.9 to 3.1, with peak at 1.5
 * - triangle from 2.9 to 4.1, with peak at 3.5
 * - ramp from 3.9 to 5.
 *
 * In case the variable is an id, the first and last element will have a size
 * width/2. This is necessary so we can have a constant distance from the peak
 * of each term. This distance is always equal width.
 *
 * For example, for the variable created by the following code:
 * @code{.c}
 * struct sml_variable *var = sml_new_input(sml, "my_var");
 * sml_variable_set_range(sml, var, 0, 5);
 * sml_fuzzy_variable_set_default_term_width(sml, var, 1);
 * sml_fuzzy_variable_set_is_id(sml, var, true);
 * @endcode
 * We would have 5 terms:
 * - ramp from 0 to 0.6
 * - triangle from 0.4 to 1.6, with peak at 1
 * - triangle from 1.4 to 2.6 with peak at 2
 * - triangle from 2.4 to 3.6, with peak at 3
 * - triange from 3.4 to 4.6, with peak at 4
 * - ramp from 4.4 to 5
 *
 * Terms autocreated are also rebalanced every time ::sml_variable_set_range is
 * called. So new terms are created to make sure all variable range is covered.
 * And terms that are no longer necessary are removed.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param width Default term width
 * @remark The default value is @c NAN
 *
 * @see ::sml_fuzzy_variable_get_default_term_width
 */
bool sml_fuzzy_variable_set_default_term_width(struct sml_object *sml, struct sml_variable *sml_variable, float width);

/**
 * @brief Get the default term width used by fuzzy to create terms.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 *
 * @see ::sml_fuzzy_variable_set_default_term_width
 */
float sml_fuzzy_variable_get_default_term_width(struct sml_object *sml, struct sml_variable *sml_variable);

/**
 * @brief Check if this variable is used as an id field.
 *
 * The is_id property is used by fuzzy engine when it is creating terms
 * automatically. Terms created by fuzzy engine will be optimized to be
 * used by variables that keeps ids.
 *
 * For more information take a look at ::sml_fuzzy_variable_set_default_term_width
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param is_id @c true if this variable should be considered an id. @c false otherwise
 * @remark The default value is @c false
 *
 * @see ::sml_fuzzy_variable_get_is_id
 */
bool sml_fuzzy_variable_set_is_id(struct sml_object *sml, struct sml_variable *sml_variable, bool is_id);

/**
 * @brief Check if this variable is used as an id field.
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @return @c true if sml_variable is an id field
 * @return @c false if sml_variable isn't an id field
 *
 * @see ::sml_fuzzy_variable_set_is_id
 */
bool sml_fuzzy_variable_get_is_id(struct sml_object *sml, struct sml_variable *sml_variable);

/**
 * @brief Add a rectangle term for a variable.
 *
 * A rectangle term uses a mathematical function defined by its start and end
 * points. For any X value from start to end, the function
 * value will be 1.0. For all other X values, the function value will be
 * @c zero.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name. Its length must be smaller
 * than @ref SML_TERM_NAME_MAX_LEN.
 * @param start The point on the X axis that the term will start.
 * @param end The point on the X axis that the term will end.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_rectangle(struct sml_object *sml, struct sml_variable *variable, const char *name, float start, float end);

/**
 * @brief Add a triangle term for a variable.
 *
 * A triangle term uses is a mathematical function defined by 3 vertex (a, b c).
 * For X coordinates between vertex a and b, function value is
 * obtained by the linear function connecting points (vertex_a; 0) to
 * (vertex_b; 1). In vertex_b, function value will 1.0  and from vertex_b to vertex_c,
 * function value is obtained by the linear function connecting points (vertex_b; 1.0)
 * to (vertex_c; 0).
 * For all other X values, the function value will be @c zero.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name. Its length must be smaller
 * than @ref SML_TERM_NAME_MAX_LEN.
 * @param vertex_a The point on the X axis that the term will start.
 * @param vertex_b The point on the X axis that will be the middle of the triangle.
 * @param vertex_c the point on the X axis that the term will end.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_triangle(struct sml_object *sml, struct sml_variable *variable, const char *name, float vertex_a, float vertex_b, float vertex_c);

/**
 * @brief Add a cosine term for a variable.
 *
 * Cosine term value is obtained by the function value from a cosine function
 * centered in X coordinate center and with width defined by width parameter.
 * The maximum value (1.0) of the cosine function is in X coordinate center.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name. Its length must be smaller
 * than @ref SML_TERM_NAME_MAX_LEN.
 * @param center The center of the cosine curve.
 * @param width The width of the cosine curve.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_cosine(struct sml_object *sml, struct sml_variable *variable, const char *name, float center, float width);

/**
 * @brief Add a gaussian term for a variable.
 *
 * Gaussian term value is obtained by the function value from a gaussian
 * function defined by the parameters mean, standard_deviation. The maximum
 * value in Y axis of the gaussian function is 1.0.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name. Its length must be smaller
 * than @ref SML_TERM_NAME_MAX_LEN.
 * @param mean The mean of the gaussian curve.
 * @param standard_deviation The standard deviation of the curve.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_gaussian(struct sml_object *sml, struct sml_variable *variable, const char *name, float mean, float standard_deviation);

/**
 * @brief Add a ramp term for a variable.
 *
 * A ramp term uses a mathematical function defined by its start and end points.
 * Start parameters is the coordinate in X axis where the ramp
 * is in its minimum value (@c zero). End parameter is the coordinate in Y axis
 * where the ramp is in its maximum value (1.0). If start < end we will have
 * a positive ramp. If end < start we will have a negative ramp.
 * <BR> For example:
 *
 * @code{.c}
 * //this will create a ramp going up, connecting points (0.3; 0.0) and (0.7; 1.0). In X position 0.3, the ramp value will be 0.0. In X position 0.7, the ramp value will be 1.0
 *   sml_fuzzy_variable_add_term_ramp    (sml, var, "ramp_up", 0.3, 0.7);
 *
 * //this will create a ramp going down, connecting points (0.3; 1.0) and (0.7; 0.0). In X position 0.3, the ramp value will be 1.0. In X position 0.7, the ramp value will be 0.0
 * sml_fuzzy_variable_add_term_ramp  (sml, var, "ramp_up", 0.7, 0.3);
 * @endcode
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name. Its length must be smaller
 * than @ref SML_TERM_NAME_MAX_LEN.
 * @param start The start of the ramp on the X axis.
 * @param end The end of the ramp on the X axis.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_ramp(struct sml_object *sml, struct sml_variable *variable, const char *name, float start, float end);

/**
 * @brief Set terms autobalance
 *
 * Defining terms overlaps is a hard task, sometimes the user defined terms are not
 * well suited for an application. If the term definitions are poor, the fuzzy engine
 * the fuzzy predictions will be poor as well. If auto balance is enabled and if the
 * fuzzy engine detects that the terms are not well set, it will recreate the terms.
 *
 * @remarks Terms auto balance is disabled by default
 *
 * @param sml The ::sml_object object.
 * @param variable_terms_auto_balance @c true to enable, @c false to disable.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_set_variable_terms_auto_balance(struct sml_object *sml, bool variable_terms_auto_balance);

/**
 * @brief Remove a fuzzy term from the engine.
 *
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable to remove the term.
 * @param term The The ::sml_fuzzy_term to be removed.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_variable_remove_term(struct sml_object *sml, struct sml_variable *variable, struct sml_fuzzy_term *term);

/**
 * @brief Disable/Enable simplification.
 *
 * The rule simplification uses a heuristic to try to simplify the fuzzy rules.
 * For example, if the fuzzy engine is controlling a a home Light using two variables
 * (Weekday and TimeOfTheDay) we can have the following rules:
 * <BLOCKQUOTE>
 * If Weekday is Sunday and TimeOfTheDay is Night then Light is On. <BR>
 * If Weekday is Monday and TimeOfTheDay is Night then Light is On. <BR>
 * ....<BR>
 * If Weekday is Saturday and TimeOfTheDay is Night then Light is On.<BR>
 * </BLOCKQUOTE>
 * It is clear that the Weekday is not important, if TimeOfTheDay is Night then
 * the Light should be On. Knowing that, the fuzzy engine will simplify those rules
 * into one rule:
 * <BLOCKQUOTE>
 * If TimeOfTheDay is Night then Light is On.
 * </BLOCKQUOTE>
 * However if after the simplification a new rule is created that denies the
 * simplificated rule, the simplification is undone.
 *
 * @remarks Rule simplification is enabled by default.
 *
 * @param sml The ::sml_object object.
 * @param disabled @c true to disable, @c false to enable.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_set_simplification_disabled(struct sml_object *sml, bool disabled);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
