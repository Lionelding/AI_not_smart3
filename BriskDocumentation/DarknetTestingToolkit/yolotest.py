import subprocess
import os
import csv
import os.path


llist = os.listdir("/home/liqiang/Topview/Localization_MIOTCD/MIO-TCD-Localization/test/")
trainpath="/home/liqiang/Topview/Localization_MIOTCD/MIO-TCD-Localization/test/"

print len(llist)

if os.path.isfile("mycsv.csv"):
	os.remove("mycsv.csv")



for i in range(len(llist)):

	print i
	sinlgeimage=llist[i]
	print sinlgeimage

	singleDetection=os.popen("./darknet detector test data/mio0.names cfg/yolo-mio0.cfg weights/yolo-mio_80000.weights "+trainpath+sinlgeimage).read()


	objectInSingle=singleDetection.split("\n")[1:-1]
	#print objectInSingle


	csvfile = open("mycsv.csv", 'a')
	writer = csv.writer(csvfile, delimiter=',')
	for j in range(len(objectInSingle)):
		singleObject=objectInSingle[j].split(", ")
		singleObject=[sinlgeimage[:-4]]+singleObject
		print singleObject

		writer.writerow(singleObject)

	csvfile.close()





