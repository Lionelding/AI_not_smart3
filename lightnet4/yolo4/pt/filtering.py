#  filtering.py database_file_name fps
import numpy as np
import sys
import os
import gen2_tools
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TI'))
import storage

database_file_name = sys.argv[1] # sqlite file
fps = float(sys.argv[2])
print 'loading database...'
objects = storage.loadTrajectoriesFromSqlite(database_file_name, 'object')
print 'filtering...'

for obj in objects:
	obj_speed = obj.getSpeeds()
	obj_speed.sort()
	obj_max_speed = obj_speed[int(.95 * len(obj))] * fps * 3.6 # in kmph
	obj_positions = obj.getPositions()
	first_last_position_distance = ((obj_positions[0].x - obj_positions[-1].x) ** 2 + (obj_positions[0].y - obj_positions[-1].y) ** 2) ** .5
	if first_last_position_distance < 2: # remove any object that moved less than 2m
		obj.userType = gen2_tools.userType2Num('unknown')
	elif obj.userType == gen2_tools.userType2Num('car'):
		if obj_max_speed < 5:
			obj.userType = gen2_tools.userType2Num('unknown')
	elif obj_max_speed < 2:
	 	obj.userType = gen2_tools.userType2Num('unknown')

storage.setRoadUserTypes(database_file_name, objects)
