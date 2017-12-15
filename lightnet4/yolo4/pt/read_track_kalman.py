#  read_track.py file_name_from_yolo fps homography(optioinal)
import numpy as np
import sys
import os
import sqlite3
import operator
import gen2_tools
sys.path.append('/home/brisk/gen2/TI/')
import cvutils

def find_common_surface_ratio(frame_num, iteration, data_dict, obj_left, obj_right, obj_top, obj_bottom, box_id):
	prev_obj_right = data_dict[frame_num - iteration][box_id]['right']
	prev_obj_left = data_dict[frame_num - iteration][box_id]['left']
	prev_obj_bottom = data_dict[frame_num - iteration][box_id]['bottom']
	prev_obj_top = data_dict[frame_num - iteration][box_id]['top']
	common_surface = max(0, (min(obj_right, prev_obj_right) - max(obj_left, prev_obj_left))) * max(0, (min(obj_bottom, prev_obj_bottom) - max(obj_top, prev_obj_top)))
	surface = (obj_right - obj_left) * (obj_bottom - obj_top)
	prev_surface = (prev_obj_right - prev_obj_left) * (prev_obj_bottom - prev_obj_top)
	common_surface_ratio = min(float(common_surface) / float(surface), float(common_surface) / float(prev_surface))
	return common_surface_ratio

def find_match_score(frame_num, iteration, data_dict, obj_class, obj_left, obj_right, obj_top, obj_bottom, match_candidates, box_id, match_score):
	if frame_num - iteration in match_candidates:
		for box_id_prev in match_candidates[frame_num - iteration]:
			if data_dict[frame_num - iteration][box_id_prev]['class'] == obj_class:
				common_surface_ratio = find_common_surface_ratio(frame_num, iteration, data_dict, obj_left, obj_right, obj_top, obj_bottom, box_id_prev)
				if common_surface_ratio > .2:
					match_score[(box_id_prev, box_id)] = common_surface_ratio
	return match_score

def find_best_match(match_score, frame_data_dict, data_dict, matched_box, match_candidates, iteration, box_id):
	# find the best candidate for matching
	while True: # continue until all objects are matched
		if len(match_score) == 0: # all the objects are matched
			break
		(bip, bi) = max(match_score.iteritems(), key = operator.itemgetter(1))[0] # find the maximum score between two frames' objects
		frame_data_dict[bi]['id'] = data_dict[frame_num - iteration][bip]['id'] # matching two objects
		frame_data_dict[bi]['number_of_matched_boxes'] = data_dict[frame_num - iteration][bip]['number_of_matched_boxes'] + 1
		frame_data_dict[bi]['matched_to_box_id'] = bip
		if iteration > 1:
			data_dict, box_id = interapolate_box(bip, bi, frame_num, iteration, data_dict, frame_data_dict, box_id)
			frame_data_dict[bi]['matched_to_box_id'] = box_id - 1
			frame_data_dict[bi]['number_of_matched_boxes'] = data_dict[frame_num - iteration][bip]['number_of_matched_boxes'] + iteration
		matched_box.append(bi)
		match_candidates[frame_num - iteration].remove(bip) # it's matched
		for (i, j) in list(match_score):
			if i == bip or j == bi:
				del match_score[(i, j)]
	return match_score, frame_data_dict, matched_box, match_candidates, data_dict, box_id

def interapolate_box(bip, bi, frame_num, iteration, data_dict, frame_data_dict, box_id):
	k = 1
	box1 = data_dict[frame_num - iteration][bip]
	box2 = frame_data_dict[bi]
	while k < iteration: # intrapolate between frames
		coef = float(k) / float(iteration)
		left_temp = (1 - coef) * box1['left'] + coef * box2['left']
		right_temp = (1 - coef) * box1['right'] + coef * box2['right']
		top_temp = (1 - coef) * box1['top'] + coef * box2['top']
		bottom_temp = (1 - coef) * box1['bottom'] + coef * box2['bottom']
		center_temp = [(left_temp + right_temp) / 2, (top_temp + bottom_temp) / 2]
		data_dict[frame_num - iteration + k][box_id] = {
		'class': box1['class'],
		'probability': (box1['probability'] + box2['probability']) / 2,
		'left': left_temp,
		'right': right_temp,
		'top': top_temp,
		'bottom': bottom_temp,
		'center': center_temp,
		'id': box1['id'],
		'number_of_matched_boxes': box1['number_of_matched_boxes'] + k,
		'matched_to_box_id': box_id - 1,
		'state_tl': -1,
		'state_tr': -1,
		'state_bl': -1,
		'state_br': -1,
		'P_tl': -1,
		'P_tr': -1,
		'P_bl': -1,
		'P_br': -1}
		if k == 1:
			data_dict[frame_num - iteration + k][box_id]['matched_to_box_id'] = bip
		put_in_table(data_dict, data_dict[frame_num - iteration + k], frame_num - iteration + k, box_id, obj_class_dict, homography)
		box_id += 1
		k += 1
	return data_dict, box_id

def position_list(data_dict, first_obj_center, first_matched_to_box_id, frame_num, box_id, obj_center_list_x, obj_center_list_y, homography, iteration):
	if first_obj_center != []:
		obj_center = first_obj_center
		matched_to_box_id = first_matched_to_box_id
	else:
		obj_center = data_dict[frame_num][box_id]['center']
		matched_to_box_id = data_dict[frame_num][box_id]['matched_to_box_id']
	if homography is not None:
		obj_center = cvutils.projectArray(homography, np.array(obj_center).reshape(2, 1))
		obj_center_list_x.append(obj_center[0][0])
		obj_center_list_y.append(obj_center[1][0])
	if matched_to_box_id == -1 or iteration == 0:
		return obj_center_list_x, obj_center_list_y, iteration
	iteration -= 1
	obj_center_list_x, obj_center_list_y, iteration = position_list(data_dict, [], [], frame_num - 1, matched_to_box_id, obj_center_list_x, obj_center_list_y, homography, iteration)
	return obj_center_list_x, obj_center_list_y, iteration

def filter_list_mean(value_list):
	value_list.sort()
	len_value_list = len(value_list)
	filtered_value_list = value_list[int(np.floor(len_value_list * .25)):int(np.ceil(len_value_list * .75))]
	return np.mean(filtered_value_list)

def kalman_xy(x, P, measurement, R,					# Kalman filter from: https://stackoverflow.com/questions/13901997/kalman-2d-filter-in-python
			  motion = np.matrix([0., 0., 0., 0.]).T,
			  Q = np.matrix(np.eye(4))):
	"""
	Parameters:
	x: initial state 4-tuple of location and velocity: (x0, x1, x0_dot, x1_dot)
	P: initial uncertainty convariance matrix
	measurement: observed position
	R: measurement noise
	motion: external motion added to state vector x
	Q: motion noise (same shape as P)
	"""
	return kalman(x, P, measurement, R, motion, Q,
				  F = np.matrix([
					  [1., 0., 1., 0.],
					  [0., 1., 0., 1.],
					  [0., 0., 1., 0.],
					  [0., 0., 0., 1.]
					  ]),
				  H = np.matrix([
					  [1., 0., 0., 0.],
					  [0., 1., 0., 0.]
					  ]))

def kalman(x, P, measurement, R, motion, Q, F, H):
	'''
	Parameters:
	x: initial state
	P: initial uncertainty convariance matrix
	measurement: observed position (same shape as H*x)
	R: measurement noise (same shape as H)
	motion: external motion added to state vector x
	Q: motion noise (same shape as P)
	F: next state function: x_prime = F*x
	H: measurement function: position = H*x
	Return: the updated and predicted new values for (x, P)
	See also http://en.wikipedia.org/wiki/Kalman_filter
	This version of kalman can be applied to many different situations by
	appropriately defining F and H
	'''
	# UPDATE x, P based on measurement m
	# distance between measured and current position-belief
	y = np.matrix(measurement).T - H * x
	S = H * P * H.T + R  # residual convariance
	K = P * H.T * S.I	# Kalman gain
	x = x + K * y
	I = np.matrix(np.eye(F.shape[0])) # identity matrix
	P = (I - K * H) * P
	# PREDICT x, P based on motion
	x = F * x + motion
	P = F * P * F.T + Q
	return x, P

def kalman_estimate(box_data, data_dict, frame_num, matched_to_box_id, x_tl, y_tl, x_tr, y_tr, x_bl, y_bl, x_br, y_br, R = 100):
	if matched_to_box_id == -1: # first frame the object is observed
		box_data['state_tl'] = np.matrix([x_tl, y_tl, 0., 0.]).T
		box_data['state_tr'] = np.matrix([x_tr, y_tr, 0., 0.]).T
		box_data['state_bl'] = np.matrix([x_bl, y_bl, 0., 0.]).T
		box_data['state_br'] = np.matrix([x_br, y_br, 0., 0.]).T
		box_data['P_tl'] = np.matrix(np.eye(4)) * 1 # initial uncertainty
		box_data['P_tr'] = np.matrix(np.eye(4)) * 1 # initial uncertainty
		box_data['P_bl'] = np.matrix(np.eye(4)) * 1 # initial uncertainty
		box_data['P_br'] = np.matrix(np.eye(4)) * 1 # initial uncertainty
	else:
		box_data['state_tl'], box_data['P_tl'] = kalman_xy(data_dict[frame_num - 1][matched_to_box_id]['state_tl'], data_dict[frame_num - 1][matched_to_box_id]['P_tl'], (x_tl, y_tl), R)
		box_data['state_tr'], box_data['P_tr'] = kalman_xy(data_dict[frame_num - 1][matched_to_box_id]['state_tr'], data_dict[frame_num - 1][matched_to_box_id]['P_tr'], (x_tr, y_tr), R)
		box_data['state_bl'], box_data['P_bl'] = kalman_xy(data_dict[frame_num - 1][matched_to_box_id]['state_bl'], data_dict[frame_num - 1][matched_to_box_id]['P_bl'], (x_bl, y_bl), R)
		box_data['state_br'], box_data['P_br'] = kalman_xy(data_dict[frame_num - 1][matched_to_box_id]['state_br'], data_dict[frame_num - 1][matched_to_box_id]['P_br'], (x_br, y_br), R)
	return box_data

def put_in_table(data_dict, frame_data_dict, frame_num, box_id, obj_class_dict, homography):
 	obj_id = frame_data_dict[box_id]['id']
	obj_class = frame_data_dict[box_id]['class']
	obj_left = frame_data_dict[box_id]['left']
	obj_right = frame_data_dict[box_id]['right']
	obj_top = frame_data_dict[box_id]['top']
	obj_bottom = frame_data_dict[box_id]['bottom']
	obj_center = frame_data_dict[box_id]['center']
	matched_to_box_id = frame_data_dict[box_id]['matched_to_box_id']
	number_of_matched_boxes = frame_data_dict[box_id]['number_of_matched_boxes']
	if obj_id not in obj_class_dict:
		obj_class_dict[obj_id] = gen2_tools.userType2Num('unknown')
		c.execute('INSERT INTO objects VALUES (?, ?, ?)', [obj_id, gen2_tools.userType2Num('unknown'), 1])
	# TODO: a better way for (fps / 2)
	if number_of_matched_boxes >= fps / 2 and obj_class_dict[obj_id] != obj_class: # each object has to be in the video for at least 0.5s (fps/2), otherwise it will not be classified and used
		obj_class_dict[obj_id] = obj_class
		c.execute('UPDATE objects SET road_user_type = ? WHERE object_id = ?', [gen2_tools.userType2Num(obj_class), obj_id])
	c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id * 4 + 0])
	c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id * 4 + 1])
	c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id * 4 + 2])
	c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id * 4 + 3])
	if homography is not None:
		proj_tl = cvutils.projectArray(homography, np.array([obj_left, obj_top]).reshape(2, 1))
		proj_x_tl = proj_tl[0][0]
		proj_y_tl = proj_tl[1][0]
		proj_tr = cvutils.projectArray(homography, np.array([obj_right, obj_top]).reshape(2, 1))
		proj_x_tr = proj_tr[0][0]
		proj_y_tr = proj_tr[1][0]
		proj_bl = cvutils.projectArray(homography, np.array([obj_left, obj_bottom]).reshape(2, 1))
		proj_x_bl = proj_bl[0][0]
		proj_y_bl = proj_bl[1][0]
		proj_br = cvutils.projectArray(homography, np.array([obj_right, obj_bottom]).reshape(2, 1))
		proj_x_br = proj_br[0][0]
		proj_y_br = proj_br[1][0]
	else:
		proj_x_tl = obj_left
		proj_y_tl = obj_top
		proj_x_tr = obj_right
		proj_y_tr = obj_top
		proj_x_bl = obj_left
		proj_y_bl = obj_bottom
		proj_x_br = obj_right
		proj_y_br = obj_bottom
	frame_data_dict[box_id] = kalman_estimate(frame_data_dict[box_id], data_dict, frame_num, matched_to_box_id, proj_x_tl, proj_y_tl, proj_x_tr, proj_y_tr, proj_x_bl, proj_y_bl, proj_x_br, proj_y_br)
	kalman_x_tl, kalman_y_tl, kalman_vx_tl, kalman_vy_tl = frame_data_dict[box_id]['state_tl'].tolist()
	kalman_x_tr, kalman_y_tr, kalman_vx_tr, kalman_vy_tr = frame_data_dict[box_id]['state_tr'].tolist()
	kalman_x_bl, kalman_y_bl, kalman_vx_bl, kalman_vy_bl = frame_data_dict[box_id]['state_bl'].tolist()
	kalman_x_br, kalman_y_br, kalman_vx_br, kalman_vy_br = frame_data_dict[box_id]['state_br'].tolist()
	c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id * 4 + 0, frame_num, kalman_x_tl[0], kalman_y_tl[0]])
	c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id * 4 + 1, frame_num, kalman_x_tr[0], kalman_y_tr[0]])
	c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id * 4 + 2, frame_num, kalman_x_bl[0], kalman_y_bl[0]])
	c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)', [box_id * 4 + 3, frame_num, kalman_x_br[0], kalman_y_br[0]])
	c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)', [box_id * 4 + 0, frame_num, kalman_vx_tl[0], kalman_vy_tl[0]])
	c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)', [box_id * 4 + 1, frame_num, kalman_vx_tr[0], kalman_vy_tr[0]])
	c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)', [box_id * 4 + 2, frame_num, kalman_vx_bl[0], kalman_vy_bl[0]])
	c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)', [box_id * 4 + 3, frame_num, kalman_vx_br[0], kalman_vy_br[0]])
	if number_of_matched_boxes == 2: # update the velocity for the first frame
		c.execute('UPDATE velocities SET x_coordinate = ? WHERE trajectory_id = ?', [kalman_vx_tl[0], matched_to_box_id * 4 + 0])
		c.execute('UPDATE velocities SET y_coordinate = ? WHERE trajectory_id = ?', [kalman_vy_tl[0], matched_to_box_id * 4 + 0])
		c.execute('UPDATE velocities SET x_coordinate = ? WHERE trajectory_id = ?', [kalman_vx_tr[0], matched_to_box_id * 4 + 1])
		c.execute('UPDATE velocities SET y_coordinate = ? WHERE trajectory_id = ?', [kalman_vy_tr[0], matched_to_box_id * 4 + 1])
		c.execute('UPDATE velocities SET x_coordinate = ? WHERE trajectory_id = ?', [kalman_vx_bl[0], matched_to_box_id * 4 + 2])
		c.execute('UPDATE velocities SET y_coordinate = ? WHERE trajectory_id = ?', [kalman_vy_bl[0], matched_to_box_id * 4 + 2])
		c.execute('UPDATE velocities SET x_coordinate = ? WHERE trajectory_id = ?', [kalman_vx_br[0], matched_to_box_id * 4 + 3])
		c.execute('UPDATE velocities SET y_coordinate = ? WHERE trajectory_id = ?', [kalman_vy_br[0], matched_to_box_id * 4 + 3])
	return


data_file_name = sys.argv[1] # text file from YOLO
fps = float(sys.argv[2])
database_name = data_file_name[:-3] + 'sqlite'
if os.path.isfile(database_name):
	os.remove(database_name)
conn = sqlite3.connect(database_name)
c = conn.cursor()
c.execute('''CREATE TABLE objects (object_id INTEGER, road_user_type INTEGER DEFAULT 0, n_objects INTEGER DEFAULT 1, PRIMARY KEY(object_id))''')
c.execute('''CREATE TABLE objects_features (object_id INTEGER, trajectory_id INTEGER, PRIMARY KEY(object_id, trajectory_id))''')
c.execute('''CREATE TABLE positions (trajectory_id INTEGER, frame_number INTEGER, x_coordinate REAL, y_coordinate REAL, PRIMARY KEY(trajectory_id, frame_number))''')
c.execute('''CREATE TABLE velocities (trajectory_id INTEGER, frame_number INTEGER, x_coordinate REAL, y_coordinate REAL, PRIMARY KEY(trajectory_id, frame_number))''')
try:
	homography = np.loadtxt(sys.argv[3])
except:
	homography = None
	print 'Warning: No homography found!'
frame_num = -1
obj_id = 0
box_id = 0
next_obj_id = 0
data_dict = {}
obj_class_dict = {}
match_candidates = {}
obj_center_list_x = {}
obj_center_list_y = {}
obj_center_list_x[0] = {}
obj_center_list_y[0] = {}
lines = open(data_file_name).read().splitlines()
for line in lines:
	currentline = line.split(', ')
	if currentline[0] == 'next_frame':
		matched_box = []
		if frame_num > 0:
			iteration = 1
			while iteration <= fps and frame_num >= iteration:
				match_score, frame_data_dict, matched_box, match_candidates, data_dict, box_id = find_best_match(match_score, frame_data_dict, data_dict, matched_box, match_candidates, iteration, box_id)
				if len(matched_box) == len(frame_data_dict):
					break
				iteration += 1
				for bi in frame_data_dict:
					if bi not in matched_box: # could not find a match in previous frame
						match_score = find_match_score(frame_num, iteration, data_dict, frame_data_dict[bi]['class'], frame_data_dict[bi]['left'], frame_data_dict[bi]['right'], frame_data_dict[bi]['top'], frame_data_dict[bi]['bottom'], match_candidates, bi, match_score)
		if frame_num > -1:
			for bi in frame_data_dict:
				if bi not in matched_box: # new object
					frame_data_dict[bi]['id'] = obj_id
					obj_id += 1
					obj_center_list_x[obj_id] = {}
					obj_center_list_y[obj_id] = {}
				put_in_table(data_dict, frame_data_dict, frame_num, bi, obj_class_dict, homography)
			data_dict[frame_num] = frame_data_dict
		frame_num += 1
		if not frame_num % 100:
			print 'frame', frame_num
		# if frame_num == 1000:
		# 	break
		frame_data_dict = {}
		match_score = {}
		match_candidates[frame_num] = []
	else:
		########################## reading the text file
		obj_class = currentline[0]
		obj_probability = int(currentline[1])
		obj_left = int(currentline[2])
		obj_right = int(currentline[3])
		obj_top = int(currentline[4])
		obj_bottom = int(currentline[5])
		obj_center = [(obj_left + obj_right) / 2, (obj_top + obj_bottom) / 2]
		if obj_class == 'bus' or obj_class == 'work_van' or obj_class == 'truck' or obj_class == 'motorized_vehicle' or obj_class == 'pickup_truck' or obj_class == 'single_unit_truck' or obj_class == 'articulated_truck':
			obj_class = 'car'
		########################## measure the match score with all the objects in previous frame(s)
		match_score = find_match_score(frame_num, 1, data_dict, obj_class, obj_left, obj_right, obj_top, obj_bottom, match_candidates, box_id, match_score)
		match_candidates[frame_num].append(box_id)
		frame_data_dict[box_id] = {
		'class': obj_class,
		'probability': obj_probability,
		'left': obj_left,
		'right': obj_right,
		'top': obj_top,
		'bottom': obj_bottom,
		'center': obj_center,
		'id': -1,
		'number_of_matched_boxes': 1,
		'matched_to_box_id': -1,
		'state_tl': -1,
		'state_tr': -1,
		'state_bl': -1,
		'state_br': -1,
		'P_tl': -1,
		'P_tr': -1,
		'P_bl': -1,
		'P_br': -1}
		box_id += 1

# for i in data_dict:
# 	for j in data_dict[i]:
# 		if data_dict[i][j]['id']==5:
# 			print data_dict[i][j]

conn.commit()
conn.close()
