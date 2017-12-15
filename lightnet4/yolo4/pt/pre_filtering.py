#  pre_filtering.py file_name_from_yolo video(optional, only for demo)
import numpy as np
import sys

data_file_name = sys.argv[1] # text file from YOLO
demo = False
if len(sys.argv) > 2: # generate a demo video
	demo = True
	import cv2
	capture = cv2.VideoCapture(sys.argv[2])
	fourcc = cv2.cv.CV_FOURCC(*'XVID')
	width = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_WIDTH))
	height = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT))
	fps = int(round(capture.get(cv2.cv.CV_CAP_PROP_FPS)))
	out = cv2.VideoWriter('demo.avi', fourcc, fps, (width, height))
	ret = False
filtered_file = open(data_file_name + '.tmp', 'w')
current_frame_objects = []
lines = open(data_file_name).read().splitlines()
for line in lines:
	currentline = line.split(', ')
	if currentline[0] == 'next_frame':
		if demo:
			for cfo in current_frame_objects:
				cv2.rectangle(img, (cfo[2], cfo[4]), (cfo[3], cfo[5]), (255,0,0), 2)
		for i in range(len(current_frame_objects)):
			for j in range(i + 1, len(current_frame_objects)):
				if current_frame_objects[i][0] != current_frame_objects[j][0]:
					continue
				common_surface = max(0, (min(current_frame_objects[i][3], current_frame_objects[j][3]) - max(current_frame_objects[i][2], current_frame_objects[j][2]))) * max(0, (min(current_frame_objects[i][5], current_frame_objects[j][5]) - max(current_frame_objects[i][4], current_frame_objects[j][4])))
				surface_i = (current_frame_objects[i][3] - current_frame_objects[i][2]) * (current_frame_objects[i][5] - current_frame_objects[i][4])
				surface_j = (current_frame_objects[j][3] - current_frame_objects[j][2]) * (current_frame_objects[j][5] - current_frame_objects[j][4])
				min_surface = min(surface_i, surface_j)
				max_surface = max(surface_i, surface_j)
				max_common_surface_ratio = float(common_surface) / float(min_surface)
				min_common_surface_ratio = float(common_surface) / float(max_surface)
				if (current_frame_objects[i][0] != 'pedestrian' and min_common_surface_ratio > .8) or (current_frame_objects[i][0] == 'pedestrian' and max_common_surface_ratio > .5):
					if current_frame_objects[i][1] < current_frame_objects[j][1]: # keep the bounding box with higher probablity
						if demo:
							cv2.rectangle(img, (current_frame_objects[i][2], current_frame_objects[i][4]), (current_frame_objects[i][3], current_frame_objects[i][5]), (0,0,255), 2)
						current_frame_objects[i][0] += '_noise'
					else:
						if demo:
							cv2.rectangle(img, (current_frame_objects[j][2], current_frame_objects[j][4]), (current_frame_objects[j][3], current_frame_objects[j][5]), (0,0,255), 2)
						current_frame_objects[j][0] += '_noise'
		if demo:
			if ret:
		 		out.write(img)
		for cfo in current_frame_objects:
			filtered_file.write('%s, %i, %i, %i, %i, %i \n' % (cfo[0], cfo[1], cfo[2], cfo[3], cfo[4], cfo[5]))
		filtered_file.write('next_frame\n')
		if demo:
			ret, img = capture.read()
		current_frame_objects = []
	else:
		obj_class = currentline[0]
		obj_probability = int(currentline[1])
		obj_left = int(currentline[2])
		obj_right = int(currentline[3])
		obj_top = int(currentline[4])
		obj_bottom = int(currentline[5])
		if obj_class == 'bus' or obj_class == 'work_van' or obj_class == 'truck' or obj_class == 'motorized_vehicle' or obj_class == 'pickup_truck' or obj_class == 'single_unit_truck' or obj_class == 'articulated_truck':
			obj_class = 'car'
		current_frame_objects.append([obj_class, obj_probability, obj_left, obj_right, obj_top, obj_bottom])
