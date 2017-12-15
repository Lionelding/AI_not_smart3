# compute_homography.py points_file pixels_per_meter
import numpy as np
import cv2
import sys

points = np.loadtxt(sys.argv[1], dtype = np.float32)
pixels_per_meter = float(sys.argv[2])

world_points = points[:2, :].T / pixels_per_meter
video_points = points[2:, :].T
homography = cv2.findHomography(video_points, world_points)[0]

np.savetxt('homography.txt', homography)
