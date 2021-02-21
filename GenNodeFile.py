import commands 
import os
import sys

def fetch_idle_node_list():
    status,output = commands.getstatusoutput("yhi --states=idle")
    start=output.find('[')
    end=output.find(']') 
    tmp = (output[start+1:end]).split(',')
    #print(tmp)
    IdleNodeVec=[]
    for nodestr in tmp:
        t = nodestr.find('-')
        if t==-1:
            IdleNodeVec.append(int(nodestr))
        else:
            start=int(nodestr[:t])
            end  =int(nodestr[t+1:])
            for nodeid in range(start,end+1):
                IdleNodeVec.append(nodeid)
    return IdleNodeVec
def main(argv):
	N = int(argv[1])
	Contiguous = int(argv[2])
	IdleLists = fetch_idle_node_list()
	step = len(IdleLists)/N
	fileN = "nodefile.txt"
	f = open(fileN,'w')
	restr = ""
	i = 0
	while i < N:
		step = min(Contiguous,N-i)
		for j in range(0,step):
			if i+j != 0:
				restr += (",cn"+str(IdleLists[(i*step+j)]))
			else:
				restr += ("cn"+str(IdleLists[(i*step+j)]))
		i+=step
	f.write(restr)
	f.close()
 
if __name__ == '__main__':
	main(sys.argv)