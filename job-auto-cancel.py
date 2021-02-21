import os
import subprocess


# yhqRe=os.popen("yhq --name=batch-alltoall.sh").readlines()
yhqRe=os.popen("yhq").readlines()
# print(yhqRe)
for yhqr in yhqRe:
    strs=(yhqr.strip().split())
    print(strs)
    for i in range(0,len(strs)-2):
        if strs[i+2] == "batch-al":
            jobid = strs[i]
            os.system("yhcancel "+jobid)



# yhqRe=os.popen("yhq --name=batch-allreduce.sh").readlines()
# # strs=(yhqRe.strip().split())
# print(yhqRe)
# # jobid = strs[8]
# # os.system("yhcancel "+jobid)