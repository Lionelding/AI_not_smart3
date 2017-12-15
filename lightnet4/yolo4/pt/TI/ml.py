#! /usr/bin/env python
'''Libraries for machine learning algorithms'''
from __future__ import print_function

from os import path
from random import shuffle
from copy import copy, deepcopy

import numpy as np
from matplotlib.pylab import text
import matplotlib as mpl
import matplotlib.pyplot as plt
from scipy.cluster.vq import kmeans, whiten, vq
from sklearn import mixture
import cv2

import utils

#####################
# OpenCV ML models
#####################

class StatModel(object):
    '''Abstract class for loading/saving model'''    
    def load(self, filename):
        if path.exists(filename):
            self.model.load(filename)
        else:
            print('Provided filename {} does not exist: model not loaded!'.format(filename))

    def save(self, filename):
        self.model.save(filename)

class SVM(StatModel):
    '''wrapper for OpenCV SimpleVectorMachine algorithm'''
    def __init__(self, svmType = cv2.SVM_C_SVC, kernelType = cv2.SVM_RBF, degree = 0, gamma = 1, coef0 = 0, Cvalue = 1, nu = 0, p = 0):
        self.model = cv2.SVM()
        self.params = dict(svm_type = svmType, kernel_type = kernelType, degree = degree, gamma = gamma, coef0 = coef0, Cvalue = Cvalue, nu = nu, p = p)
        # OpenCV3
        # self.model = cv2.SVM()
        # self.model.setType(svmType)
        # self.model.setKernel(kernelType)
        # self.model.setDegree(degree)
        # self.model.setGamma(gamma)
        # self.model.setCoef0(coef0)
        # self.model.setC(Cvalue)
        # self.model.setNu(nu)
        # self.model.setP(p)

    def train(self, samples, responses):
        self.model.train(samples, responses, params = self.params)

    def predict(self, hog):
        return self.model.predict(hog)


#####################
# Clustering
#####################

class Centroid(object):
    'Wrapper around instances to add a counter'

    def __init__(self, instance, nInstances = 1):
        self.instance = instance
        self.nInstances = nInstances

    # def similar(instance2):
    #     return self.instance.similar(instance2)

    def add(self, instance2):
        self.instance = self.instance.multiply(self.nInstances)+instance2
        self.nInstances += 1
        self.instance = self.instance.multiply(1/float(self.nInstances))

    def average(c):
        inst = self.instance.multiply(self.nInstances)+c.instance.multiply(instance.nInstances)
        inst.multiply(1/(self.nInstances+instance.nInstances))
        return Centroid(inst, self.nInstances+instance.nInstances)

    def plot(self, options = ''):
        self.instance.plot(options)
        text(self.instance.position.x+1, self.instance.position.y+1, str(self.nInstances))

def kMedoids(similarityMatrix, initialCentroids = None, k = None):
    '''Algorithm that clusters any dataset based on a similarity matrix
    Either the initialCentroids or k are passed'''
    pass

def assignCluster(data, similarFunc, initialCentroids = None, shuffleData = True):
    '''k-means algorithm with similarity function
    Two instances should be in the same cluster if the sameCluster function returns true for two instances. It is supposed that the average centroid of a set of instances can be computed, using the function. 
    The number of clusters will be determined accordingly

    data: list of instances
    averageCentroid: '''
    localdata = copy(data) # shallow copy to avoid modifying data
    if shuffleData:
        shuffle(localdata)
    if initialCentroids is None:
        centroids = [Centroid(localdata[0])]
    else:
        centroids = deepcopy(initialCentroids)
    for instance in localdata[1:]:
        i = 0
        while i<len(centroids) and not similarFunc(centroids[i].instance, instance):
            i += 1
        if i == len(centroids):
            centroids.append(Centroid(instance))
        else:
            centroids[i].add(instance)

    return centroids

# TODO recompute centroids for each cluster: instance that minimizes some measure to all other elements

def spectralClustering(similarityMatrix, k, iter=20):
	'''Spectral Clustering algorithm'''
	n = len(similarityMatrix)
	# create Laplacian matrix
	rowsum = np.sum(similarityMatrix,axis=0)
	D = np.diag(1 / np.sqrt(rowsum))
	I = np.identity(n)
	L = I - np.dot(D,np.dot(similarityMatrix,D))
	# compute eigenvectors of L
	U,sigma,V = np.linalg.svd(L)
	# create feature vector from k first eigenvectors
	# by stacking eigenvectors as columns
	features = np.array(V[:k]).T
	# k-means
	features = whiten(features)
	centroids,distortion = kmeans(features,k, iter)
	code,distance = vq(features,centroids) # code starting from 0 (represent first cluster) to k-1 (last cluster)
	return code,sigma	

def prototypeCluster(instances, similarities, minSimilarity, similarityFunc = None, minClusterSize = None, randomInitialization = False):
    '''Finds exemplar (prototype) instance that represent each cluster
    Returns the prototype indices (in the instances list) and the cluster label of each instance

    the elements in the instances list must have a length (method __len__), or one can use the random initialization
    the positions in the instances list corresponds to the similarities
    if similarityFunc is provided, the similarities are calculated as needed (this is faster) if not in similarities (negative if not computed)
    similarities must still be allocated with the right size

    if an instance is different enough (<minSimilarity), 
    it will become a new prototype. 
    Non-prototype instances will be assigned to an existing prototype
    if minClusterSize is not None, the clusters will be refined by removing iteratively the smallest clusters
    and reassigning all elements in the cluster until no cluster is smaller than minClusterSize

    TODO: at each step, optimize the prototype as the most similar in its current cluster (can be done easily if similarities are already computed)'''

    # sort instances based on length
    indices = range(len(instances))
    if randomInitialization:
        indices = np.random.permutation(indices)
    else:
        def compare(i, j):
            if len(instances[i]) > len(instances[j]):
                return -1
            elif len(instances[i]) == len(instances[j]):
                return 0
            else:
                return 1
        indices.sort(compare)
    # go through all instances
    prototypeIndices = [indices[0]]
    for i in indices[1:]:
        if similarityFunc is not None:
            for j in prototypeIndices:
                if similarities[i][j] < 0:
                    similarities[i][j] = similarityFunc(instances[i], instances[j])
                    similarities[j][i] = similarities[i][j]
        if similarities[i][prototypeIndices].max() < minSimilarity:
            prototypeIndices.append(i)
        elif randomInitialization: # replace prototype by current instance i if longer
            label = similarities[i][prototypeIndices].argmax()
            if len(instances[prototypeIndices[label]]) < len(instances[i]):
                prototypeIndices[label] = i

    # assignment
    indices = [i for i in range(similarities.shape[0]) if i not in prototypeIndices]
    assign = True
    while assign:
        labels = [-1]*similarities.shape[0]
        for i in prototypeIndices:
            labels[i] = i
        for i in indices:
            if similarityFunc is not None:
                for j in prototypeIndices:
                    if similarities[i][j] < 0:
                        similarities[i][j] = similarityFunc(instances[i], instances[j])
                        similarities[j][i] = similarities[i][j]
            prototypeIdx = similarities[i][prototypeIndices].argmax()
            if similarities[i][prototypeIndices[prototypeIdx]] >= minSimilarity:
                labels[i] = prototypeIndices[prototypeIdx]
            else:
                labels[i] = -1 # outlier
        clusterSizes = {i: sum(np.array(labels) == i) for i in prototypeIndices}
        smallestClusterIndex = min(clusterSizes, key = clusterSizes.get)
        assign = (clusterSizes[smallestClusterIndex] < minClusterSize)
        if assign:
            prototypeIndices.remove(smallestClusterIndex)
            indices.append(smallestClusterIndex)

    return prototypeIndices, labels

def computeClusterSizes(labels, prototypeIndices, outlierIndex = -1):
    clusterSizes = {i: sum(np.array(labels) == i) for i in prototypeIndices}
    clusterSizes['outlier'] = sum(np.array(labels) == outlierIndex)
    return clusterSizes

# Gaussian Mixture Models
def plotGMMClusters(model, dataset = None, fig = None, colors = utils.colors, nUnitsPerPixel = 1., alpha = 0.3):
    '''plot the ellipse corresponding to the Gaussians
    and the predicted classes of the instances in the dataset'''
    if fig is None:
        fig = plt.figure()
    labels = model.predict(dataset)
    tmpDataset = dataset/nUnitsPerPixel
    for i in xrange(model.n_components):
        mean = model.means_[i]/nUnitsPerPixel
        covariance = model.covars_[i]/nUnitsPerPixel
        if dataset is not None:
            plt.scatter(tmpDataset[labels == i, 0], tmpDataset[labels == i, 1], .8, color=colors[i])
        plt.annotate(str(i), xy=(mean[0]+1, mean[1]+1))

        # Plot an ellipse to show the Gaussian component                                                  
        v, w = np.linalg.eigh(covariance)
        angle = np.arctan2(w[0][1], w[0][0])
        angle = 180*angle/np.pi  # convert to degrees                                             
	v *= 4
        ell = mpl.patches.Ellipse(mean, v[0], v[1], 180+angle, color=colors[i])
        ell.set_clip_box(fig.bbox)
        ell.set_alpha(alpha)
        fig.axes[0].add_artist(ell)
    return labels
