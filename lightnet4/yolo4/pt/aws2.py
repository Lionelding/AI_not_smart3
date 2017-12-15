# nohup python aws.py s3_location video_extention
import subprocess
import os
import shutil
import sys
import numpy as np
import glob

s3_location = sys.argv[1] #"s3://brisk-zerys/Cochabamba,Bolivia/BRT Corridor/1. Cruce peatonal en terminal de bus de Av. Ayacucho and Puntata/"
video_extention = sys.argv[2] #'dav'

file_list = subprocess.check_output('s3cmd ls "' + s3_location + '"' + """ | awk '{ s = ""; for (i = 4; i <= NF; i++) s = s $i " "; print s }'""", shell = True)
file_list = file_list.split('\n')

if os.path.isdir('./config/'):
    shutil.rmtree('./config/')

subprocess.call('s3cmd get --recursive "' + s3_location + 'config"', shell = True)
safety_scenarios = glob.glob('./config/safety_config*')

f = open('/home/ubuntu/local_job_started.txt', 'a')
f.write('0')
f.close()

try:
    for video in file_list:
        if video == '':
            continue
        video_name = '"' + video[:-1].split('/')[-1] + '"'
        if video_name[-4:-1] != video_extention:
            continue
        job_started = []
        if os.path.isfile('./config/job_started.txt'):
            os.remove('./config/job_started.txt') # update the file
            subprocess.call('s3cmd get "' + s3_location + 'config/job_started.txt" ./config/job_started.txt', shell = True)
            job_started = np.loadtxt('./config/job_started.txt', dtype = 'str', delimiter = '\n')
            if video in job_started: # another instanse is already running this video
                continue
        job_started = np.append(job_started, video)
        np.savetxt('./config/job_started.txt', job_started, fmt = '%s')
        subprocess.call('s3cmd put ./config/job_started.txt "' + s3_location + 'config/job_started.txt"', shell = True)
        subprocess.call('s3cmd get "' + video[:-1] + '" ' + video_name, shell = True)
        if os.path.isfile('file.txt'):
            os.remove('file.txt')
        if os.path.isfile('file.sqlite'):
            os.remove('file.sqlite')
        if video_extention == 'dav' or video_extention == 'avi':
            new_video_name = video[:-1].split('/')[-1][:-3] + 'mp4'
            subprocess.call('ffmpeg -i ' + video_name + ' -preset veryfast ' + new_video_name, shell = True)
            subprocess.call('s3cmd put ' + new_video_name + ' "' + s3_location + '"', shell = True)
            os.remove(video_name[1:-1])
            video_name = new_video_name
            textfile_name = video_name[:-3] + 'txt'
            sqlitefile_name = video_name[:-3] + 'sqlite'
        else:
            textfile_name = video[:-1].split('/')[-1][:-3] + 'txt'
            sqlitefile_name = video[:-1].split('/')[-1][:-3] + 'sqlite'
        fps = subprocess.check_output('ffmpeg -i ' + video_name + ' 2>&1 | sed -n "s/.*, \(.*\) fp.*/\\1/p"', shell = True).strip()
        subprocess.call('./darknet detector demo data/mio.data cfg/yolo-mio.cfg cfg/yolo-mio.weights ' + video_name + ' -thresh .3', shell = True)
        if not os.path.isfile('file.txt'): # problem with gpu unit of this ec2 machine
            os.remove('./config/job_started.txt') # update the file
            subprocess.call('s3cmd get "' + s3_location + 'config/job_started.txt" ./config/job_started.txt', shell = True)
            job_started = np.loadtxt('./config/job_started.txt', dtype = 'str', delimiter = '\n')
            ind = np.where(job_started == video)[0][0]
            job_started = np.delete(job_started, ind)
            np.savetxt('./config/job_started.txt', job_started, fmt = '%s')
            subprocess.call('s3cmd put ./config/job_started.txt "' + s3_location + 'config/job_started.txt"', shell = True)
            break
        subprocess.call('python /home/ubuntu/gen2/pre_filtering.py file.txt', shell = True)
        os.remove('file.txt')
        os.rename('file.txt.tmp', 'file.txt')
        subprocess.call('s3cmd put file.txt "' + s3_location + 'text_files/' + textfile_name + '"', shell = True)
        subprocess.call('python /home/ubuntu/gen2/read_track.py file.txt ' + fps + ' ./config/homography.txt', shell = True)
        subprocess.call('python /home/ubuntu/gen2/filtering.py file.sqlite ' + fps, shell = True)
        subprocess.call('s3cmd put file.sqlite "' + s3_location + 'sqlite_files/' + sqlitefile_name + '"', shell = True)
        for scenario_config in safety_scenarios:
            output_folder = './output_' + scenario_config.split('/')[-1].split('.')[0]
            subprocess.call('python /home/ubuntu/helix-analysis/NEW/safety.py ' + scenario_config + ' 1 ' + video_name + ' file.sqlite ' + output_folder, shell = True)
            subprocess.call('s3cmd put --recursive ' + output_folder + ' "' + s3_location + '"', shell = True)
            shutil.rmtree(output_folder)
        try:
            os.remove(video_name)
        except:
            os.remove(video_name[1:-1])
except Exception as e:
    print e

f = open('/home/ubuntu/local_job_ended.txt', 'a')
f.write('0')
f.close()

fs = open('/home/ubuntu/local_job_started.txt', 'r')
fe = open('/home/ubuntu/local_job_ended.txt', 'r')
if len(fs.read()) == len(fe.read()):
    subprocess.call('sudo shutdown', shell = True)
    fs.close()
    fe.close()
    os.remove('/home/ubuntu/local_job_started.txt')
    os.remove('/home/ubuntu/local_job_ended.txt')
fs.close()
fe.close()
