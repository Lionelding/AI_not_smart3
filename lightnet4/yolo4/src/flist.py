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
from matplotlib.ticker import FuncFormatter
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


    fig, (ax00, ax01, ax10, ax11) = plt.subplots(ncols=4, figsize=(8, 4))
    
    inString=inString.split(" ")
    listFloat=map(float, inString)
    #listFloat = [int(i) for i in listFloat]
    #print "\nNumber of Elements: "+str(len(listFloat))

    ## Fit data to the Gaussian Mixture Model
    meanlist,covarlist=gaussianMixtureModel(listFloat, "GaussianMixture", 1, 'kmeans')



    ## ax00: Plot the Histogram
    
    ax00.hist(listFloat, bins='auto', facecolor='g', histtype='bar')
    ax00.set_title("Histogram")
    ax00.grid(True)

    ## ax01: Plot the Normalized Histogram
    weights=np.ones_like(listFloat)/len(listFloat)
    n, bins, patches =ax01.hist(listFloat, weights=weights, normed=1, facecolor='g', histtype='stepfilled')
    ax01.set_title("Normalized Histogram")

    plotGaussian(ax01, len(listFloat), meanlist, covarlist)
    # mu = meanlist[0][0]
    # variance = covarlist[0][0]
    # print mu
    # print variance
    # sigma = math.sqrt(variance)
    # x = np.linspace(mu-3*variance,mu+3*variance, len(listFloat))
    # ax01.plot(x, mlab.normpdf(x, mu, sigma), 'r', label='norm pdf', alpha=0.9)
    

    # mu = meanlist[0][1]
    # variance = covarlist[0][1]
    # print mu
    # print variance
    # sigma = math.sqrt(variance)
    # x = np.linspace(mu-3*variance,mu+3*variance, len(listFloat))
    # ax01.plot(x, mlab.normpdf(x, mu, sigma), 'r', label='norm pdf', alpha=0.9)
    # #ax01.grid(True)

    # mu = meanlist[0][2]
    # variance = covarlist[0][2]
    # print mu
    # print variance
    # sigma = math.sqrt(variance)
    # x = np.linspace(mu-3*variance,mu+3*variance, len(listFloat))
    # ax01.plot(x, mlab.normpdf(x, mu, sigma), 'r', label='norm pdf', alpha=0.9)
    # ax01.grid(True)


    ## Perform Maximum Clipping
    listFloat=maximumClipping(listFloat, 0);


    ## Fit the modified data to Gaussian Mixture Model
    modelName="GaussianMixture"
    modelComponent=2
    initial='random'
    meanlist,covarlist=gaussianMixtureModel(listFloat, modelName, modelComponent, initial)

    
    ## ax10: Plot the Histogram after maximum clipping
    ax10.hist(listFloat, bins='auto', facecolor='g', histtype='bar')
    ax10.set_title("Maximum Clipping")
    ax10.grid(True)
    
    
    ## ax11: Plot the Normalized Histogram after maximum clipping
    weights=np.ones_like(listFloat)/len(listFloat)
    #n, bins, patches =ax11.hist(listFloat, weights=weights, normed=1, facecolor='g', histtype='stepfilled')
    n, bins, patches =ax11.hist(listFloat, bins='auto', normed=1, facecolor='g', histtype='stepfilled')
    ax11.set_title("Normalized & Maximum Clipping")

    plotGaussian(ax11, len(listFloat), meanlist, covarlist)


    ## Show the Graph
    plt.suptitle(str(modelName)+" Model "+" Component Number: "+str(modelComponent))
    plt.show()

    return 1

def plotGaussian(axx, bins, meanlist, covarlist):

    for i in range(len(meanlist[0])):    
        mu = meanlist[0][i]
        variance = covarlist[0][i]
        sigma = math.sqrt(variance)
        x = np.linspace(mu-3*variance,mu+3*variance, bins)
        axx.plot(x, mlab.normpdf(x, mu, sigma), 'r', alpha=0.9)
    axx.grid(True)

    return 

def maximumClipping(listFloat, diff):

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
    max3=heapq.nlargest(3, dictionary, key=dictionary.get)
    oldMax0=dictionary[max3[0]]
    oldMax1=dictionary[max3[1]]
    oldMax2=dictionary[max3[2]]

    print "\nLargest Three Elements with Values: "
    print str(max3[0])+": "+str(oldMax0)+", "+str(max3[1])+": "+str(oldMax1)+", "+str(max3[2])+": "+str(oldMax2)

    if (max3[0]==0 and (dictionary[max3[0]]-dictionary[max3[1]])>diff):
        dictionary[max3[0]]=dictionary[max3[1]]+diff
        newMax0=dictionary[max3[0]]
    
    print "\nClipped Largest Three Elements with Values: "
    print str(max3[0])+": "+str(newMax0)+", "+str(max3[1])+": "+str(oldMax1)+", "+str(max3[2])+": "+str(oldMax2)

    ## Find the starting point of 0
    ii=0
    while (listFloat[ii]!=0):
        ii=ii+1

    del listFloat[(ii+newMax0):(oldMax0+ii)]  
    print "\nElement number: "+str(len(listFloat))
    #print listFloat
    return listFloat



def gaussianMixtureModel(inList, modelName, modelComponent, initial):

    arrayFloat=np.asarray(inList)
     
    X=arrayFloat.reshape(-1,1)
    meanlist=[]
    covarlist=[]

    if(modelName=="GaussianMixture"):

        print ("Shape of Input: "+str(arrayFloat.shape))
        gmm = mixture.GaussianMixture(n_components=modelComponent, init_params=initial, covariance_type='full', max_iter=100).fit(X)
        print "Means:"
        print "\t"+str([float(x) for x in gmm.means_])
        print "Covariance: "
        print "\t"+str([float(x) for x in gmm.covariances_])
        print "Converged: "
        print "\t"+str(gmm.converged_)
        print "Iterations: "
        print "\t"+str(gmm.n_iter_)
        print ""
        temp=gmm

    if(modelName=="BayesianGM"):
        dpgmm1 = mixture.BayesianGaussianMixture(
        n_components=modelComponent, covariance_type='full', weight_concentration_prior=1e-2,
        weight_concentration_prior_type='dirichlet_process',
        mean_precision_prior=1e-2, covariance_prior=1e0 * np.eye(1),
        init_params="random", max_iter=100, random_state=2).fit(X)
        print "Means:"
        print "\t"+str([float(x) for x in dpgmm1.means_])
        print "Covariance: "
        print "\t"+str([float(x) for x in dpgmm1.covariances_])
        print "Converged: "
        print "\t"+str(dpgmm1.converged_)
        print "Iterations: "
        print "\t"+str(dpgmm1.n_iter_)
        print ""
        temp=dpgmm1


    if(modelName=="BayesianGM2"):
        dpgmm2 = mixture.BayesianGaussianMixture(
        n_components=modelComponent, covariance_type='full', weight_concentration_prior=1e+2,
        weight_concentration_prior_type='dirichlet_process',
        mean_precision_prior=1e-2, covariance_prior=1e0 * np.eye(1),
        init_params="kmeans", max_iter=100, random_state=2).fit(X)
        print "Means:"
        print "\t"+str([float(x) for x in dpgmm2.means_])
        print "Covariance: "
        print "\t"+str([float(x) for x in dpgmm2.covariances_])
        print "Converged: "
        print "\t"+str(dpgmm2.converged_)
        print "Iterations: "
        print "\t"+str(dpgmm2.n_iter_)
        print ""
        temp=dpgmm2

    
    meanlist.append([float(x) for x in temp.means_])
    covarlist.append([float(x) for x in temp.covariances_])
    return meanlist, covarlist 

    #return 1

def testGaussianMixture():
    mu, sigma = 0, 0.3 # mean and standard deviation
    s = np.random.normal(mu, sigma, 1000)
    s=s.reshape(-1,1)
    print np.shape(s)

    gmm = mixture.GaussianMixture(n_components=1, covariance_type='full', init_params='random', max_iter=100).fit(s)
    print "Means:"
    print "\t"+str([float(x) for x in gmm.means_])
    print "Covariance: "
    print "\t"+str([float(x) for x in gmm.covariances_])
    print "Converged: "
    print "\t"+str(gmm.converged_)
    print "Iterations: "
    print "\t"+str(gmm.n_iter_)
    print ""

    count, bins, ignored = plt.hist(s, 30, normed=True)
    p1, = plt.plot(bins, 1/(sigma * np.sqrt(2 * np.pi)) *np.exp( - (bins - mu)**2 / (2 * sigma**2) ),linewidth=2, color='r')
    
    mu = gmm.means_[0][0]
    variance = gmm.covariances_[0][0][0]
    sigma = math.sqrt(variance)
    p2, = plt.plot(bins, 1/(sigma * np.sqrt(2 * np.pi)) *np.exp( - (bins - mu)**2 / (2 * sigma**2) ),linewidth=2, color='k')

    # mu = gmm.means_[1][0]
    # print gmm.covariances_
    # variance = gmm.covariances_[1][0][0]
    # sigma = math.sqrt(variance)
    # p2, = plt.plot(bins, 1/(sigma * np.sqrt(2 * np.pi)) *np.exp( - (bins - mu)**2 / (2 * sigma**2) ),linewidth=2, color='k')

    l2 = plt.legend([p1, p2], ["Ground Truth", "Predicted"], fontsize=10, loc=1)
    plt.title('Test of Gaussian Model Fitting with Random Initialization')
    plt.grid(True)
    plt.show()


xString="0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.01 0.01 0.01 0.01 0.01 0.01 0.02 0.02 0.05 0.05 0.08 0.11 0.11 0.13 0.13 0.14 0.15 0.17 0.17 0.17 0.18 0.18 0.21 0.21 0.21 0.21 0.21 0.21 0.23 0.23 0.23 0.23 0.23 0.23 0.23 0.23 0.24 0.24 0.24 0.24 0.24 0.24 0.25 2.20 2.21 2.22 2.22 2.22 2.23 2.23 2.23 2.24 2.24 2.24 2.24 2.24 3.17 3.18 3.19 3.19 3.19 4.13 4.13 4.23 5.10 5.21 7.15 8.19 9.06 9.06 10.21 11.05 11.05 11.15 11.15 12.09 12.13 14.04 15.07 15.11 15.14 17.16 18.06 18.15 19.14 21.10 21.16 23.09 23.15 24.12 24.12 26.02 26.04 26.06 26.08 26.17 28.12 29.08 30.05 30.11 30.11 33.03 36.05 37.11 45.01 45.07 48.10 48.13 50.07 51.06 60.08 63.02 63.06 63.06 68.10 71.03 71.06 71.09 81.07 90.01 90.03 90.03 90.04 98.08 105.12 135.07 144.05 150.05 161.11 162.09 171.06 180.01 198.09 215.08 215.08 225.01 225.01 225.01 225.01 225.01 225.01 229.17 231.06 231.06 243.02 245.12 251.03 251.03 251.03 270.01 270.01 270.01 270.02 270.02 285.04 292.10 315.04 334.02 334.04 339.05 339.13 340.08 340.17 342.03 342.12 344.10 344.17 346.04 348.13 348.13 349.05 349.05 349.05 350.11 350.16 351.18 352.07 352.13 353.08 353.16 354.09 355.10 355.10 355.21 355.21 356.12 356.12 356.24 356.24 356.24 357.15 357.16 358.23 358.24 358.24 358.24 358.24 358.24"
yString="-12.00 -11.00 -10.00 -10.00 -10.00 -9.00 -8.00 -8.00 -8.00 -8.00 -8.00 -8.00 -7.00 -7.00 -7.00 -7.00 -6.00 -6.00 -6.00 -6.00 -6.00 -6.00 -5.00 -5.00 -5.00 -5.00 -5.00 -4.00 -4.00 -4.00 -4.00 -4.00 -4.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -3.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -2.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 -1.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 3.00 3.00 3.00 3.00 3.00 4.00 5.00 7.00"
zString="1.00 2.00 3.00 2.00 3.00 3.00 4.00 5.00 4.00"
zzString="1 2 3 2 3 3 4 5 4"

showHistogram(yString)
#testGaussianMixture()
