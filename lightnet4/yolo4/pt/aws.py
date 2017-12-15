# nohup python aws.py s3_location video_extention
import subprocess
import os
import shutil
import sys

s3_location = sys.argv[1] #"s3://brisk-zerys/Cochabamba,Bolivia/BRT Corridor/1. Cruce peatonal en terminal de bus de Av. Ayacucho and Puntata/"
video_extention = sys.argv[2] #'dav'
number_of_instances = int(sys.argv[3]) #5
process_per_instance = int(sys.argv[4]) #3
process_number = int(sys.argv[5]) #1

file_list = subprocess.check_output('s3cmd ls "' + s3_location + '"' + """ | awk '{ s = ""; for (i = 4; i <= NF; i++) s = s $i " "; print s }'""", shell = True)
file_list = file_list.split('\n')
i = -1
if os.path.isfile('/home/ubuntu/state.txt'):
    os.remove('/home/ubuntu/state.txt')

try:
    for video in file_list:
        if video == '':
            continue
        video_name = '"' + video[:-1].split('/')[-1] + '"'
        if video_name[-4:-1] != video_extention:
            continue
        i += 1
        if i % (number_of_instances * process_per_instance) != process_number - 1:
            continue
        subprocess.call('s3cmd get "' + video[:-1] + '" ' + video_name, shell = True)
        if os.path.isfile('file.txt'):
            os.remove('file.txt')
        if os.path.isfile('file.sqlite'):
            os.remove('file.sqlite')
        if video_extention == 'dav':
            new_video_name = video[:-1].split('/')[-1][-25:-17] + '_' + video[:-1].split('/')[-1][-17:-11] + '.mp4'
            subprocess.call('ffmpeg -i ' + video_name + ' ' + new_video_name, shell = True)
            subprocess.call('s3cmd put ' + new_video_name + ' "' + s3_location + '"', shell = True)
            os.remove(video_name[1:-1])
            video_name = new_video_name
            textfile_name = video_name[:-3] + 'txt'
            sqlitefile_name = video_name[:-3] + 'sqlite'
        else: # this has to be checked
            textfile_name = video[:-1].split('/')[-1][:-3] + 'txt'
            sqlitefile_name = video[:-1].split('/')[-1][:-3] + 'sqlite'
        fps = subprocess.check_output('ffmpeg -i ' + video_name + ' 2>&1 | sed -n "s/.*, \(.*\) fp.*/\\1/p"', shell = True).strip()
        subprocess.call('./darknet detector demo data/mio.data cfg/yolo-mio.cfg cfg/yolo-mio.weights ' + video_name + ' -thresh .3', shell = True)
        subprocess.call('s3cmd put file.txt "' + s3_location + textfile_name + '"', shell = True)
        subprocess.call('python /home/ubuntu/gen2/read_track.py file.txt ' + fps + ' /home/ubuntu/config/homography.txt', shell = True)
        subprocess.call('python /home/ubuntu/gen2/filtering.py file.sqlite ' + fps, shell = True)
        subprocess.call('s3cmd put file.sqlite "' + s3_location + sqlitefile_name + '"', shell = True)
        subprocess.call('python /home/ubuntu/helix-analysis/NEW/safety.py /home/ubuntu/config/safety_config.txt 1 ' + video_name + ' file.sqlite ./output', shell = True)
        subprocess.call('s3cmd put --recursive ./output "' + s3_location + '"', shell = True)
        try:
            os.remove(video_name)
        except:
            os.remove(video_name[1:-1])
        shutil.rmtree('output/')
except Exception as e:
    print e

f = open('/home/ubuntu/state.txt', 'a')
f.write('0')
f.close()
f = open('/home/ubuntu/state.txt', 'r')
if len(f.read()) == process_per_instance:
    subprocess.call('sudo shutdown', shell = True)
f.close()
