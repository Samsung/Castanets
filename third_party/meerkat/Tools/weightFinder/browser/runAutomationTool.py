#
# Tool to run Automation testing.
# Save the result as csv

import sys, csv, socket, json
from subprocess import check_output

global g_ip
global g_port

global g_socket

g_loadingTimeExtractor = './loadingtime_extract.js'
g_availableFreq = [3200, 2500, 1800, 1100, 800]
#g_availableFreq = [3200, 3000, 2900, 2700, 2500, 2300, 2200, 2000, 1800, 1700, 1500, 1300, 1100, 1000,800]
g_numCores = 4

g_maxNetDelay = 80 # max 80 ms delay
g_NetDelayStep = 20 # 20 ms per step

g_maxPacketDropRate = 10 # maximum 10%
g_packetDropRateStep = 3 # 3% per step

g_netBandwidth = 304857600 # 300Mbps as default. Modification needed after testing

g_benchSiteFile = 'top-100.csv'
g_resultFile = 'result.csv'


def configRemoteNode(latency, packetDropRate, bigcoreFreq, littlecoreFreq, numBigCores, numLittleCores, cpuUtil): #network bandwidth, network bandwidth, cpu frequency, free memory
    data = json.dumps({"latency": latency, "dropRate": packetDropRate, "frequency_big": bigcoreFreq, "numBigCores": numBigCores, "frequency_little": littlecoreFreq, "numLittleCores": numLittleCores, "cpuUtil": cpuUtil})
    g_socket.send(data.encode())
    g_socket.recv(1024) # receive Complete msg

def loadAlexaPage(filename):
    webpages = []
    with open(filename) as csv_file:
        csv_reader = csv.reader(csv_file, delimiter=',')
        line_count = 0
        for row in csv_reader:
            webpages.append(row[1]);
        return webpages;

def calculateBandwidth(packetDropRate): #calculates bandwidth from packetDropRate
    return (g_netBandwidth * (1 - float(packetDropRate)/100))

def writeLoadingTime(url, latency, packetDropRate, bigcoreFreq, littlecoreFreq, numBigCores, numLittleCores, cpuUtil, csv_writer):
    loadingTime = check_output([r'node', g_loadingTimeExtractor, url])
    bandwidth = calculateBandwidth(packetDropRate)
    csv_writer.writerow([url, latency, bandwidth, bigcoreFreq, littlecoreFreq, numBigCores,numLittleCores, cpuUtil, str(loadingTime.split()[0])])

def _main():

    global g_socket

    g_socket = socket.socket()
    g_socket.connect((g_ip, int(g_port)))

    webpageList = loadAlexaPage(g_benchSiteFile)
    csv_writer = csv.writer(open(g_resultFile, 'w'))
    csv_writer.writerow(["URL", "latency(ms)", "bandwidth(bps)", "bigcoreFreq", "littlecoreFreq", "numBigCores","numLittleCores", "cpu Utilization", "loadingTime(ms)"])
    prefix = "http://www."
    

    for bfreq in g_availableFreq[0:]: #big core frequency
        for lfreq in g_availableFreq[1:]: #little core frequency
            if bfreq > lfreq: # big freq should be larger than little freq
               for numLittleCore in range(0, g_numCores, 1): #numCores range between 0 ~ numCore-1
                   for cpuUtil in range(0, 80, 20): # for CPU usage 0% to 80%
                       for netDelay in range(0, g_maxNetDelay, g_NetDelayStep): # for network delay 0 to (g_maxNetDelay) sec
                           for netPacketDropRate in range(0, g_maxPacketDropRate, g_packetDropRateStep): # 
                               for wp in webpageList: # for each webpage
                                   configRemoteNode(netDelay, netPacketDropRate, bfreq, lfreq, g_numCores - numLittleCore, numLittleCore, cpuUtil)                                  
                                   writeLoadingTime(prefix+wp, netDelay, netPacketDropRate, bfreq, lfreq, g_numCores - numLittleCore, numLittleCore, cpuUtil, csv_writer)

    g_socket.close()

if __name__ == "__main__":
    if (len(sys.argv) < 3):
        print("Usage: python " + __file__ + " [server_ip] [server_port]")
    else:
        global g_ip
        global g_port
        g_ip = sys.argv[1]
        g_port = sys.argv[2]

        _main()
