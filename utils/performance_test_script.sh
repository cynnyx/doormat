#!/bin/bash

for i in {1..30}; do
	(./bin/doormatperf --gtest_output=xml:performances.xml | grep "\[PERF\]" >> performance_output.txt) || exit 1
	echo "test $i"...
done

while cat performance_output.txt | python ../utils/measurement_convergence.py measurements.txt;
do
	echo "not reached convergency... executing all tests"
	./bin/doormatperf --gtest_output=xml:performances.xml | grep "\[PERF\]" >> performance_output.txt
done

echo "ok, collected enough information to go on"

python ../utils/acceptancy.py /opt/doormat/performances/reference.txt measurements.txt || exit 1

rm performance_output.txt
mv measurements.txt "/opt/doormat/performances/accepted_$1"
