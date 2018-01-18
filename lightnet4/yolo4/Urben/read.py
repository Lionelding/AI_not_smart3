import numpy as np
import sys
import os
import sqlite3


data_file_name1 = 'bounding_boxes.txt'
data_file_name2 = 'objects.txt'
data_file_name3 = 'objects_type.txt'
myset1 = set()
myset2 = set()

database_name = 'eat.sqlite'
if os.path.isfile(database_name):
	os.remove(database_name)

conn = sqlite3.connect(database_name)
c = conn.cursor()
c.execute('''CREATE TABLE bounding_boxes ( object_id INTEGER, frame_number INTEGER, x_top_left REAL, y_top_left REAL, x_bottom_right REAL, y_bottom_right REAL, PRIMARY KEY(object_id, frame_number) )''')
c.execute('''CREATE TABLE objects ( object_id INTEGER, road_user_type INTEGER, description TEXT, PRIMARY KEY(object_id) )''')
c.execute('''CREATE TABLE objects_type ( road_user_type INTEGER, type_string TEXT, PRIMARY KEY(road_user_type) )''')


with open(data_file_name1) as f1:
    content1 = f1.readlines()

with open(data_file_name2) as f2:
    content2 = f2.readlines()

with open(data_file_name3) as f3:
    content3 = f3.readlines()


for i in range(0, len(content1)):
	line=content1[i][:-1].split(",")
	line=map(int, line)
	print line
	c.execute('INSERT INTO bounding_boxes VALUES (?, ?, ?, ?, ?, ?)', [line[0], line[1], line[2], line[3], line[4], line[5]])


	myset1.add(line[0])

print myset1

for j in range(0, len(content2)):
	line2=content2[j][:-1].split(",")
	#print line2
	line2_1=line2[0]
	line2_2=line2[1]
	line2_3=line2[2]
	list2_1=int(line2_1)
	#print line2_1

	line2_2=int(line2_2)
	#print line2_3

	c.execute('INSERT INTO objects VALUES (?, ?, ?)', [line2_1, line2_2, line2_3])


	myset2.add(int(line2_1))

print myset2

print myset2-myset1
print myset1-myset2



for k in range(0, len(content3)):
	line3=content3[k][:-1].split(",")
	#print line3
	line3_int=line3[0]
	line3_txt=line3[1]
	line3=map(int, line3_int)
	#print line3_txt
	c.execute('INSERT INTO objects_type VALUES (?, ?)', [line3_int, line3_txt])


# c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 0])
# c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id_new * 4 + 0, frame_num - offset, filtered_proj_x_tl, filtered_proj_y_tl])


conn.commit()
conn.close()

print "Finished importing txt files to sqltables"



