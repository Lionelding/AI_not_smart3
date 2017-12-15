import cv2
import numpy as np
import sqlite3

UN2T = {}
UN2T[0] = 'unknown'
UN2T[1] = 'car'
UN2T[2] = 'pedestrian'
UN2T[3] = 'motorcycle'
UN2T[4] = 'bicycle'
UN2T[5] = 'bus'
UN2T[6] = 'truck'
UT2N = {}
UT2N['unknown'] = 0
UT2N['car'] = 1
UT2N['pedestrian'] = 2
UT2N['motorcycle'] = 3
UT2N['bicycle'] = 4
UT2N['bus'] = 5
UT2N['truck'] = 6

def userType2Num(t):
    try:
        return UT2N[t]
    except:
        return 0

def userNum2Type(n):
    return UN2T[n]

def projectedPoints(point, homography):
    point = np.append(np.array(point), 1)
    prod = np.dot(homography, point)
    proj_point = prod[:2] / prod[2]
    return proj_point.tolist()

def videoToWorldProject(point, homography):
    homography = np.linalg.inv(homography)
    proj_point = projectedPoints(point, homography)
    return proj_point

def worldToVideoProject(point, homography):
    proj_point = projectedPoints(point, homography)
    return map(int, proj_point)

def load_database(trajectoryType, cursor):
    if trajectoryType == 'feature':
        queryStatement = 'SELECT * from ' + tableName + ' ORDER BY trajectory_id, frame_number'
    elif trajectoryType == 'object':
        queryStatement = 'SELECT OF.object_id, P.frame_number, avg(P.x_coordinate), avg(P.y_coordinate) from ' + tableName + ' P, objects_features OF WHERE P.trajectory_id = OF.trajectory_id GROUP BY OF.object_id, P.frame_number ORDER BY OF.object_id, P.frame_number'
    print queryStatement
    cursor.execute(queryStatement)

    objId = -1
    obj = None
    objects = []
    for row in cursor:
        if row[0] != objId:
            objId = row[0]
            if obj is not None and obj.length() == obj.positions.length():
                objects.append(obj)
            elif obj is not None:
                print('Object {} is missing {} positions'.format(obj.getNum(), int(obj.length())-obj.positions.length()))
            obj = moving.MovingObject(row[0], timeInterval = moving.TimeInterval(row[1], row[1]), positions = moving.Trajectory([[row[2]],[row[3]]]))
        else:
            obj.timeInterval.last = row[1]
            obj.positions.addPositionXY(row[2],row[3])

    if obj is not None and (obj.length() == obj.positions.length() or (timeStep is not None and npceil(obj.length()/timeStep) == obj.positions.length())):
        objects.append(obj)
    elif obj is not None:
        print('Object {} is missing {} positions'.format(obj.getNum(), int(obj.length())-obj.positions.length()))

    return objects
