#/usr/bin/python
from __future__ import division
import fileinput
import sys
import math

def parse_measurement_line(line):
    (name, value) = line.split(':')
    return name, float(value)

all_values = {} 
averages = {}
m2 = {}
samples = {}
acceptancy = 0.95

for line in sys.stdin.readlines():
	(name, value) = parse_measurement_line(line)
	if name in all_values:
		all_values[name].append(value)
		samples[name] += 1
		oldavg = averages[name]
		delta = value - oldavg
		averages[name] += delta/samples[name]
		m2[name] += delta * (value - averages[name])
	else:
		all_values[name] = [value]
		averages[name] = value
		samples[name] = 1
		m2[name] = 0

for name, values_list in all_values.items():
	retained = 0
	for v in values_list:
		if abs(v - averages[name]) < 3 * math.sqrt((m2[name]/samples[name])):
			retained += 1
	if retained/samples[name] < acceptancy:
		accepted = float(retained/samples[name])
		print name, "has not reached convergency. Only ", retained, " samples were valid over ", samples[name], "[", accepted*100, "%]"
		exit(0)

output_file = open(sys.argv[1], 'w+')
for name, value in averages.items():
	st = name
	st += "\t" + str(value) + "\t" + str(math.sqrt(m2[name]/samples[name]))+"\n"
	output_file.write(st)
output_file.close()

exit(1)