#! /usr/bin/env python

import sys,getopt

sys.path.append('/home/brisk/trafficintelligence/python/')

import storage
import sohailcvutils2

from numpy.linalg.linalg import inv
from numpy import loadtxt
from ConfigParser import ConfigParser

options, args = getopt.getopt(sys.argv[1:], 'hi:d:o:f:l:',['help'])
# alternative long names are a pain to support ,'video-filename=','database-filename=', 'type='

options = dict(options)

print options, args

if '--help' in options.keys() or '-h' in options.keys() or len(sys.argv) == 1:
    print('Usage: '+sys.argv[0]+' --help|-h -i video-filename -d database-filename [-o image2world_homography] [-f first_frame] [-l last_frame]\n'
          'Order matters between positional and named arguments\n')
    sys.exit()

videoFilename = options['-i']
databaseFilename = options['-d']
homography = None
if '-o' in options.keys():
    homography = inv(loadtxt(options['-o']))
firstFrameNum = 0
if '-f' in options.keys():
    firstFrameNum = int(options['-f'])
lastFrameNum = None
if '-l' in options.keys():
    lastFrameNum = int(options['-l'])

print "Loading Objects"
objects = storage.loadTrajectoriesFromSqlite(databaseFilename, 'object')
# print "Loading Features"
# features = storage.loadTrajectoriesFromSqlite(databaseFilename, 'feature')
features = []
sohailcvutils2.displayTrajectories(videoFilename, objects, features, homography, firstFrameNum, lastFrameNum)
