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
 * @defgroup Ann_Engine Neural network engine
 * @brief The SML neural network engine.
 *
 * \anchor Neural_Network_Engine_Introduction
 *
 * A neural network consists in a set of neurons that are inter-connected
 * and distributed in layers, usually three (Input, hidden and output layers).
 * For every connection between neurons there is a weight associated to it,
 * these weights are initialized randomly with values between -0.2 and 0.2 and
 * adjusted during the training phase, so the neural network
 * output predict the right value.
 *
 * The neuron is the basic unit of the neural network and it's responsible for
 * producing an output value based on its inputs, the output is calculated
 * using the following formula:
 * <br> <center> \f$ f(\sum_{i=1}^{n} W_i * I_i) = Out (1)\f$ </center>
 * Where:
 * <ul>
 *    <li>\f$i\f$ is the index of the input.
 *    <li>\f$n\f$ is the number of inputs.
 *    <li>\f$I_i\f$ is the input value for input i.
 *    <li>\f$W_i\f$ is the weight value for the input i.
 *    <li>\f$Out\f$ is the output value.
 *    <li>\f$f(x)\f$ is the activation function.
 * </ul>
 *
 * The activation function is chosen by the user and it can be any differentiable function,
 * however most problems can be solved using the Sigmoid function.
 *
 * As an example, imagine that one wants to predict if a light should be on/off if Bob and Alice
 * is present in the room using the following trained neural network.
 * @image html ann.png
 *
 * Consider that Bob is present (input is 1) and Alice (input is 0) is not.
 *
 * The first step is to provide the Bob's and Alice's presence to the input neurons,
 * In a neuron network the input neurons are special, because they do not apply the formula (1) to produce an output.
 * The output value from a input neuron is the input value itself, so in this case the
 * N1 and N2 neurons will output 1 and 0 respectively.
 *
 * Consider that all neurons are using the sigmoid function defined as:
 * <br><center>\f$\frac{1}{1 + e^{-x}}\f$</center>
 *
 * Using the formula (1) the N3's output will be:
 * \f{eqnarray*}{
 *     OutputN3 &=& \frac{1}{1 + e^{-x}}\\
 *              &=& \frac{1}{1 + e^{-((1 * 3) + (0 * 1.2))}}\\
 *              &=& 0.95
 * \f}
 *
 * The same thing for N4:
 * \f{eqnarray*}{
 *     OutputN4 &=& \frac{1}{1 + e^{-x}}\\
 *              &=& \frac{1}{1 + e^{-((1 * 3.4) + (0 * 2.5))}}\\
 *              &=& 0.96
 * \f}
 *
 * Finally the Light state (N5):
 * \f{eqnarray*}{
 *     Light state  &=& \frac{1}{1 + e^{-x}}\\
 *              &=& \frac{1}{1 + e^{-((0.95 * 4) + (0.96 * 5))}}\\
 *              &=& 0.99
 * \f}
 *
 * The neural network predict that the light state is 0.99, as it's very close to 1 we can consider
 * that the light should be On.
 *
 * @remark The values used in the neural network above
 * (inputs, outputs, neuron weights) were chosen randomly.
 *
 * The example above uses two neurons in the hidden layer, however for some problem, two neurons is not good enough.
 * There is no silver bullet about how many neurons one should use in the hidden layer, this number is obtained by trial and error.
 * SML can handle this automatically, during the training phase SML will automatically choose the neural network topology
 * (how many neurons the hidden layer must have), it will also decide which is the best activation function.
 *
 * The SML neural network engine has two methods of operation, these methods try
 * to reduce or eliminate a problem called catastrophic forgetting that affects neural
 * networks.
 *
 * Basically catastrophic forgetting is a problem that the neural network may forget everything
 * that has learnt in the past and only accumulate recent memory. This happens due the nature
 * of their training. In order to reduce this problem the following methods were implemented.
 *
 * The first method is called pseudo-rehearsal (the default one), in this method only one neural network is created
 * and every time it needs be retrained, random inputs are generated and
 * feed to the network. The corresponding outputs are stored and used to train the neural network
 * with the new collected data.
 *
 * The second method of operation consists in creating N neural networks,
 * that are very specific for each pattern that SML encounters and every time the SML wants to make a prediction,
 * it will choose which is the best neural network for the current situation.
 * It is possible to set a limit of how many neural networks SML will have in memory, this limit
 * can be set with ::sml_ann_set_cache_max_size. This cache implements the LRU algorithm so,
 * neural networks that were not recent used will be deleted.
 *
 *
 * To know more about catastrophic forgetting: https://en.wikipedia.org/wiki/Catastrophic_interference
 *  @{
 */

/**
 * @enum sml_ann_training_algorithm
 * @brief Algorithm types used to train a neural network.
 * @see ::sml_ann_set_training_algorithm
 */
enum sml_ann_training_algorithm {
    SML_ANN_TRAINING_ALGORITHM_QUICKPROP,     /**< Faster than the standard backpropagation.
                                                 Uses the gradient information to update the weights;
                                                 It's based on the Newton's method.
                                               */
    SML_ANN_TRAINING_ALGORITHM_RPROP     /**< Uses the sign of the gradient to update the weights.*/
};     /**< The neural networks training type. */


/**
 * @enum sml_ann_activation_function
 * @brief The functions that are used by the neurons to produce an output.
 * @see ::sml_ann_set_activation_function_candidates
 */
enum sml_ann_activation_function {
    SML_ANN_ACTIVATION_FUNCTION_SIGMOID,     /**< Sigmoid function. Defined for 0 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_SIGMOID_SYMMETRIC,     /**< Sigmoid function. Defined for -1 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN,     /**< Gaussian function. Defined for  0 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_GAUSSIAN_SYMMETRIC,     /**< Gaussian function. Defined for -1 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_ELLIOT,     /**< Elliot function. Defined for 0 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_ELLIOT_SYMMETRIC,     /**< Elliot function. Defined for -1 < y < 1*/
    SML_ANN_ACTIVATION_FUNCTION_COS,      /**< Cosinus function. Defined for 0 <= y <= 1 */
    SML_ANN_ACTIVATION_FUNCTION_COS_SYMMETRIC,     /**< Cosinus function. Defined for -1 <= y <= 1 */
    SML_ANN_ACTIVATION_FUNCTION_SIN,     /**< Sinus function. Defined for 0 <= y <= 1 */
    SML_ANN_ACTIVATION_FUNCTION_SIN_SYMMETRIC     /**< Sinus function. Defined for -1 <= y <= 1 */
};     /**< The neuron activation functions */

/**
 * @brief Creates a SML neural networks engine.
 *
 * @remark It must be freed with ::sml_free after usage is done.
 *
 * @return A ::sml_object object on success.
 * @return @c NULL on failure.
 *
 * @see ::sml_free
 * @see ::sml_object
 */
struct sml_object *sml_ann_new(void);

/**
 * @brief Check if the SML object is a neural network engine.
 *
 * @param sml The ::sml_object object.
 * @return @c true If it is fuzzy.
 * @return @c false If it is not fuzzy.
 */
bool sml_is_ann(struct sml_object *sml);

/**
 * @brief Check if SML was built with neural networks support.
 *
 * @return @c true If it has neural network support.
 * @return @c false If it is has not neural network support.
 */
bool sml_ann_supported(void);

/**
 * @brief Set the neural network training algorithm.
 *
 * The training algorithm is responsible for adjusting the neural network weights.
 *
 * @remark The default training algorithm is ::SML_ANN_TRAINING_ALGORITHM_QUICKPROP
 *
 * @param sml The ::sml_object object.
 * @param algorithm The ::sml_ann_training_algorithm algorithm.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_set_training_algorithm(struct sml_object *sml, enum sml_ann_training_algorithm algorithm);

/**
 * @brief Set the neural networks activation function candidates.
 *
 * Activation functions resides inside the neurons and they are responsible for producing the
 * neuron output value. As choosing the correct activation functions may required a
 * lot of trial and error tests, the SML uses an algorithm that tries to suit the best
 * activation functions for a given problem.
 *
 * @remark By default all ::sml_ann_activation_function are used as candidates.
 *
 * @param sml The ::sml_object object.
 * @param functions The ::sml_ann_activation_function functions vector.
 * @param size The size of the vector functions.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_set_activation_function_candidates(struct sml_object *sml, enum sml_ann_activation_function *functions, unsigned int size);


/**
 * @brief Set the maximum number of neurons in the network
 *
 * SML automatically add neurons to the hidden layers when training the neural network,
 * you can prevent the neural network to grow too big by setting the max neurons.
 * Larger networks are more difficult to train, thus required more time. However,
 * smaller networks will have poor predictions.
 *
 * @remark The default max_neurons is 5
 *
 * @param sml The ::sml_object object.
 * @param max_neurons The maximum number of neurons in the neural network.
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_set_max_neurons(struct sml_object *sml, unsigned int max_neurons);

/**
 * @brief Set the number of neural network candidates
 *
 * During the training phase the SML will choose the network topology by itself.
 * To do so, it will create (M * 4) neuron candidates (where M is
 * the number of activation function candidates) and then N (where N is the number of candidate groups)
 * candidate groups will be created, ending up with M*4*N candidate neurons.
 * The only different between these candidate groups is the initial weight values.
 *
 * @remark The default number of candidates is 6
 *
 * @param sml The ::sml_object object.
 * @param candidate_groups The number of candidate groups.
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_ann_set_max_neurons
 * @see ::sml_ann_set_activation_function_candidates
 */
bool sml_ann_set_candidate_groups(struct sml_object *sml, unsigned int candidate_groups);

/**
 * @brief Set the neural network train epochs
 *
 * The training epochs is used to know when to stop the training.
 * If the desired error is never reached, the training phase will stop
 * when it reaches the training_epochs value.
 *
 * @remark The default training epochs is 300
 *
 * @param sml The ::sml_object object.
 * @param training_epochs The number of training_epochs
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_ann_set_desired_error
 */
bool sml_ann_set_training_epochs(struct sml_object *sml, unsigned int training_epochs);

/**
 * @brief Set the neural network desired error
 *
 * This is used as a shortcut to stop the training phase. If
 * the neural network error is equals or below desired_error the train will be stopped.
 *
 * @remark The default desired error is 0.001
 *
 * @param sml The ::sml_object object.
 * @param desired_error The desired error
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_set_desired_error(struct sml_object *sml, float desired_error);

/**
 * @brief Set the maximum number of neural networks in the cache.
 *
 * This cache is used to store the neural networks that will be used to predict
 * output values. Setting this limit is only necessary if one sets pseudorehearsal
 * strategy to @c false, otherwise it's ignored.
 *
 * @remark The default cache size is 30.
 * @remark 0 means "infinite" cache size.
 *
 * @param sml The ::sml_object object.
 * @param max_size The max cache size
 * @return @c true on success.
 * @return @c false on failure.
 *
 * @see ::sml_ann_use_pseudorehearsal_strategy
 */
bool sml_ann_set_cache_max_size(struct sml_object *sml, unsigned int max_size);

/**
 * @brief Set the required number of observations to train the neural network.
 *
 * The SML will only train the neural network when the observation count reaches
 * required_observation. However this number is only a hint to SML, because if SML detects
 * that the provided number is not enough, the required_observations will grow.
 * There is a way to control how much memory the SML will use to store observations,
 * this memory cap can be set with ::sml_set_max_memory_for_observations
 *
 * @remark The default required observations is 2500
 *
 * @param sml The ::sml_object object.
 * @param required_observations The number of required observations
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_set_initial_required_observations(struct sml_object *sml, unsigned int required_observations);

/**
 * @brief Set the pseudorehearsal strategy
 *
 * For more information about the pseudorehearsal strategy look at the
 * \ref Neural_Network_Engine_Introduction
 * @remark The default value for pseudorehearsal is @c true.
 *
 * @param sml The ::sml_object object.
 * @param use_pseudorehearsal @c true to enable, @c false to disable
 * @return @c true on success.
 * @return @c false on failure.
 */
bool sml_ann_use_pseudorehearsal_strategy(struct sml_object *sml, bool use_pseudorehearsal);
/**
 * @}
 */
#ifdef __cplusplus
}
#endif
