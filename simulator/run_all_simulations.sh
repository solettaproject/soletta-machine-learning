#!/bin/bash

# This file is part of the Soletta Project
#
# Copyright (C) 2015 Intel Corporation. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

results_dir=""

function parse_results()
{
    prefix=$1
    total=0
    count=0
    total_duration=0
    max_it_duration=0
    avg_anns=0
    anns=0
    observations=0
    avg_observations=0
    for result in ${results_dir}/result_${prefix}_${conf}.${data}.*; do
        value=$(cat $result | grep "Variable${i}"|cut -d ' ' -f 10)
        it_duration=$(cat $result | grep "Max Iteration Duration"|cut -d ' ' -f 6)
        duration=$(cat $result | grep "Average Duration"|cut -d ' ' -f 5)
        if [[ "$prefix" == "ann" ]]; then
            anns=$(cat $result | grep "Total ANNs created" | cut -d ' ' -f 6 | sed 's/[()]//g')
            avg_anns=$(echo $anns + $avg_anns | bc -l)
            observations=$(cat $result | grep "Required observations" | cut -d ' ' -f 5)
            avg_observations=$(echo $observations + $avg_observations | bc -l)
        fi
        total=$(echo $total + $value | bc -l)
        total_duration=$(echo $total_duration + $duration | bc -l)
        if [ $(echo "$max_it_duration < $it_duration" | bc -l) -eq 1 ]; then
            max_it_duration=$it_duration
        fi
        count=$(echo $count + 1 | bc)
    done
    duration=$(echo "scale=2; $total_duration / $count" | bc -l)
    result=$(echo "scale=4; $total / $count" | bc -l)

    if [[ "$prefix" == "ann" ]]; then
        avg_anns=$(echo "scale=2; $avg_anns / $count" | bc -l)
        avg_observations=$(echo "scale=2; $avg_observations / $count" | bc -l)
    fi
}

function get_engine_number()
{
    engine=$1
    if [[ "$engine" == "fuzzy" ]]; then
        return 0
    elif [[ "$engine" == "ann" ]]; then
        return 1
    elif [[ "$engine" == "naive" ]]; then
        return 2
    elif [[ "$engine" == "fuzzy_no_simplification" ]]; then
        return 3
    fi
    return 0 #default
}

function run_simulation()
{
    engine=$1
    get_engine_number $engine
    engine_number=$?
    seed=$2
    memory_size=$3
    cache_size=$4
    pseudo_rehearsal=$5
    for conf in $(ls *.conf); do
        for data in $(ls ${conf/.conf/}.*data); do
            ${SIM_EXE} $conf $data $seed 1 ${engine_number} ${memory_size} ${cache_size} ${pseudo_rehearsal} > "${results_dir}/result_${engine}_${conf}.${data}.${seed}" 2>&1 &
        done
    done
    jobs
    wait
}

function print_results()
{
    engine=$1
    if [[ "$engine" == "ann" ]]; then
        OUTPUT="$engine Results Max_It_Duration Avg_It_Duration Avg_Anns_Created Avg_Max_Observations"
    else
        OUTPUT="$engine Results Max_It_Duration Avg_It_Duration"
    fi

    #parse results
    for conf in $(ls *.conf); do
        for data in $(ls ${conf/.conf/}.*data); do
            file=$(ls ${results_dir}/result_${engine}_${conf}.${data}.* | head -n 1)
            i=0
            #for each variable
            while cat ${file} | grep "Variable${i}" > /dev/null 2> /dev/null; do
                var_name=$(cat $file | grep "Variable${i}"|cut -d ' ' -f 2)

                parse_results ${engine}
                engine_result=$result
                engine_max_it_duration=${max_it_duration}
                engine_duration=${duration}

                if [[ "$engine" == "ann" ]]; then
                    OUTPUT="$OUTPUT\n${data/.data/}.$var_name ${engine_result} ${engine_max_it_duration} ${engine_duration} ${avg_anns} ${avg_observations}"
                else
                    OUTPUT="$OUTPUT\n${data/.data/}.$var_name ${engine_result} ${engine_max_it_duration} ${engine_duration}"
                fi

                i=$(echo $i + 1 | bc)
            done
        done
    done
}

function create_dir()
{
    local prefix="./"
    if [ $# -ne 0 ]; then
        prefix="${1}/"
    fi

    results_dir="${prefix}simulation_results_"$(date +"%d_%b_%Y_%T")
    mkdir -p $results_dir
    if [ $? -ne 0 ]; then
        echo "Could not create the dir $results_dir"
        exit
    fi
    results_dir=$(realpath $results_dir)
    echo "Result dir at: ${results_dir}"
}

if [[ $# < 2 ]]; then
    echo "Correct use ./run_all_simulations.sh <simulator_exe>" \
         "<simulations_dir> [repeat] [engines] [seed]" \
         "[max_memory_for_observation] [ann_cache size] [result_dir_path]"
    exit -1
fi

SIM_EXE=$(realpath $1)
SIM_DIR=$2

if ! pushd $SIM_DIR; then
    exit
fi

if [[ $# < 3 ]]; then
    repeat=10
else
    repeat=${3}
fi

if [[ $# < 4 ]]; then
    engines="1 2 3 4"
else
    engines=${4}
fi

if [[ $# < 5 ]]; then
    start_seed=$(date +%s)
else
    start_seed=${5}
fi

if [[ $# < 6 ]]; then
    memory_size=0
else
    memory_size=${6}
fi

if [[ $# < 7 ]]; then
    cache_size=0
else
    cache_size=${7}
fi

if [[ $# < 8 ]]; then
    pseudo_rehearsal=0
else
    pseudo_rehearsal=${8}
fi

create_dir ${9}

for engine in $engines; do
    echo "Running engine $i"
    echo "Start seed: $start_seed"
    for i in $(seq ${repeat}); do
        seed=$(echo "${start_seed} + ${i}" | bc)
        run_simulation $engine $seed $memory_size $cache_size $pseudo_rehearsal
    done
done

echo "========================================================================================="
print_results $engine
echo -ne "$OUTPUT\n" | column -t > ${results_dir}/results.txt

if [ $? -ne 0 ]; then
    echo -ne "$OUTPUT\n" | column -t
    echo "Could not save the output as file!"
else
    cat ${results_dir}/results.txt
    echo "Results saved at:${results_dir}/results.txt"
fi

popd
