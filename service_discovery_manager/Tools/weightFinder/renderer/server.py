#
# Sets renderer configs on Ubuntu 16.04 
# cpufreq-info, cpulimit command must be avaliable to run the Automation code
# edit GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub and add 'intel_pstate=disable'
# 'sudo update-grub' must be performed after editing GRUB_CMDLINE_LINUX_DEFAULT
# and must reboot after setting new grub
# Prior to run the code, chrome renderer should be running
#
# This auto-test code is only tested on Ubuntu 16.04
# Use it at your own risk and feel free to modify the code
#

import socket, json, atexit, os, sys
from subprocess import Popen, PIPE
from subprocess import check_output

global g_port

g_availableFreq = [3200, 3000, 2900, 2700, 2500, 2300, 2200, 2000, 1800, 1700, 1500, 1300, 1100, 1000, 800] # available MHz by cpufrq-info cmd
g_procName = "chrome"
g_cgroupDir = "/dev/cgroups"
g_groupMemLimitDir = g_cgroupDir+"/limitGroup"

g_numCores = 1 # modifies at init
g_chromePIDs = []

def exit_handler():
    cmd = 'tc qdisc del dev enp4s0 root'
    os.system(cmd)
    cmd = 'killall cpulimit'
    os.system(cmd)
    return

def get_pid(name):
    g_chromePIDs =  map(int,check_output(["pidof",name]).split())
    return map(int,check_output(["pidof",name]).split())
    #print g_chromePIDs;

def setNetworkConfig(latency, packetDropRate):
    cmd = 'tc qdisc replace dev enp4s0 root handle 1:0 netem loss '+str(packetDropRate)+'% delay ' + str(latency) +'ms'
    os.system(cmd)
    return

'''
def setNetworkLatency(latency):
    print ("Set Latnecy: "+str(latency))
    cmd = 'tc qdisc replace dev enp4s0 root handle 1:0 netem delay ' + str(latency)+'ms'
    print cmd
    os.system(cmd)
    return

def setNetworkBandwidth(packetDropRate): #since only egress can be controlled and ingress cannot be controled, we control the bandwidth with packet drop rate.
    print "Set packetDropRate: " + str(packetDropRate)
    cmd = 'tc qdisc replace dev enp4s0 parent 1:0 handle 10: netem loss ' + str(packetDropRate)+'%'
    print cmd
    os.system(cmd)
    return
'''

def setCPUUtilization(utilization):
    utilization = (100 - utilization) * g_numCores # limit cpu usage (Current CPU utilization 30% = my proc can use maximum 70% of the CPU)
    cmd = 'killall cpulimit'
    os.system(cmd)
    
    for i in g_chromePIDs:
        cmd = 'cpulimit -b -l ' + str(utilization) + ' -p ' + str(i)
        #print cmd
        os.system(cmd)
    return

def setCPUFrequency(numBigCore, bigCoreFreq, numLittleCore, littleCoreFreq):
    numCores = numBigCore + numLittleCore
    for i in range(1,g_numCores): # turn on Cores
        cmd = 'echo 1 | tee /sys/devices/system/cpu/cpu' + str(i) +'/online'
        os.system(cmd)
    for i in range(numCores, g_numCores): # turn off Rest of the cores
        cmd = 'echo 0 | tee /sys/devices/system/cpu/cpu' + str(i) +'/online'
        os.system(cmd)

    for i in range(0, numBigCore):
        if (bigCoreFreq in g_availableFreq): #frequency must exist in the available list
            cmd = 'cpufreq-set -c ' +str(i)+ ' -u ' + str(bigCoreFreq)+ 'MHz'
            os.system(cmd)
    for i in range(numBigCore, numBigCore + numLittleCore):
        if (littleCoreFreq in g_availableFreq):
            cmd = 'cpufreq-set -c ' +str(i)+ ' -u ' + str(littleCoreFreq)+ 'MHz'
            os.system(cmd)
    return

def setNumCores(numCores): # turn off some cores
    if (g_numCores <= numCores):
        return
    else:
        for i in range(1,g_numCores):
            cmd = 'echo 1 | tee /sys/devices/system/cpu/cpu' + str(i) +'/online'
            os.system(cmd)
        for i in range(numCores, g_numCores): # cores to turn off            
            cmd = 'echo 0 | tee /sys/devices/system/cpu/cpu' + str(i) +'/online'
            os.system(cmd)
    return

'''
# Memory setting removed since Android supports OOMK. OOMK kills process if memory is not enough
def setMemory(memSizeMB):
    memSizeByte = memSizeMB * 1024 * 1024
    cmd = "echo "+ str(memSizeByte) + " | tee " + g_groupMemLimitDir + "/memory.limit_in_bytes"
    print cmd
    os.system(cmd);
    cmd = "echo "+ str(memSizeByte) + " | tee " + g_groupMemLimitDir + "/memory.memsw.limit_in_bytes"
    print cmd
    os.system(cmd);
    return

def addChromeToCgroupLimit():
    for i in g_chromePIDs:
        # echo <PID> > /dev/cgroups/test/tasks
        os.system("echo "+ str(i) + " | tee "+ g_groupMemLimitDir + "/tasks")
    return
'''

def initAutoConfig():
    global g_numCores
    global g_chromePIDs
    '''
    os.system("mkdir " + g_cgroupDir)
    os.system("mount -t cgroup -omemory memory "+g_cgroupDir)
    os.system("mkdir " + g_groupMemLimitDir)
    '''

    process = Popen([r'nproc','--all'], stdin=PIPE, stdout=PIPE)
    g_numCores = int(process.stdout.readline().rstrip())
    g_chromePIDs = get_pid(g_procName);

    return

def _main():

    permission = os.getuid()
    if permission != 0:  #check if the testCode is executed with root permission
        print "Please execute with root permission"
        return

    initAutoConfig()    
    #setCPUUtilization(60)
    atexit.register(exit_handler)


    s = socket.socket()         # Create a socket object
    port = g_port                # Reserve a port for your service.
    s.bind(('0.0.0.0', int(port)))        # Bind to the port
    s.listen(5)                 # Now wait for client connection.
    prev_nwDropRate = -1
    prev_nwLatency = -1
    prev_cpuFrequency_big = -1
    prev_cpuFrequency_little = -1
    prev_numBigCores = -1 
    prev_numLittleCores = -1 
    prev_freeMemMB = -1
    prev_cpuUtilization = -1
    
    c, addr = s.accept()     # Establish connection with client.

    while True:
        print 'Got connection from', addr
        data = c.recv(1024)
        data = json.loads(data.decode())

        #Network Parameters
        latency = data.get("latency") # add latency to send packet
        dropRate = data.get("dropRate") #control packet drop rate to control bandwidth

        #CPU parameters
        cpuFrequency_big = data.get("frequency_big") # Big core frequency
        cpuFrequency_little = data.get("frequency_little") # Little core frequency
        numBigCores = data.get("numBigCores")
        numLittleCores = data.get("numLittleCores")
        cpuUtilization = data.get("cpuUtil")

       
        print str(latency) + " " + str(dropRate)  + " " + str(cpuFrequency_big) + " " + str(cpuFrequency_little) + " " + str(numBigCores) + " " + str(numLittleCores) + " " + str(cpuUtilization) 
        
        if ((latency != prev_nwLatency) or (dropRate != prev_nwDropRate)): 
            setNetworkConfig(latency, dropRate)
        if ((numBigCores != prev_numBigCores) or (numLittleCores != prev_numLittleCores) or (prev_cpuFrequency_big != prev_cpuFrequency_big) or (prev_cpuFrequency_little != cpuFrequency_little)):
            setCPUFrequency(numBigCores, cpuFrequency_big, numLittleCores, cpuFrequency_little)
        if(cpuUtilization != prev_cpuUtilization):
            setCPUUtilization(cpuUtilization)
        
        #if freeMemMB != prev_freeMemMB:
        #    setMemory(freeMemMB)
        c.send("Complete")
        
        prev_nwDropRate = dropRate
        prev_nwLatency = latency

        prev_cpuFrequency_big = cpuFrequency_big
        prev_cpuFrequency_little = cpuFrequency_little
        prev_numBigCores = numBigCores
        prev_numLittleCores = numLittleCores
        prev_cpuUtilization = cpuUtilization

        #prev_freeMemMB = freeMemMB
    c.close()


if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print("Usage: python " + __file__ + " [port]")
    else:
        global g_port
        g_port = sys.argv[1]

        _main()

