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
 * A good start to know more about fuzzy logic is visiting https://en.wikipedia.org/wiki/Fuzzy_logic
 * @{
 */

/**
 * @enum sml_fuzzy_snorm
 * @brief SNorm rules are also known as disjuction.
 *
 * @see ::sml_fuzzy_disjunction_set
 */
enum sml_fuzzy_snorm {
    SML_FUZZY_SNORM_NONE,     /**< No SNorm rule will be applied */
    SML_FUZZY_SNORM_ALGEBRAIC_SUM,     /**< A or B ->  A+B - (A*B)*/
    SML_FUZZY_SNORM_BOUNDED_SUM,     /**< A or B -> min(A+B,1) */
    SML_FUZZY_SNORM_DRASTIC_SUM,     /**< A or B -> if A=0 then B else if B=0 then A else 0 */
    SML_FUZZY_SNORM_EINSTEIN_SUM,     /**< A or B -> A+B/1+A*B  */
    SML_FUZZY_SNORM_HAMACHER_SUM,     /**< A or B -> if A=B=0 then 0 else A*B/A+B-A*B */
    SML_FUZZY_SNORM_MAXIMUM,     /**< A or B -> max(A,B) */
    SML_FUZZY_SNORM_NILPOTENT_MAXIMUM,     /**< A or B -> if A+B < 1 then max(A,B) else 1 */
    SML_FUZZY_SNORM_NORMALIZED_SUM,     /**< A or B ->  A+B/max(1, max(A,B))*/
};     /**< A fuzzy TNorm type. */

/**
 * @enum sml_fuzzy_tnorm
 * @brief TNorm rules are also known as conjuntion.
 *
 * @see ::sml_fuzzy_conjunction_set
 */
enum sml_fuzzy_tnorm {
    SML_FUZZY_TNORM_NONE,     /**< No TNorm rule will be applied */
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
    SML_FUZZY_DEFUZZIFIER_LARGEST_OF_MAXIMUM,     /**< The largest value of the maximun degrees of membership */
    SML_FUZZY_DEFUZZIFIER_MEAN_OF_MAXIMUM,     /**< The mean of the maximum degrees of membership */
    SML_FUZZY_DEFUZZIFIER_SMALLEST_OF_MAXIMUM,     /**< The smallest value of the maximun degrees of membership */
    SML_FUZZY_DEFUZZIFIER_WEIGHTED_AVERAGE,     /**< The avarage of the activation degrees multipled by a weight*/
    SML_FUZZY_DEFUZZIFIER_WEIGHTED_SUM,     /**< The sum of the activation degrees multipled by a weight*/
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
 * The conjuction rule is equivalent to the boolean and operation.
 * Example:
 * If the fuzzy engie encounters the following expression in a fuzzy rule, "... A and B ...".
 * Given that A is 0.6 and B is 0.8 and the disjuction operator is ::SML_FUZZY_TNORM_MINIMUM,
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
 * @brief Set the disjuction fuzzy rule.
 *
 * The disjunction rule is equivalent to the boolean or operation.
 * Example:
 * If the fuzzy engie encounters the following expression in a fuzzy rule, "... A or B ...".
 * Given that A is 0.6 and B is 0.8 and the disjuction operator is ::SML_FUZZY_SNORM_MAXIMUM,
 * the result of the operation will be 0.8
 *
 * @remark The default value is ::SML_FUZZY_SNORM_MAXIMUM
 *
 * @param sml The ::sml_object object.
 * @param norm The desired ::sml_fuzzy_snorm.
 *
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_fuzzy_snorm
 */
bool sml_fuzzy_disjunction_set(struct sml_object *sml, enum sml_fuzzy_snorm norm);

/**
 * @brief Rules below a given value will be ignored.
 *
 * Fuzzy rules may have weights associated with it, bigger weights
 * means bigger the relevance.
 *
 * @remark The default weight treshold is 0.05
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
 * @remarks The default accumulation is ::SML_FUZZY_SNORM_MAXIMUM
 *
 * @param sml The ::sml_object object.
 * @param sml_variable The ::sml_variable.
 * @param accumulation The ::sml_fuzzy_snorm rule.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_fuzzy_output_set_accumulation(struct sml_object *sml, struct sml_variable *sml_variable, enum sml_fuzzy_snorm accumulation);

/**
 * @brief Add a rectangle term for a variable.
 *
 * A rectangle term uses a mathematical function defined by its start and end
 * points and its height. For any X value from start to end, the function
 * value will be height. For all other X values, the function value will be
 * @c zero.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name.
 * @param start The point on the X axis that the term will start.
 * @param end The point on the X axis that the term will end.
 * @param height The term's height, usually 1.0.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_rectangle(struct sml_object *sml, struct sml_variable *variable, const char *name, float start, float end, float height);

/**
 * @brief Add a tirangle term for a variable.
 *
 * A triangle term uses is a mathematical function defined by 3 vertex (a, b c)
 * and its height. For X coordinates between vertex a and b, function value is
 * obtained by the linear function connecting points (vertex_a; 0) to
 * (vertex_b; height). In vertex_b, function value will be the maximum possible
 * value (height) and from vertex_b to vertex_c, function value is obtained by
 * the linear function connecting opints (vertex_b; height) to (vertex_c; 0).
 * For all other X values, the function value will be @c zero.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name.
 * @param vertex_a The point on the X axis that the term will start.
 * @param vertex_b The point on the X axis that will be the middle of the triangle.
 * @param vertex_c the point on the X axis that the term will end.
 * @param height The term's height, usually 1.0.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_triangle(struct sml_object *sml, struct sml_variable *variable, const char *name, float vertex_a, float vertex_b, float vertex_c, float height);

/**
 * @brief Add a cosine term for a variable.
 *
 * Cosine term value is obtained by the function value from a cosine function
 * centered in X coordinate center and with width defined by width paramenter.
 * The maximum value (height) of the cosine function is in X coordinate center.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name.
 * @param center The center of the cosine curve.
 * @param width The width of the cosine curve.
 * @param height The term's height, usually 1.0.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_cosine(struct sml_object *sml, struct sml_variable *variable, const char *name, float center, float width, float height);

/**
 * @brief Add a gaussian term for a variable.
 *
 * Gaussian term value is obtained by the function value from a gaussian
 * function defined by the parametes mean, standard_deviation. The maximum
 * value of the gaussian function is the value of height parameter.
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name.
 * @param mean The mean of the gaussian curve.
 * @param standard_deviation The standard deviation of the curve.
 * @param height The term's height, usually 1.0.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_gaussian(struct sml_object *sml, struct sml_variable *variable, const char *name, float mean, float standard_deviation, float height);

/**
 * @brief Add a ramp term for a variable.
 *
 * A ramp term uses a mathematical function defined by its start and end points
 * and its height. Start parameters is the coordinate in X axis where the ramp
 * is in its minimum value (@c zero). End parameter is the coordinate in Y axis
 * where the ramp is in its maximum value (height). If start < end we will have
 * a positive ramp. If end < start we will have a negative ramp.
 * <BR> For example:
 *
 * @code{.c}
 * //this will create a ramp going up, connecting points (0.3; 0.0) and (0.7; 1.0). In X position 0.3, the ramp value will be 0.0. In X position 0.7, the ramp value will be 1.0
 *   sml_fuzzy_variable_add_term_ramp    (sml, var, "ramp_up", 0.3, 0.7, 1.0);
 *
 * //this will create a ramp going down, connecting points (0.3; 1.0) and (0.7; 0.0). In X position 0.3, the ramp value will be 1.0. In X position 0.7, the ramp value will be 0.0
 * sml_fuzzy_variable_add_term_ramp  (sml, var, "ramp_up", 0.7, 0.3, 1.0);
 * @endcode
 *
 * @remark The term name can not contain spaces!
 * @param sml The ::sml_object object.
 * @param variable The ::sml_variable.
 * @param name The term's name.
 * @param start The start of the ramp on the X axis.
 * @param end The end of the ramp on the X axis.
 * @param height The term's maximum value, usually 1.0.
 * @return @c ::sml_fuzzy_term on success.
 * @return @c NULL on failure.
 */
struct sml_fuzzy_term *sml_fuzzy_variable_add_term_ramp(struct sml_object *sml, struct sml_variable *variable, const char *name, float start, float end, float height);

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
 * (Weekday and TimeOfTheDay) we can ha ve the following rules:
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
