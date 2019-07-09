#
# Tool to predict webpage loading time.
# The edge device with shortest loading time will be selected
# The prediction is achieved by polynomial regression
#

import numpy as np
import sys, csv
from sklearn.preprocessing import polynomialFeatures
from sklearn.linear_model import LinearRegression

g_poly = PolynomialFeatures(degree=2)
g_clf = LinearRegression()

def CSV2Array(filename):  
    file_ = open(filename, "r")
    reader = csv.reader(file_, delimiter=",", quoting=csv.QUOTE_NONNUMERIC) #store as numeric data

    dataSet = []
    lastCol = []
    
    file_.readline() # skip first row

    for row in reader:
        dataSet.append(row[:-1]) # last column = loading time
        lastCol.append(row[-1]) # append only last column
    file_.close()
    return dataSet, lastCol

def polynomialRegression(dataset, answerSet):
    dataset_ = g_poly.fit_transform(dataset)
    dataset_ = np.delete(dataset_,(1),axis=1)
    g_clf.fit(dataset_, answerSet)
    coef = g_clf.coef_
    print ["{:0.2f}".format(x) for x in coef]
    #print ("Print Coef: ", g_clf.coef_)

def predictData(predictData):
    predictData_ = g_poly.fit_transform(predictData)
    predictData_ = np.delete(predictData_,(1),axis=1)
    #g_clf.predict(predictData_)
    print("Predict Val: ", g_clf.predict(predictData_))

def _main():
    dataSet, loadingTime = CSV2Array(sys.argv[1]); #"evalData.csv";
    print("DataSet: ", dataSet)
    print("Loading Time: ", loadingTime)

    polynomialRegression(dataSet, loadingTime)

    #TEST CODE!!
    #predict = [[20,21,22,23,24,25]]
    #predictData(predict)

if __name__ == '__main__':
    if (len(sys.argv) < 2):
        print("Usage: python " + __file__ + " [evalData.csv] ")
    else:
        _main()
