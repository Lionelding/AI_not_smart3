#  read_track.py file_name_from_yolo fps homography(optioinal)
import numpy as np
import sys
import os
import sqlite3
import operator
import gen2_tools
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TI'))
import cvutils

def find_common_surface_ratio(frame_num, iteration, data_dict, obj_left, obj_right, obj_top, obj_bottom, box_id):
	prev_obj_right = data_dict[frame_num - iteration][box_id]['right']
	prev_obj_left = data_dict[frame_num - iteration][box_id]['left']
	prev_obj_bottom = data_dict[frame_num - iteration][box_id]['bottom']
	prev_obj_top = data_dict[frame_num - iteration][box_id]['top']
	common_surface = max(0, (min(obj_right, prev_obj_right) - max(obj_left, prev_obj_left))) * max(0, (min(obj_bottom, prev_obj_bottom) - max(obj_top, prev_obj_top)))
	surface = (obj_right - obj_left) * (obj_bottom - obj_top)
	prev_surface = (prev_obj_right - prev_obj_left) * (prev_obj_bottom - prev_obj_top)
	if surface == 0 or prev_surface == 0:
		common_surface_ratio = 0
	else:
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

def find_best_match(match_score, frame_data_dict, data_dict, matched_box, match_candidates, iteration, box_id, box_id_new):
	# find the best candidate for matching
	while True: # continue until all objects are matched
		if len(match_score) == 0: # all the objects are matched
			break
		(bip, bi) = max(match_score.iteritems(), key = operator.itemgetter(1))[0] # find the maximum score between two frames' objects
		frame_data_dict[bi]['id'] = data_dict[frame_num - iteration][bip]['id'] # matching two objects
		frame_data_dict[bi]['number_of_matched_boxes'] = data_dict[frame_num - iteration][bip]['number_of_matched_boxes'] + 1
		frame_data_dict[bi]['matched_to_box_id'] = bip
		if iteration > 1:
			data_dict, box_id, box_id_new = interapolate_box(bip, bi, frame_num, iteration, data_dict, frame_data_dict, box_id, box_id_new)
			frame_data_dict[bi]['matched_to_box_id'] = box_id - 1
			frame_data_dict[bi]['number_of_matched_boxes'] = data_dict[frame_num - iteration][bip]['number_of_matched_boxes'] + iteration
		matched_box.append(bi)
		match_candidates[frame_num - iteration].remove(bip) # it's matched
		for (i, j) in list(match_score):
			if i == bip or j == bi:
				del match_score[(i, j)]
	return match_score, frame_data_dict, matched_box, match_candidates, data_dict, box_id, box_id_new

def interapolate_box(bip, bi, frame_num, iteration, data_dict, frame_data_dict, box_id, box_id_new):
	k = 1
	box1 = data_dict[frame_num - iteration][bip]
	box2 = frame_data_dict[bi]
	while k < iteration: # intrapolate between frames
		coef = float(k) / float(iteration)
		left_temp = (1 - coef) * box1['left'] + coef * box2['left']
		right_temp = (1 - coef) * box1['right'] + coef * box2['right']
		top_temp = (1 - coef) * box1['top'] + coef * box2['top']
		bottom_temp = (1 - coef) * box1['bottom'] + coef * box2['bottom']
		data_dict[frame_num - iteration + k][box_id] = {
		'class': box1['class'],
		'probability': (box1['probability'] + box2['probability']) / 2,
		'left': left_temp,
		'right': right_temp,
		'top': top_temp,
		'bottom': bottom_temp,
		'id': box1['id'],
		'number_of_matched_boxes': box1['number_of_matched_boxes'] + k,
		'matched_to_box_id': box_id - 1,
		'tl': 0,
		'tr': 0,
		'bl': 0,
		'br': 0,
		'vtl': 0,
		'vtr': 0,
		'vbl': 0,
		'vbr': 0,
		'filtered_tl': 0,
		'filtered_tr': 0,
		'filtered_bl': 0,
		'filtered_br': 0}
		if k == 1:
			data_dict[frame_num - iteration + k][box_id]['matched_to_box_id'] = bip
		box_id_new = put_in_table(data_dict, data_dict[frame_num - iteration + k], frame_num - iteration + k, box_id, obj_class_dict, homography, box_id_new)
		box_id += 1
		k += 1
	return data_dict, box_id, box_id_new

def position_list(data_dict, first_obj_pos, first_matched_to_box_id, frame_num, box_id, corner_list_x, corner_list_y, corner_list_vx, corner_list_vy, iteration, corner):
	if first_obj_pos != []:
		pos = first_obj_pos
		matched_to_box_id = first_matched_to_box_id
	else:
		pos = data_dict[frame_num][box_id][corner]
		vel = data_dict[frame_num][box_id]['v' + corner]
		matched_to_box_id = data_dict[frame_num][box_id]['matched_to_box_id']
		corner_list_vx.append(vel[0])
		corner_list_vy.append(vel[1])
	corner_list_x.append(pos[0])
	corner_list_y.append(pos[1])
	if matched_to_box_id == -1 or iteration == 0:
		return corner_list_x, corner_list_y, corner_list_vx, corner_list_vy, iteration
	iteration -= 1
	corner_list_x, corner_list_y, corner_list_vx, corner_list_vy, iteration = position_list(data_dict, [], [], frame_num - 1, matched_to_box_id, corner_list_x, corner_list_y, corner_list_vx, corner_list_vy, iteration, corner)
	return corner_list_x, corner_list_y, corner_list_vx, corner_list_vy, iteration

def filter_list_mean(value_list):
	value_list.sort()
	len_value_list = len(value_list)
	filtered_value_list = value_list[int(np.floor(len_value_list * .25)):int(np.ceil(len_value_list * .75))]
	return np.mean(filtered_value_list)

def put_in_table(data_dict, frame_data_dict, frame_num, box_id, obj_class_dict, homography, box_id_new):
 	obj_id = frame_data_dict[box_id]['id']
	obj_class = frame_data_dict[box_id]['class']
	obj_left = frame_data_dict[box_id]['left']
	obj_right = frame_data_dict[box_id]['right']
	obj_top = frame_data_dict[box_id]['top']
	obj_bottom = frame_data_dict[box_id]['bottom']
	matched_to_box_id = frame_data_dict[box_id]['matched_to_box_id']
	number_of_matched_boxes = frame_data_dict[box_id]['number_of_matched_boxes']
	proj_x_tl = obj_left
	proj_y_tl = obj_top
	proj_x_tr = obj_right
	proj_y_tr = obj_top
	proj_x_bl = obj_left
	proj_y_bl = obj_bottom
	proj_x_br = obj_right
	proj_y_br = obj_bottom
	frame_data_dict[box_id]['tl'] = [proj_x_tl, proj_y_tl]
	frame_data_dict[box_id]['tr'] = [proj_x_tr, proj_y_tr]
	frame_data_dict[box_id]['bl'] = [proj_x_bl, proj_y_bl]
	frame_data_dict[box_id]['br'] = [proj_x_br, proj_y_br]

	if matched_to_box_id == -1:
		if homography is not None:
			filtered_proj_tl = cvutils.projectArray(homography, np.array([proj_x_tl, proj_y_tl]).reshape(2, 1))
			filtered_proj_x_tl = filtered_proj_tl[0][0]
			filtered_proj_y_tl = filtered_proj_tl[1][0]
			filtered_proj_tr = cvutils.projectArray(homography, np.array([proj_x_tr, proj_y_tr]).reshape(2, 1))
			filtered_proj_x_tr = filtered_proj_tr[0][0]
			filtered_proj_y_tr = filtered_proj_tr[1][0]
			filtered_proj_bl = cvutils.projectArray(homography, np.array([proj_x_bl, proj_y_bl]).reshape(2, 1))
			filtered_proj_x_bl = filtered_proj_bl[0][0]
			filtered_proj_y_bl = filtered_proj_bl[1][0]
			filtered_proj_br = cvutils.projectArray(homography, np.array([proj_x_br, proj_y_br]).reshape(2, 1))
			filtered_proj_x_br = filtered_proj_br[0][0]
			filtered_proj_y_br = filtered_proj_br[1][0]
		else:
			filtered_proj_x_tl = proj_x_tl
			filtered_proj_y_tl = proj_y_tl
			filtered_proj_x_tr = proj_x_tr
			filtered_proj_y_tr = proj_y_tr
			filtered_proj_x_bl = proj_x_bl
			filtered_proj_y_bl = proj_y_bl
			filtered_proj_x_br = proj_x_br
			filtered_proj_y_br = proj_y_br
		vel_x_tl = 0
		vel_y_tl = 0
		vel_x_tr = 0
		vel_y_tr = 0
		vel_x_bl = 0
		vel_y_bl = 0
		vel_x_br = 0
		vel_y_br = 0
		vtl_list_x = [0]
		vtl_list_y = [0]
		vtr_list_x = [0]
		vtr_list_y = [0]
		vbl_list_x = [0]
		vbl_list_y = [0]
		vbr_list_x = [0]
		vbr_list_y = [0]
	else:
		tl_list_x, tl_list_y, vtl_list_x, vtl_list_y, _ = position_list(data_dict, [proj_x_tl, proj_y_tl], matched_to_box_id, frame_num, box_id, [], [], [], [], window_size, 'tl')
		tr_list_x, tr_list_y, vtr_list_x, vtr_list_y, _ = position_list(data_dict, [proj_x_tr, proj_y_tr], matched_to_box_id, frame_num, box_id, [], [], [], [], window_size, 'tr')
		bl_list_x, bl_list_y, vbl_list_x, vbl_list_y, _ = position_list(data_dict, [proj_x_bl, proj_y_bl], matched_to_box_id, frame_num, box_id, [], [], [], [], window_size, 'bl')
		br_list_x, br_list_y, vbr_list_x, vbr_list_y, _ = position_list(data_dict, [proj_x_br, proj_y_br], matched_to_box_id, frame_num, box_id, [], [], [], [], window_size, 'br')
		if homography is not None:
			filtered_proj_tl = cvutils.projectArray(homography, np.array([filter_list_mean(tl_list_x), filter_list_mean(tl_list_y)]).reshape(2, 1))
			filtered_proj_x_tl = filtered_proj_tl[0][0]
			filtered_proj_y_tl = filtered_proj_tl[1][0]
			filtered_proj_tr = cvutils.projectArray(homography, np.array([filter_list_mean(tr_list_x), filter_list_mean(tr_list_y)]).reshape(2, 1))
			filtered_proj_x_tr = filtered_proj_tr[0][0]
			filtered_proj_y_tr = filtered_proj_tr[1][0]
			filtered_proj_bl = cvutils.projectArray(homography, np.array([filter_list_mean(bl_list_x), filter_list_mean(bl_list_y)]).reshape(2, 1))
			filtered_proj_x_bl = filtered_proj_bl[0][0]
			filtered_proj_y_bl = filtered_proj_bl[1][0]
			filtered_proj_br = cvutils.projectArray(homography, np.array([filter_list_mean(br_list_x), filter_list_mean(br_list_y)]).reshape(2, 1))
			filtered_proj_x_br = filtered_proj_br[0][0]
			filtered_proj_y_br = filtered_proj_br[1][0]
		else:
			filtered_proj_x_tl = filter_list_mean(tl_list_x)
			filtered_proj_y_tl = filter_list_mean(tl_list_y)
			filtered_proj_x_tr = filter_list_mean(tr_list_x)
			filtered_proj_y_tr = filter_list_mean(tr_list_y)
			filtered_proj_x_bl = filter_list_mean(bl_list_x)
			filtered_proj_y_bl = filter_list_mean(bl_list_y)
			filtered_proj_x_br = filter_list_mean(br_list_x)
			filtered_proj_y_br = filter_list_mean(br_list_y)
		vel_x_tl = filtered_proj_x_tl - data_dict[frame_num - 1][matched_to_box_id]['filtered_tl'][0]
		vel_y_tl = filtered_proj_y_tl - data_dict[frame_num - 1][matched_to_box_id]['filtered_tl'][1]
		vel_x_tr = filtered_proj_x_tr - data_dict[frame_num - 1][matched_to_box_id]['filtered_tr'][0]
		vel_y_tr = filtered_proj_y_tr - data_dict[frame_num - 1][matched_to_box_id]['filtered_tr'][1]
		vel_x_bl = filtered_proj_x_bl - data_dict[frame_num - 1][matched_to_box_id]['filtered_bl'][0]
		vel_y_bl = filtered_proj_y_bl - data_dict[frame_num - 1][matched_to_box_id]['filtered_bl'][1]
		vel_x_br = filtered_proj_x_br - data_dict[frame_num - 1][matched_to_box_id]['filtered_br'][0]
		vel_y_br = filtered_proj_y_br - data_dict[frame_num - 1][matched_to_box_id]['filtered_br'][1]

	frame_data_dict[box_id]['vtl'] = [vel_x_tl, vel_y_tl]
	frame_data_dict[box_id]['vtr'] = [vel_x_tr, vel_y_tr]
	frame_data_dict[box_id]['vbl'] = [vel_x_bl, vel_y_bl]
	frame_data_dict[box_id]['vbr'] = [vel_x_br, vel_y_br]
	filter_v_x = filter_list_mean(vtl_list_x + vtr_list_x + vbl_list_x + vbr_list_x)
	filter_v_y = filter_list_mean(vtl_list_y + vtr_list_y + vbl_list_y + vbr_list_y)

	frame_data_dict[box_id]['filtered_tl'] = [filtered_proj_x_tl, filtered_proj_y_tl]
	frame_data_dict[box_id]['filtered_tr'] = [filtered_proj_x_tr, filtered_proj_y_tr]
	frame_data_dict[box_id]['filtered_bl'] = [filtered_proj_x_bl, filtered_proj_y_bl]
	frame_data_dict[box_id]['filtered_br'] = [filtered_proj_x_br, filtered_proj_y_br]

	if obj_id not in obj_class_dict:
		obj_class_dict[obj_id] = gen2_tools.userType2Num('unknown')
		c.execute('INSERT INTO objects VALUES (?, ?, ?)', [obj_id, gen2_tools.userType2Num('unknown'), 1])
	if number_of_matched_boxes >= fps / 2 and obj_class_dict[obj_id] != obj_class: # each object has to be in the video for at least 0.5s (fps/2), otherwise it will not be classified and used
		obj_class_dict[obj_id] = obj_class
		c.execute('UPDATE objects SET road_user_type = ? WHERE object_id = ?', [gen2_tools.userType2Num(obj_class), obj_id])
	if number_of_matched_boxes >= window_size:
	  	offset = int(np.ceil(window_size / 2.))
		c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 0])
		c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)',          [box_id_new * 4 + 0, frame_num - offset, filtered_proj_x_tl, filtered_proj_y_tl])
		c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)',         [box_id_new * 4 + 0, frame_num - offset, filter_v_x, filter_v_y])
		c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 1])
		c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)',          [box_id_new * 4 + 1, frame_num - offset, filtered_proj_x_tr, filtered_proj_y_tr])
		c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)',         [box_id_new * 4 + 1, frame_num - offset, filter_v_x, filter_v_y])
		c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 2])
		c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)',          [box_id_new * 4 + 2, frame_num - offset, filtered_proj_x_bl, filtered_proj_y_bl])
		c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)',         [box_id_new * 4 + 2, frame_num - offset, filter_v_x, filter_v_y])
		c.execute('INSERT INTO objects_features VALUES (?, ?)', [obj_id, box_id_new * 4 + 3])
		c.execute('INSERT INTO positions VALUES (?, ?, ?, ?)',          [box_id_new * 4 + 3, frame_num - offset, filtered_proj_x_br, filtered_proj_y_br])
		c.execute('INSERT INTO velocities VALUES (?, ?, ?, ?)',         [box_id_new * 4 + 3, frame_num - offset, filter_v_x, filter_v_y])
		box_id_new += 1
	return box_id_new

box_id_new = 0
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
except Exception as e:
	homography = None
	print 'Warning: No homography found!'
	print e
frame_num = -1
obj_id = 0
box_id = 0
next_obj_id = 0
data_dict = {}
obj_class_dict = {}
match_candidates = {}
window_size = int(fps / 2)
history_window = fps
lines = open(data_file_name).read().splitlines()
for line in lines:
	currentline = line.split(', ')
	if currentline[0] == 'next_frame':
		matched_box = []
		if frame_num > 0:
			#####################################################################################################
			iteration = 1
			while iteration <= history_window and frame_num >= iteration:
				match_score, frame_data_dict, matched_box, match_candidates, data_dict, box_id, box_id_new = find_best_match(match_score, frame_data_dict, data_dict, matched_box, match_candidates, iteration, box_id, box_id_new)
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
				box_id_new = put_in_table(data_dict, frame_data_dict, frame_num, bi, obj_class_dict, homography, box_id_new)
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
		'id': -1,
		'number_of_matched_boxes': 1,
		'matched_to_box_id': -1,
		'tl': 0,
		'tr': 0,
		'bl': 0,
		'br': 0,
		'vtl': 0,
		'vtr': 0,
		'vbl': 0,
		'vbr': 0,
		'filtered_tl': 0,
		'filtered_tr': 0,
		'filtered_bl': 0,
		'filtered_br': 0}
		box_id += 1

conn.commit()
conn.close()
