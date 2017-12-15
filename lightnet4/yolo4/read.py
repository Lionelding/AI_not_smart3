import numpy as np
import sys
import os
import sqlite3


data_file_name1 = 'table/objects.txt'
data_file_name2 = 'table/objects_features.txt'
data_file_name3 = 'table/Positions.txt'


database_name = 'sqliteTable/eat.sqlite'
if os.path.isfile(database_name):
	os.remove(database_name)

conn = sqlite3.connect(database_name)
c = conn.cursor()
c.execute('''CREATE TABLE objects (object_id INTEGER, road_user_type INTEGER DEFAULT 0, n_objects INTEGER DEFAULT 1, PRIMARY KEY(object_id))''')
c.execute('''CREATE TABLE objects_features (object_id INTEGER, trajectory_id INTEGER, PRIMARY KEY(object_id, trajectory_id))''')
c.execute('''CREATE TABLE positions (trajectory_id INTEGER, frame_number INTEGER, x_coordinate REAL, y_coordinate REAL, PRIMARY KEY(trajectory_id, frame_number))''')
c.execute('''CREATE TABLE velocities (trajectory_id INTEGER, frame_number INTEGER, x_coordinate REAL, y_coordinate REAL, PRIMARY KEY(trajectory_id, frame_number))''')


with open(data_file_name1) as f1:
    content1 = f1.readlines()

with open(data_file_name2) as f2:
    content2 = f2.readlines()

with open(data_file_name3) as f3:
    content3 = f3.readlines()


for i in range(0, len(content1)):
	line=content1[i][:-1].split(",")
	line=map(int, line)
	#print line
	c.execute('INSERT INTO objects VALUES (?, ?, ?)', [line[0], line[1], 1])


for j in range(0, len(content2)):
	line2=content2[j][:-1].split(",")
	#print line2
	line2=map(int, line2)
	#print line2
	c.execute('INSERT INTO objects_features VALUES (?, ?)', [line2[0], line2[1]])

for k in range(0, len(content3)):
	line3=content3[k][:-1].split(",")
	line3=map(int, line3)
	c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [line3[0], line3[1], line3[2], line3[3]])


# c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 0])
# c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id_new * 4 + 0, frame_num - offset, filtered_proj_x_tl, filtered_proj_y_tl])


conn.commit()
conn.close()

print "Finished importing txt files to sqltables"
