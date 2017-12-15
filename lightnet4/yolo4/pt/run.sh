#!/bin/bash 

# ./run.sh s3_location video_extention yolo_resolution parallel_process_on_this_machine
cd
cd gen2/
git fetch --all
git reset --hard origin/master
git pull
cd
cd helix-analysis/
git fetch --all
git reset --hard origin/master
git pull
cd

if [ -f local_job_ended.txt ]; then 
	rm local_job_ended.txt
fi
if [ -f local_job_started.txt ]; then 
	rm local_job_started.txt
fi

i=1
while [ $i -le $4 ]
do
	cd darknet$i
	sed -i '/height=/ c\height='$3 ./cfg/yolo-mio.cfg
	sed -i '/width=/ c\width='$3 ./cfg/yolo-mio.cfg
	if [ -f nohup.out ]; then 
		rm nohup.out
	fi
	nohup python ../gen2/aws2.py "$1" $2 & 
	cd 
	sleep 30
	(( i++ ))
done

