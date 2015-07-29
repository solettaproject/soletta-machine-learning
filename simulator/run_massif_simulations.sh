#!/bin/bash

# This file is part of the Soletta Project
#
# Copyright (C) 2015 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
          "<simulations_dir> <seed>"
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
