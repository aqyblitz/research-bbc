import sys
import random
import math
import operator

def readfile(filename):
  f = open(filename, 'r')
  s = f.readlines()
  info = map(int,s[0].split(' ')[1:])
  h = {}
  for i in range(info[0]):
    t = s[i].split(' ',1)
    h[t[0]] = t[1][1:-2]
  return h

def readrow(filename):
  f = open(filename, 'r')
  s = f.readlines()
  r = s[0][:-1].split(' ')
  return r

def stats(h, r, j):
  print('Name: '+h[r[0]])
  d = {i: r[i] for i in range(1, len(r))}
  for k,v in d.items():
    if v == '0':
       del d[k]
  for k,v in d.items():
    if int(k) > 6486:
       del d[k]
  sorted_d = sorted(d.items(), key=operator.itemgetter(1))
  print(str(min(len(sorted_d),j))+' closest characters:')
  for i in range(min(len(sorted_d),j)):
    print(h[str(sorted_d[i][0])])
  print('\n'+str(min(len(sorted_d),j))+' furthest characters:')
  for i in reversed(range(min(len(sorted_d),j))):
    print(h[str(sorted_d[i][0])])

def main(argv):
  graphfile, rowfile, k = argv
  h = readfile(argv[0])
  r = readrow(rowfile)
  stats(h,r,int(k))

if __name__ == "__main__":
   main(sys.argv[1:])
