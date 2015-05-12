import sys
import random
import math
import copy

def writegraph(graph, filename):
  f = open(filename, 'w')
  for row in graph:
    f.write(" ".join(map(str, row))) 
    f.write("\n")

def readfile(filename):
  f = open(filename, 'r')
  s = f.readlines()
  info = map(int,s[0].split(' ')[1:])
  graph = []
  graph2 = []
  val = float('inf')
  r = [val for col in range(info[0])]
  r2 = [0 for col in range(info[0])]
  print('Generating base graph...')
  for i in range(info[0]):
    graph.append(copy.copy(r))
    graph2.append(copy.copy(r2))
  for i in range(len(graph)):
    graph[i][i] = 0
  print('Added values...')
  for i in range(info[0]+2,info[0]+2+info[1]):
    row = map(int,s[i].split(' '))
    for j in row[1:]:
      graph[row[0]-1][j-1] = 1
      graph2[row[0]-1][j-1] = 1
      graph[j-1][row[0]-1] = 1
      graph2[j-1][row[0]-1] = 1
  return graph, graph2

def main(argv):
  graph, graph2 = readfile(argv[0])
  print(graph[0])
  print('Writing files...')
  writegraph(graph, 'marvel.in')
  writegraph(graph2, 'marvel.test')

if __name__ == "__main__":
   main(sys.argv[1:])
