# python display_trajectory.py video_name database_name homography(optioinal)
import cv2
import sys
import sqlite3
import numpy as np
import gen2_tools

video_input_name = sys.argv[1]
video_output_name = video_input_name[:-4] + '_demo.avi'

fourcc = cv2.cv.CV_FOURCC(*'XVID')
capture = cv2.VideoCapture(video_input_name)
width = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_WIDTH))
height = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT))
fps = capture.get(cv2.cv.CV_CAP_PROP_FPS)
video_out = cv2.VideoWriter(video_output_name, fourcc, fps, (width, height))

def draw_line(img, data, frame_num, obj_id, obj_center, iteration):
	iteration += 1
	if iteration == 600:
		return
	if frame_num == 0:
		return
	for box_num in data[frame_num - 1]:
		if data[frame_num - 1][box_num]['id'] == obj_id:
			obj_center_prev_frame = data[frame_num - 1][box_num]['center']
			cv2.line(img, (obj_center_prev_frame[1], obj_center_prev_frame[0]), (obj_center[1], obj_center[0]), obj_color, 3)
			draw_line(img, data, frame_num - 1, obj_id, obj_center_prev_frame, iteration)
			return
	return

def dict_factory(cursor, row):
    d = {}
    for idx, col in enumerate(cursor.description):
        d[col[0]] = row[idx]
    return d

database_name = sys.argv[2]
try:
	homography = np.loadtxt(sys.argv[3])
except:
	homography = None
print 'Loading database...'
connection = sqlite3.connect(database_name)
connection.row_factory = dict_factory
cursor = connection.cursor()
objects = cursor.execute('SELECT OF.object_id, P.frame_number, avg(P.x_coordinate), avg(P.y_coordinate) from objects P, objects_features OF WHERE P.trajectory_id = OF.trajectory_id GROUP BY OF.object_id, P.frame_number ORDER BY OF.object_id, P.frame_number').fetchall()
# objects = cursor.execute('SELECT * FROM objects').fetchall()
# objects_features = cursor.execute('SELECT * FROM objects_features').fetchall()
# positions = cursor.execute('SELECT * FROM positions').fetchall()
# velocities = cursor.execute('SELECT * FROM velocities').fetchall()
print 'Database loaded. Converting...'
classification_dict = {}
for row in objects:
	classification_dict[row['object_id']] = row['road_user_type']
objects_features_dict = {}
for row in objects_features:
	if row['object_id'] not in objects_features_dict:
		objects_features_dict[row['object_id']] = [row['trajectory_id']]
	else:
		objects_features_dict[row['object_id']].append(row['trajectory_id'])
corners_dict = {}
# for row in corners:


data = {}
box_num = 0
p_frame = -1
for row in objects:
	if row['frame'] != p_frame:
		data[row['frame']] = {}
	p_frame = row['frame']
	if homography is not None:
		cent = gen2_tools.worldToVideoProject([row['x'], row['y']], homography)
	else:
		cent = [row['x'], row['y']]
	data[row['frame']][box_num] = {
	'class': classification_dict[row['object_id']],
	# 'left': row['left'],
	# 'right': row['right'],
	# 'top': row['top'],
	# 'bottom': row['bottom'],
	'center': cent,
	'id': row['object_id']}
	box_num += 1
print 'Done!'

frame_num = 0
ret = True
while ret:
	ret, img = capture.read()
	if not ret:
		break
	if not frame_num % 100:
		print frame_num
	for box_num in data[frame_num]:
		# obj_left = data[frame_num][box_num]['left']
		# obj_right = data[frame_num][box_num]['right']
		# obj_top = data[frame_num][box_num]['top']
		# obj_bottom = data[frame_num][box_num]['bottom']
		obj_class = data[frame_num][box_num]['class']
		obj_center = data[frame_num][box_num]['center']
		obj_id = data[frame_num][box_num]['id']
		if obj_class == 'car':
			obj_color = (0, 0, 255)
		elif obj_class == 'motorbike' or obj_class == 'motorcycle':
			obj_color = (0, 255, 255)
		elif obj_class == 'truck':
			obj_color = (255, 0, 255)
		elif obj_class == 'bus':
			obj_color = (255, 255, 0)
		elif obj_class == 'person' or obj_class == 'pedestrian':
			obj_color = (255, 0, 0)
		elif obj_class == 'bicycle':
			obj_color = (0, 255, 0)
		else:
			obj_color = (0, 0, 0)
 		draw_line(img, data, frame_num, obj_id, obj_center, 0)
		# cv2.rectangle(img, (obj_left, obj_top), (obj_right, obj_bottom), obj_color, 2)
		cv2.putText(img, str(obj_id), (obj_center[1], obj_center[0]), cv2.FONT_HERSHEY_SIMPLEX, 1, obj_color, 2)
		# speed = 0
		# if frame_num > 0:
		# 	for i in data[frame_num - 1]:
		# 		if data[frame_num - 1][i]['id'] == obj_id:
		# 			dist_x = data[frame_num - 1][i]['center'][0] - obj_center[0]
		# 			dist_y = data[frame_num - 1][i]['center'][1] - obj_center[1]
		# 			speed = (dist_x ** 2 + dist_y ** 2) ** .5 * fps * 3.6
		# 			break
		# cv2.putText(img, str(int(speed)), (obj_center[1], obj_center[0]), cv2.FONT_HERSHEY_SIMPLEX, 1, obj_color, 2)
	video_out.write(img)
	frame_num += 1
