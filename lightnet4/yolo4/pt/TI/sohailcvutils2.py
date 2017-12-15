#! /usr/bin/env python
'''Image/Video utilities'''

#import Image, ImageDraw # PIL
try:
    import cv2
    opencvExists = True
except ImportError:
    print('OpenCV library could not be loaded')
    opencvExists = False
from sys import stdout

import utils

#import aggdraw # agg on top of PIL (antialiased drawing)
#import utils

__metaclass__ = type

cvRed = (0,0,255)
cvGreen = (0,255,0)
cvBlue = (255,0,0)
cvYellow = (0,255,255)
cvColors = utils.PlottingPropertyValues([cvRed,
                                         cvGreen,
                                         cvBlue])

def quitKey(key):
    return chr(key&255)== 'q' or chr(key&255) == 'Q'

def saveKey(key):
    return chr(key&255) == 's'

def drawLines(filename, origins, destinations, w = 1, resultFilename='image.png'):
    '''Draws lines over the image '''

    img = Image.open(filename)

    draw = ImageDraw.Draw(img)
    #draw = aggdraw.Draw(img)
    #pen = aggdraw.Pen("red", width)
    for p1, p2 in zip(origins, destinations):
        draw.line([p1.x, p1.y, p2.x, p2.y], width = w, fill = (256,0,0))
        #draw.line([p1.x, p1.y, p2.x, p2.y], pen)
    del draw

    #out = utils.openCheck(resultFilename)
    img.save(resultFilename)

def matlab2PointCorrespondences(filename):
    '''Loads and converts the point correspondences saved
    by the matlab camera calibration tool'''
    from numpy.lib.io import loadtxt, savetxt
    from numpy.lib.function_base import append
    points = loadtxt(filename, delimiter=',')
    savetxt(utils.removeExtension(filename)+'-point-correspondences.txt',append(points[:,:2].T, points[:,3:].T, axis=0))

def loadPointCorrespondences(filename):
    '''Loads and returns the corresponding points in world (first 2 lines) and image spaces (last 2 lines)'''
    from numpy.lib.npyio import loadtxt
    from numpy import float32
    points = loadtxt(filename, dtype=float32)
    return  (points[:2,:].T, points[2:,:].T) # (world points, image points)

def cvMatToArray(cvmat):
    '''Converts an OpenCV CvMat to numpy array.'''
    from numpy.core.multiarray import zeros
    a = zeros((cvmat.rows, cvmat.cols))#array([[0.0]*cvmat.width]*cvmat.height)
    for i in xrange(cvmat.rows):
        for j in xrange(cvmat.cols):
            a[i,j] = cvmat[i,j]
    return a

if opencvExists:
    def computeHomography(srcPoints, dstPoints, method=0, ransacReprojThreshold=0.0):
        '''Returns the homography matrix mapping from srcPoints to dstPoints (dimension Nx2)'''
        H, mask = cv2.findHomography(srcPoints, dstPoints, method, ransacReprojThreshold)
        return H

    # def arrayToCvMat(a, t = cv2.cv.CV_64FC1):
    #     '''Converts a numpy array to an OpenCV CvMat, with default type CV_64FC1.'''
    #     cvmat = cv2.cv.CreateMat(a.shape[0], a.shape[1], t)
    #     for i in range(cvmat.rows):
    #         for j in range(cvmat.cols):
    #             cvmat[i,j] = a[i,j]
    #     return cvmat

    def draw(img, positions, color, fps, lastCoordinate = None):
        firstCoordinate = max(0, lastCoordinate - 30 * fps)
        last = lastCoordinate + 1
        if lastCoordinate != None and lastCoordinate >=0:
            last = min(positions.length()-1, lastCoordinate)
        for i in range(firstCoordinate, last-1):
            cv2.line(img, positions[i].asint().astuple(), positions[i+1].asint().astuple(), color,2)

    def playVideo(filename, firstFrameNum = 0, frameRate = -1):
        '''Plays the video'''
        wait = 5
        if frameRate > 0:
            wait = int(round(1000./frameRate))
        capture = cv2.VideoCapture(filename)
        if capture.isOpened():
            key = -1
            ret = True
            frameNum = firstFrameNum
            if cv2.__version__[0] == '3': # opencv 3
                capture.set(cv2.CAP_PROP_POS_FRAMES, firstFrameNum)
            else:
                capture.set(cv2.cv.CV_CAP_PROP_POS_FRAMES, firstFrameNum)
            while ret and not quitKey(key):
                ret, img = capture.read()
                if ret:
                    print('frame {0}'.format(frameNum))
                    frameNum+=1
                    cv2.imshow('frame', img)
                    key = cv2.waitKey(wait)

    def getImagesFromVideo(filename, nImages = 1, saveImage = False):
        '''Returns nImages images from the video sequence'''
        images = []
        capture = cv2.VideoCapture(filename)
        if capture.isOpened():
            ret = False
            numImg = 0
            while numImg<nImages:
                ret, img = capture.read()
                i = 0
                while not ret and i<10:
                    ret, img = capture.read()
                    i += 1
                if img.size>0:
                    numImg +=1
                    if saveImage:
                        cv2.imwrite('image{0:04d}.png'.format(numImg), img)
                    else:
                        images.append(img)
        return images

    def displayTrajectories(videoFilename, objects, features, homography = None, firstFrameNum = 0, lastFrameNumArg = None):
        '''Displays the objects overlaid frame by frame over the video '''
        import math, moving
        import numpy as np
        from math import ceil, log10
        capture = cv2.VideoCapture(videoFilename)
        if cv2.__version__[0] == '3': # opencv 3
            fourcc = cv2.VideoWriter_fourcc('M','J','P','G')
            fps = int(capture.get(cv2.CAP_PROP_FPS))
            width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
        else:
            fourcc = cv2.cv.CV_FOURCC(*'XVID')
            fps = int(capture.get(cv2.cv.CV_CAP_PROP_FPS)        )
            width = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_WIDTH))
            height = int(capture.get(cv2.cv.CV_CAP_PROP_FRAME_HEIGHT))
        out = cv2.VideoWriter(videoFilename+'_tracked.avi',fourcc, fps, (width,height))
        px = .2
        py = .2
        if capture.isOpened():
            key = -1
            ret = True
            frameNum = firstFrameNum
            if cv2.__version__[0] == '3': # opencv 3
                capture.set(cv2.CAP_PROP_POS_FRAMES, firstFrameNum)
            else:
                capture.set(cv2.cv.CV_CAP_PROP_POS_FRAMES, firstFrameNum)
            if not lastFrameNumArg:
                from sys import maxint
                lastFrameNum = maxint
            else:
                lastFrameNum = lastFrameNumArg
            nZerosFilename = int(ceil(log10(lastFrameNum)))
            while ret and not quitKey(key) and frameNum < lastFrameNum:
                ret, img = capture.read()
                if ret:
                    print('frame {0}'.format(frameNum))
                    for obj in objects:
                        if obj.existsAtInstant(frameNum):
                            # if obj.userType == moving.userType2Num['unknown']:
                            #     continue
                            if not hasattr(obj, 'projectedPositions'):
                                if homography is not None:
                                    obj.projectedPositions = obj.positions.project(homography)
                                else:
                                    obj.projectedPositions = obj.positions
                            draw(img, obj.projectedPositions, cvRed, fps, frameNum-obj.getFirstInstant())
                            #inst_speed = ((((obj.getVelocityAtInstant(frameNum).x)**2)+((obj.getVelocityAtInstant(frameNum).y)**2))**.5)*3.6*fps
                            # mean_speed = np.median(obj.getSpeeds())*3.6*fps
                            if obj.userType == moving.userType2Num['pedestrian']:
                                obj_color = cvYellow
                            elif obj.userType == moving.userType2Num['bicycle']:
                                obj_color = cvGreen
                            elif obj.userType == moving.userType2Num['car']:
                                obj_color = cvBlue
                            elif obj.userType == moving.userType2Num['motorcycle']:
                                obj_color = (255,255,255)
                            else:
                                obj_color = (0,0,0)
                            #cv2.putText(img, str(int(round(inst_speed,0))), obj.projectedPositions[frameNum-obj.getFirstInstant()].asint().astuple(), cv2.FONT_HERSHEY_PLAIN, 2, obj_color,2)
                            # cv2.putText(img, str(round(mean_speed,1)), obj.projectedPositions[frameNum-obj.getFirstInstant()].asint().astuple(), cv2.FONT_HERSHEY_PLAIN, 2, obj_color,2)
                            cv2.putText(img, str(obj.getNum()), obj.projectedPositions[frameNum-obj.getFirstInstant()].asint().astuple(), cv2.FONT_HERSHEY_PLAIN, 2, obj_color,2)
                            # obj.setFeatures(features)
                            # xy_tl = obj.features[(frameNum - obj.getFirstInstant()) * 4].getPositions()[0].project(homography)
                            # xy_br = obj.features[(frameNum - obj.getFirstInstant()) * 4 + 3].getPositions()[0].project(homography)
                            # cv2.rectangle(img, (int(xy_tl.x), int(xy_tl.y)), (int(xy_br.x), int(xy_br.y)), obj_color, 2)
                    out.write(img)
                    frameNum += 1

def printCvMat(cvmat, out = stdout):
    '''Prints the cvmat to out'''
    for i in xrange(cvmat.rows):
        for j in xrange(cvmat.cols):
            out.write('{0} '.format(cvmat[i,j]))
        out.write('\n')

def projectArray(homography, points):
    '''Returns the coordinates of the projected points (format 2xN points)
    through homography'''
    from numpy.core import dot
    from numpy.core.multiarray import array
    from numpy.lib.function_base import append

    if points.shape[0] != 2:
        raise Exception('points of dimension {0} {1}'.format(points.shape[0], points.shape[1]))

    if (homography!=None) and homography.size>0:
        augmentedPoints = append(points,[[1]*points.shape[1]], 0)
        prod = dot(homography, augmentedPoints)
        return prod[0:2]/prod[2]
    else:
        return p

def project(homography, p):
    '''Returns the coordinates of the projection of the point p
    through homography'''
    from numpy import array
    return projectArray(homography, array([[p[0]],[p[1]]]))

def projectTrajectory(homography, trajectory):
    '''Projects a series of points in the format
    [[x1, x2, ...],
    [y1, y2, ...]]'''
    from numpy.core.multiarray import array
    return projectArray(homography, array(trajectory))

def invertHomography(homography):
    'Returns an inverted homography'
    from numpy.linalg.linalg import inv
    invH = inv(homography)
    invH /= invH[2,2]
    return invH

if opencvExists:
    def computeTranslation(img1, img2, img1Points, maxTranslation2, minNMatches, windowSize = (5,5), level = 5, criteria = (cv2.TERM_CRITERIA_EPS, 0, 0.01)):
        '''Computes the translation of img2 with respect to img1
        (loaded using OpenCV as numpy arrays)
        img1Points are used to compute the translation

        TODO add diagnostic if data is all over the place, and it most likely is not a translation (eg zoom, other non linear distortion)'''
        from numpy.core.multiarray import array
        from numpy.lib.function_base import median
        from numpy.core.fromnumeric import sum

        nextPoints = array([])
        (img2Points, status, track_error) = cv2.calcOpticalFlowPyrLK(img1, img2, img1Points, nextPoints, winSize=windowSize, maxLevel=level, criteria=criteria)
        # calcOpticalFlowPyrLK(prevImg, nextImg, prevPts[, nextPts[, status[, err[, winSize[, maxLevel[, criteria[, derivLambda[, flags]]]]]]]]) -> nextPts, status, err
        delta = []
        for (k, (p1,p2)) in enumerate(zip(img1Points, img2Points)):
            if status[k] == 1:
                dp = p2-p1
                d = sum(dp**2)
                if d < maxTranslation2:
                    delta.append(dp)
        if len(delta) >= minNMatches:
            return median(delta, axis=0)
        else:
            print(dp)
            return None
