#!/bin/sh
#-------------------------------------------------------------------------------
# Copyright (C) 2018 Tiago R. Muck <tmuck@uci.edu>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#-------------------------------------------------------------------------------

# Runs a couple of benchmarks using the interfacetest daemon
# The same test is executed multiple times and the resulting
# time traces aggregated in a single file.
#

source $SPARTA_SCRIPTDIR/runtime/common.sh

function do_test() {
    sudosh $SPARTA_SCRIPTDIR/runtime/start.sh interfacetest
    taskset 0x10 sh $SPARTA_SCRIPTDIR/ubenchmarks/high_ipc_high_load.sh > /dev/null &
    taskset 0x40 sh $SPARTA_SCRIPTDIR/ubenchmarks/high_ipc_high_load.sh > /dev/null &
    taskset 0x02 sh $SPARTA_SCRIPTDIR/ubenchmarks/high_ipc_low_load.sh > /dev/null &
    taskset 0x04 sh $SPARTA_SCRIPTDIR/ubenchmarks/high_ipc_low_load.sh > /dev/null &
    wait
    sudosh $SPARTA_SCRIPTDIR/runtime/stop.sh

    cp $RTS_DAEMON_OUTDIR/total.csv interface_test-total$1.csv
    cp $RTS_DAEMON_OUTDIR/execTraceFine.csv interface_test-fine_trace$1.csv
    cp $RTS_DAEMON_OUTDIR/execTraceCoarse.csv interface_test-coarse_trace$1.csv
}

rm -f interface_test*.csv

for iter in $(seq 0 15)
do
    do_test $iter
done

#aggregates the files from multiples runs
FILES=$(ls interface_test-fine_trace*.csv | xargs)
python3 $SPARTA_SCRIPTDIR/tracing/trace_agg-periodic.py --srcfiles $FILES --destfile interface_test-fine_trace.agg.csv
FILES=$(ls interface_test-coarse_trace*.csv | xargs)
python3 $SPARTA_SCRIPTDIR/tracing/trace_agg-periodic.py --srcfiles $FILES --destfile interface_test-coarse_trace.agg.csv


