# nohup python aws2_just_safety s3_video_location s3_sqlite_location video_extention
import subprocess
import os
import shutil
import sys
import numpy as np
import glob

s3_location = sys.argv[1] #"s3://brisk-zerys/Cochabamba,Bolivia/BRT Corridor/1. Cruce peatonal en terminal de bus de Av. Ayacucho and Puntata/"
s3_sqlite_location = sys.argv[2] #"s3://brisk-zerys/Cochabamba,Bolivia/BRT Corridor/1. Cruce peatonal en terminal de bus de Av. Ayacucho and Puntata/"
video_extention = sys.argv[3] #'dav'

file_list = subprocess.check_output('s3cmd ls "' + s3_location + '"' + """ | awk '{ s = ""; for (i = 4; i <= NF; i++) s = s $i " "; print s }'""", shell = True)
file_list = file_list.split('\n')

if os.path.isdir('./config/'):
    shutil.rmtree('./config/')

subprocess.call('s3cmd get --recursive "' + s3_location + 'config"', shell = True)
safety_scenarios = glob.glob('./config/safety_config*')

try:
    for video in file_list:
        if video == '':
            continue
        video_name = '"' + video[:-1].split('/')[-1] + '"'
        if video_name[-4:-1] != video_extention:
            continue
        subprocess.call('s3cmd get "' + video[:-1] + '" ' + video_name, shell = True)
        sqlitefile_name = video[:-1].split('/')[-1][:-3] + 'sqlite'
        subprocess.call('s3cmd get "' + s3_sqlite_location + sqlitefile_name + '" ' + sqlitefile_name, shell = True)
        for scenario_config in safety_scenarios:
            output_folder = './output_' + scenario_config.split('/')[-1].split('.')[0]
            subprocess.call('python /home/ubuntu/helix-analysis/NEW/safety.py ' + scenario_config + ' 1 ' + video_name + ' ' + sqlitefile_name + ' ' + output_folder, shell = True)
            subprocess.call('s3cmd put --recursive ' + output_folder + ' "' + s3_location + '"', shell = True)
            shutil.rmtree(output_folder)
        try:
            os.remove(video_name)
        except:
            os.remove(video_name[1:-1])
        try:
            os.remove(sqlitefile_name)
        except:
            os.remove(sqlitefile_name[1:-1])
except Exception as e:
    print e

subprocess.call('sudo shutdown', shell = True)
