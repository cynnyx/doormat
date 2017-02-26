#/usr/bin/python
from __future__ import division
import fileinput
import sys
import math


reference_fn = sys.argv[1]
checked_fn = sys.argv[2]
references =  {}
checked = {}
reference_file = open(reference_fn, 'r+')
#load everything!
for line in reference_file:
	(name, avg, stddev) = line.split('\t')
	references[name] = {'avg': float(avg), 'stddev' : float(stddev)}
reference_file.close()

checked_file = open(checked_fn, 'r')
for line in checked_file:
	(name, avg, stddev) = line.split("\t")
	checked[name] = {'avg': float(avg), 'stddev' : float(stddev)}

print checked
checked_file.close()

iter_checked = checked
print iter_checked
for name, values in iter_checked.items():
	if name not in references or values['avg'] < references[name]['avg']:
		references[name] = {'avg': values['avg'], 'stddev' : values['stddev']}
	elif values['avg'] - references[name]['avg'] > 3 * references[name]['stddev']:
		print "Metric ", name, " has worsen from ", references[name]['avg'], " to ", values['avg'], " when stddev was ", references[name]['stddev']
		print "Performance test failed."
		exit(1)
	else:
                print "Metric ", name, " is ok; value is ", values['avg'], " agains reference ", references[name]['avg'], " with sigma ", references[name]['stddev'] 


if len(references) == 0:
	exit(1) 
reference_file = open(reference_fn, 'w')
for name, values in references.items():
	output = name + "\t" + str(values['avg']) + "\t" + str(values['stddev']) + "\n"
	reference_file.write(output)

reference_file.close()
exit(0)
