import sys
import numpy as np
import matplotlib.pyplot as plt
from sklearn import mixture
from scipy import linalg
import matplotlib.pyplot as plt
import matplotlib as mpl
import itertools

import matplotlib.mlab as mlab
import math

import heapq
#from collections import Counter

sys.path.insert(0, "/home/liqiang/AI_not_smart3/lightnet4/yolo4/src")



color_iter = itertools.cycle(['navy', 'c', 'cornflowerblue', 'gold',
                              'darkorange'])

def multiply(a):
    print "Will compute", a, "times", a
    c=a*a
    return c


def showHistogram(inString):
    ## Initialization
    meanVar=[]
    fig, (ax00, ax01, ax10, ax11) = plt.subplots(ncols=4, figsize=(8, 4))
    
    inString=inString.split(" ")
    listFloat=map(float, inString)
    print "\nElement number: "+str(len(listFloat))
    #print listFloat

    [mean, var]=gaussiamMixtureModel(listFloat)
    meanVar.append([mean, var])

    ax00.hist(listFloat, bins=len(listFloat), normed=1, histtype='bar', rwidth=2)
    ax00.set_title("Histogram without Maximum Clipping")

    ## Create the an Hashmap to store the occurance frequency of each key
    dictionary={}
    for i in range(0, len(listFloat)):
        key=listFloat[i]
        if key in dictionary:
            freq=dictionary[key]+1
            dictionary[key]=freq
        else:
            dictionary[key]=1


    ## If the maximum value of key AA is larger than key BB by 10 or more, 
    ## then we assign the value of key AA with value in BB + 10
    diff=10
    max3=heapq.nlargest(3, dictionary, key=dictionary.get)
    oldMax0=dictionary[max3[0]]
    oldMax1=dictionary[max3[1]]
    oldMax2=dictionary[max3[2]]

    print "\nLargest Three Elements with Values: "
    print str(max3[0])+": "+str(oldMax0)+", "+str(max3[1])+": "+str(oldMax1)+", "+str(max3[2])+": "+str(oldMax2)

    if (max3[0]==0 and (dictionary[max3[0]]-dictionary[max3[1]])>diff):
        dictionary[max3[0]]=dictionary[max3[1]]+diff
        newMax0=dictionary[max3[0]]
    
    print "\nNormalized Largest Three Elements with Values: "
    print str(max3[0])+": "+str(newMax0)+", "+str(max3[1])+": "+str(oldMax1)+", "+str(max3[2])+": "+str(oldMax2)

    ## Find the starting point of 0
    ii=0
    while (listFloat[ii]!=0):
        ii=ii+1

    del listFloat[(ii+newMax0):(oldMax0+ii)]  
    print "\nElement number: "+str(len(listFloat))
    #print listFloat



    
    [mean, var]=gaussiamMixtureModel(listFloat)
    meanVar.append([mean, var])

    ax10.hist(listFloat, bins=len(listFloat), normed=1, histtype='bar', rwidth=2)
    ax10.set_title("Histogram after Maximum Clipping")
    


    #result=[int(x) for x in listFloat]


        
    mu = mean
    variance = var
    sigma = math.sqrt(variance)
    x = np.linspace(mu-3*variance,mu+3*variance, 100)
    ax01.plot(x,mlab.normpdf(x, mu, sigma))

    mu = mean+50
    variance = var+0.5
    sigma = math.sqrt(variance)
    x = np.linspace(mu-3*variance,mu+3*variance, 100)
    ax11.plot(x,mlab.normpdf(x, mu, sigma))

    # plt.hist(result, bins=len(result))  # arguments are passed to np.histogram
    # plt.title("Histogram with 'auto' bins")
    plt.show()

    return 1


def gaussiamMixtureModel(inList):

    arrayFloat=np.asarray(inList)
     
    X=arrayFloat.reshape(-1,1)
    modelNum=1

     
    print ("shape of input: "+str(arrayFloat.shape))
    gmm = mixture.GaussianMixture(n_components=modelNum, covariance_type='full', max_iter=100).fit(X)
    print [x for x in gmm.means_]
    print gmm.covariances_
    print "converged: "+str(gmm.converged_)
    print "iteration: "+str(gmm.n_iter_)
    print ""

    mean=gmm.means_[0][0]
    convar=gmm.covariances_[0][0][0]
    return mean, convar 

    # dpgmm1 = mixture.BayesianGaussianMixture(
    # n_components=modelNum, covariance_type='full', weight_concentration_prior=1e-2,
    # weight_concentration_prior_type='dirichlet_process',
    # mean_precision_prior=1e-2, covariance_prior=1e0 * np.eye(1),
    # init_params="random", max_iter=100, random_state=2).fit(X)
    # print [x for x in dpgmm1.means_]
    # #print dpgmm1.covariances_
    # print "converged: "+str(dpgmm1.converged_)
    # print "iteration: "+str(dpgmm1.n_iter_)
    # print ""

    # dpgmm2 = mixture.BayesianGaussianMixture(
    # n_components=modelNum, covariance_type='full', weight_concentration_prior=1e+2,
    # weight_concentration_prior_type='dirichlet_process',
    # mean_precision_prior=1e-2, covariance_prior=1e0 * np.eye(1),
    # init_params="kmeans", max_iter=100, random_state=2).fit(X)
    # print [x for x in dpgmm2.means_]
    # #print dpgmm2.covariances_
    # print "converged: "+str(dpgmm2.converged_)
    # print "iteration: "+str(dpgmm2.n_iter_)
    # print ""
    


    #return 1



xString="0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.01 0.01 0.01 0.01 0.01 0.01 0.02 0.02 0.05 0.05 0.08 0.11 0.11 0.13 0.13 0.14 0.15 0.17 0.17 0.17 0.18 0.18 0.21 0.21 0.21 0.21 0.21 0.21 0.23 0.23 0.23 0.23 0.23 0.23 0.23 0.23 0.24 0.24 0.24 0.24 0.24 0.24 0.25 2.20 2.21 2.22 2.22 2.22 2.23 2.23 2.23 2.24 2.24 2.24 2.24 2.24 3.17 3.18 3.19 3.19 3.19 4.13 4.13 4.23 5.10 5.21 7.15 8.19 9.06 9.06 10.21 11.05 11.05 11.15 11.15 12.09 12.13 14.04 15.07 15.11 15.14 17.16 18.06 18.15 19.14 21.10 21.16 23.09 23.15 24.12 24.12 26.02 26.04 26.06 26.08 26.17 28.12 29.08 30.05 30.11 30.11 33.03 36.05 37.11 45.01 45.07 48.10 48.13 50.07 51.06 60.08 63.02 63.06 63.06 68.10 71.03 71.06 71.09 81.07 90.01 90.03 90.03 90.04 98.08 105.12 135.07 144.05 150.05 161.11 162.09 171.06 180.01 198.09 215.08 215.08 225.01 225.01 225.01 225.01 225.01 225.01 229.17 231.06 231.06 243.02 245.12 251.03 251.03 251.03 270.01 270.01 270.01 270.02 270.02 285.04 292.10 315.04 334.02 334.04 339.05 339.13 340.08 340.17 342.03 342.12 344.10 344.17 346.04 348.13 348.13 349.05 349.05 349.05 350.11 350.16 351.18 352.07 352.13 353.08 353.16 354.09 355.10 355.10 355.21 355.21 356.12 356.12 356.24 356.24 356.24 357.15 357.16 358.23 358.24 358.24 358.24 358.24 358.24"
yString="-12.00 -11.00 -10.00 -10.00 -10.00 -9.00 -8.00 -8.00 -8.00 -8.00 -8.00 -8.00 -7.00 -7.00 -7.00 -7.00 -6.00 -6.00 -6.00 -6.00 -6.00 -6.00 -5.00 -5.00 -5.00 -5.00 -5.00 -4.00 -4.00 -4.00 -4.00 -4.00 -4.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 3.00 3.00 3.00 3.00 3.00 4.00 5.00 7.00"
zString="1.00 2.00 3.00 2.00 3.00 3.00 4.00 5.00 4.00"
zzString="1 2 3 2 3 3 4 5 4"

showHistogram(yString)
