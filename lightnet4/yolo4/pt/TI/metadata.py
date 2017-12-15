from datetime import datetime, timedelta
from os import path
from math import floor

from numpy import zeros, loadtxt, array

from sqlalchemy import orm, create_engine, Column, Integer, Float, DateTime, String, ForeignKey, Boolean, Interval
from sqlalchemy.orm import relationship, backref, sessionmaker
from sqlalchemy.ext.declarative import declarative_base

from utils import datetimeFormat, removeExtension
from cvutils import computeUndistortMaps
from moving import TimeInterval

"""
Metadata to describe how video data and configuration files for video analysis are stored

Typical example is 

site1/view1/2012-06-01/video.avi
           /2012-06-02/video.avi
                       ...
     /view2/2012-06-01/video.avi
           /2012-06-02/video.avi
     ...

- where site1 is the path to the directory containing all information pertaining to the site, 
relative to directory of the SQLite file storing the metadata
represented by Site class
(can contain for example the aerial or map image of the site, used for projection)

- view1 is the directory for the first camera field of view (camera fixed position) at site site1
represented by CameraView class
(can contain for example the homography file, mask file and tracking configuration file)

- YYYY-MM-DD is the directory containing all the video files for that day
with camera view view1 at site site1


"""

Base = declarative_base()

class Site(Base):
    __tablename__ = 'sites'
    idx = Column(Integer, primary_key=True)
    name = Column(String) # path to directory containing all site information (in subdirectories), relative to the database position
    description = Column(String) # longer names, eg intersection of road1 and road2
    xcoordinate = Column(Float)  # ideally moving.Point, but needs to be 
    ycoordinate = Column(Float)
    mapImageFilename = Column(String) # path to map image file, relative to site name, ie sitename/mapImageFilename
    nUnitsPerPixel = Column(Float) # number of units of distance per pixel in map image
    worldDistanceUnit = Column(String, default = 'm') # make sure it is default in the database
    
    def __init__(self, name, description = "", xcoordinate = None, ycoordinate = None, mapImageFilename = None, nUnitsPerPixel = 1., worldDistanceUnit = 'm'):
        self.name = name
        self.description = description
        self.xcoordinate = xcoordinate
        self.ycoordinate = ycoordinate
        self.mapImageFilename = mapImageFilename
        self.nUnitsPerPixel = nUnitsPerPixel
        self.worldDistanceUnit = worldDistanceUnit

    def getPath(self):
        return self.name

    def getMapImageFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.getPath(), self.mapImageFilename)
        else:
            return self.mapImageFilename

    @staticmethod
    def getSite(session, siteId):
        'Returns the site(s) matching the index or the name'
        if str.isdigit(siteId):
            return session.query(Site).filter(Site.idx == int(siteId)).all()
        else:
            return session.query(Site).filter(Site.description.like('%'+siteId+'%')).all()

    
class EnvironementalFactors(Base):
    '''Represents any environmental factors that may affect the results, in particular
    * changing weather conditions
    * changing road configuration, geometry, signalization, etc.
    ex: sunny, rainy, before counter-measure, after counter-measure'''
    __tablename__ = 'environmental_factors'
    idx = Column(Integer, primary_key=True)
    startTime = Column(DateTime)
    endTime = Column(DateTime)
    description = Column(String) # eg sunny, before, after
    siteIdx = Column(Integer, ForeignKey('sites.idx'))

    site = relationship("Site", backref=backref('environmental_factors', order_by = idx))

    def __init__(self, startTime, endTime, description, site):
        'startTime is passed as string in utils.datetimeFormat, eg 2011-06-22 10:00:39'
        self.startTime = datetime.strptime(startTime, datetimeFormat)
        self.endTime = datetime.strptime(endTime, datetimeFormat)
        self.description = description
        self.site = site

class CameraType(Base):
    ''' Represents parameters of the specific camera used. 

    Taken and adapted from tvalib'''
    __tablename__ = 'camera_types'
    idx = Column(Integer, primary_key=True)
    name = Column(String)
    resX = Column(Integer)
    resY = Column(Integer)
    frameRate = Column(Float)
    frameRateTimeUnit = Column(String, default = 's')
    intrinsicCameraMatrixStr = Column(String)
    distortionCoefficientsStr = Column(String)
    undistortedImageMultiplication = Column(Float)
    
    def __init__(self, name, resX, resY, frameRate, frameRateTimeUnit = 's', trackingConfigurationFilename = None, intrinsicCameraFilename = None, intrinsicCameraMatrix = None, distortionCoefficients = None, undistortedImageMultiplication = None):
        self.name = name
        self.resX = resX
        self.resY = resY
        self.frameRate = frameRate
        self.frameRateTimeUnit = frameRateTimeUnit
        self.intrinsicCameraMatrix = None # should be np.array
        self.distortionCoefficients = None # list
        
        if trackingConfigurationFilename is not None:
            from storage import ProcessParameters
            params = ProcessParameters(trackingConfigurationFilename)
            self.intrinsicCameraMatrix = params.intrinsicCameraMatrix
            self.distortionCoefficients = params.distortionCoefficients
            self.undistortedImageMultiplication = params.undistortedImageMultiplication
        elif intrinsicCameraFilename is not None:
            self.intrinsicCameraMatrix = loadtxt(intrinsicCameraFilename)
            self.distortionCoefficients = distortionCoefficients
            self.undistortedImageMultiplication = undistortedImageMultiplication
        else:
            self.intrinsicCameraMatrix = intrinsicCameraMatrix
            self.distortionCoefficients = distortionCoefficients
            self.undistortedImageMultiplication = undistortedImageMultiplication
            
        if self.intrinsicCameraMatrix is not None:
            self.intrinsicCameraMatrixStr = '{}'.format(self.intrinsicCameraMatrix.tolist())
        if self.distortionCoefficients is not None and len(self.distortionCoefficients) == 5:
            self.distortionCoefficientsStr = '{}'.format(self.distortionCoefficients)

    @orm.reconstructor
    def initOnLoad(self):
        if self.intrinsicCameraMatrixStr is not None:
            from ast import literal_eval
            self.intrinsicCameraMatrix = array(literal_eval(self.intrinsicCameraMatrixStr))
        else:
            self.intrinsicCameraMatrix = None
        if self.distortionCoefficientsStr is not None:
            self.distortionCoefficients = literal_eval(self.distortionCoefficientsStr)
        else:
            self.distortionCoefficients = None

    def computeUndistortMaps(self):
        if self.undistortedImageMultiplication is not None and self.intrinsicCameraMatrix is not None and self.distortionCoefficients is not None:
            self.map1, self.map2 = computeUndistortMaps(self.resX, self.resY, self.undistortedImageMultiplication, self.intrinsicCameraMatrix, self.distortionCoefficients)
        else:
            self.map1 = None
            self.map2 = None

    @staticmethod
    def getCameraType(session, cameraTypeId, resX = None):
        'Returns the site(s) matching the index or the name'
        if str.isdigit(cameraTypeId):
            return session.query(CameraType).filter(CameraType.idx == int(cameraTypeId)).all()
        else:
            if resX is not None:
                return session.query(CameraType).filter(CameraType.name.like('%'+cameraTypeId+'%')).filter(CameraType.resX == resX).all()
            else:
                return session.query(CameraType).filter(CameraType.name.like('%'+cameraTypeId+'%')).all()

# class SiteDescription(Base): # list of lines and polygons describing the site, eg for sidewalks, center lines
            
class CameraView(Base):
    __tablename__ = 'camera_views'
    idx = Column(Integer, primary_key=True)
    description = Column(String)
    homographyFilename = Column(String) # path to homograph file, relative to the site name
    siteIdx = Column(Integer, ForeignKey('sites.idx'))
    cameraTypeIdx = Column(Integer, ForeignKey('camera_types.idx'))
    trackingConfigurationFilename = Column(String) # path to configuration .cfg file, relative to site name
    maskFilename = Column(String) # path to mask file, relative to site name
    virtual = Column(Boolean) # indicates it is not a real camera view, eg merged
    
    site = relationship("Site", backref=backref('sites', order_by = idx))
    cameraType = relationship('CameraType', backref=backref('camera_types', order_by = idx))

    def __init__(self, description, homographyFilename, site, cameraType, trackingConfigurationFilename, maskFilename, virtual = False):
        self.description = description
        self.homographyFilename = homographyFilename
        self.site = site
        self.cameraType = cameraType
        self.trackingConfigurationFilename = trackingConfigurationFilename
        self.maskFilename = maskFilename
        self.virtual = virtual

    def getHomographyFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.site.getPath(), self.homographyFilename)
        else:
            return self.homographyFilename

    def getTrackingConfigurationFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.site.getPath(), self.trackingConfigurationFilename)
        else:
            return self.trackingConfigurationFilename

    def getMaskFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.site.getPath(), self.maskFilename)
        else:
            return self.maskFilename

    def getTrackingParameters(self):
        return ProcessParameters(self.getTrackingConfigurationFilename())

    def getHomographyDistanceUnit(self):
        return self.site.worldDistanceUnit
    
# class Alignment(Base):
#     __tablename__ = 'alignments'
#     idx = Column(Integer, primary_key=True)
#     cameraViewIdx = Column(Integer, ForeignKey('camera_views.idx')) # should be sites??
    
#     cameraView = relationship("CameraView", backref=backref('alignments', order_by = idx))

#     def __init__(self, cameraView):
#         self.cameraView = cameraView

# class Point(Base):
#     __tablename__ = 'points'
#     alignmentIdx = Column(Integer, ForeignKey('alignments.idx'), primary_key=True)
#     index = Column(Integer, primary_key=True) # order of points in this alignment
#     x = Column(Float)
#     y = Column(Float)

#     alignment = relationship("Alignment", backref=backref('alignments', order_by = index))
    
#     def __init__(self, alignmentIdx, index, x, y):
#         self.alignmentIdx = alignmentIdx
#         self.index = index
#         self.x = x
#         self.y = y

class VideoSequence(Base):
    __tablename__ = 'video_sequences'
    idx = Column(Integer, primary_key=True)
    name = Column(String) # path to the video file relative to the the site name
    startTime = Column(DateTime)
    duration = Column(Interval) # video sequence duration
    databaseFilename = Column(String) # path to the database file relative to the the site name
    virtual = Column(Boolean) # indicates it is not a real video sequence (no video file), eg merged
    cameraViewIdx = Column(Integer, ForeignKey('camera_views.idx'))

    cameraView = relationship("CameraView", backref=backref('camera_views', order_by = idx))

    def __init__(self, name, startTime, duration, cameraView, databaseFilename = None, virtual = False):
        '''startTime is passed as string in utils.datetimeFormat, eg 2011-06-22 10:00:39
        duration is a timedelta object'''
        self.name = name
        if isinstance(startTime, str):
            self.startTime = datetime.strptime(startTime, datetimeFormat)
        else:
            self.startTime = startTime
        self.duration = duration
        self.cameraView = cameraView
        if databaseFilename is None and len(self.name) > 0:
            self.databaseFilename = removeExtension(self.name)+'.sqlite'
        else:
            self.databaseFilename = databaseFilename
        self.virtual = virtual

    def getVideoSequenceFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.cameraView.site.getPath(), self.name)
        else:
            return self.name

    def getDatabaseFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.cameraView.site.getPath(), self.databaseFilename)
        else:
            return self.databaseFilename

    def getTimeInterval(self):
        return TimeInterval(self.startTime, self.startTime+self.duration)
        
    def containsInstant(self, instant):
        'instant is a datetime'
        return self.startTime <= instant and self.startTime+self.duration

    def intersection(self, startTime, endTime):
        'returns the moving.TimeInterval intersection with [startTime, endTime]'
        return TimeInterval.intersection(self.getTimeInterval(), TimeInterval(startTime, endTime)) 
        
    def getFrameNum(self, instant):
        'Warning, there is no check of correct time units'
        if self.containsInstant(instant):
            return int(floor((instant-self.startTime).seconds*self.cameraView.cameraType.frameRate))
        else:
            return None

class TrackingAnnotation(Base):
    __tablename__ = 'tracking_annotations'
    idx = Column(Integer, primary_key=True)
    description = Column(String) # description
    groundTruthFilename = Column(String)
    firstFrameNum = Column(Integer) # first frame num of annotated data (could be computed on less data)
    lastFrameNum = Column(Integer)
    videoSequenceIdx = Column(Integer, ForeignKey('video_sequences.idx'))
    maskFilename = Column(String) # path to mask file (can be different from camera view, for annotations), relative to site name
    undistorted = Column(Boolean) # indicates whether the annotations were done in undistorted video space

    videoSequence = relationship("VideoSequence", backref=backref('video_sequences', order_by = idx))
    
    def __init__(self, description, groundTruthFilename, firstFrameNum, lastFrameNum, videoSequence, maskFilename, undistorted = True):
        self.description = description
        self.groundTruthFilename = groundTruthFilename
        self.firstFrameNum = firstFrameNum
        self.lastFrameNum = lastFrameNum
        self.videoSequence = videoSequence
        self.undistorted = undistorted
        self.maskFilename = maskFilename

    def getGroundTruthFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.videoSequence.cameraView.site.getPath(), self.groundTruthFilename)
        else:
            return self.groundTruthFilename

    def getMaskFilename(self, relativeToSiteFilename = True):
        if relativeToSiteFilename:
            return path.join(self.videoSequence.cameraView.site.getPath(), self.maskFilename)
        else:
            return self.maskFilename

    def getTimeInterval(self):
        return TimeInterval(self.firstFrameNum, self.lastFrameNum)
        
# add class for Analysis: foreign key VideoSequenceId, dataFilename, configFilename (get the one from camera view by default), mask? (no, can be referenced in the tracking cfg file)

# class Analysis(Base): # parameters necessary for processing the data: free form
# eg bounding box depends on camera view, tracking configuration depends on camera view 
# results: sqlite

def createDatabase(filename):
    'creates a session to query the filename'
    engine = create_engine('sqlite:///'+filename)
    Base.metadata.create_all(engine)
    Session = sessionmaker(bind=engine)
    return Session()

def connectDatabase(filename):
    'creates a session to query the filename'
    engine = create_engine('sqlite:///'+filename)
    Session = sessionmaker(bind=engine)
    return Session()

def initializeSites(session, directoryName):
    '''Initializes default site objects and Camera Views
    
    eg somedirectory/montreal/ contains intersection1, intersection2, etc.
    The site names would be somedirectory/montreal/intersection1, somedirectory/montreal/intersection2, etc.'''
    from os import listdir, path
    sites = []
    cameraViews = []
    names = listdir(directoryName)
    for name in names:
        if path.isdir(directoryName+'/'+name):
            sites.append(Site(directoryName+'/'+name, None))
            cameraViews.append(CameraView(-1, None, None, sites[-1], None))
    session.add_all(sites)
    session.add_all(cameraViews)
    session.commit()
# TODO crawler for video files?
