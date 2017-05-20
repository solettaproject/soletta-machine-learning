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
function run_massif()
{
    prefix=$1
    alg=$2
    memory=$3
    cache=$4
    echo "=== ${prefix} ==="
    OUTPUT=""
    for conf in $(ls *.conf); do
        for data in $(ls ${conf/.conf/}.*data); do
            OUTPUT="$OUTPUT\n${data/.data/} "
            massif="massif_${prefix}_${conf}.${data}.${seed}"
            valgrind --tool=massif --massif-out-file=${massif} ${SIM_EXE} $conf $data $seed 1 $alg $memory $cache > "result_massif_${prefix}_${conf}.${data}.${seed}" 2>&1
            OUTPUT="$OUTPUT $(cat ${massif} | grep --color=never mem_heap_B | sed -e 's/mem_heap_B=//' | sort -g | tail -n 1)"
        done
    done
    OUTPUT="$OUTPUT"
    echo -e $OUTPUT |column -t
    echo
}

if [[ $# < 2 ]]; then
    echo "Correct use ./run_massif_simulations.sh <simulator_exe>" \
          "<simulations_dir> [seed]"
    exit -1
fi

SIM_EXE=$(realpath $1)
SIM_DIR=$2

if ! pushd $SIM_DIR; then
    exit
fi

if [[ $# < 3 ]]; then
    seed=$(date +%s)
else
    seed=${3}
fi

echo "Seed: $seed"
OUTPUT=""
run_massif fuzzy 0 0 0
run_massif ann 1 0 0
run_massif naive 2 0 0
run_massif fuzzy_no_simplification 3 0 0

run_massif ann_1k 1 1024 10
run_massif ann_10k 1 10240  10
run_massif ann_100k 1 102400  10

run_massif fuzzy_1k 0 1024 0
run_massif fuzzy_10k 0 10240 0
run_massif fuzzy_100k 0 102400 0

popd
