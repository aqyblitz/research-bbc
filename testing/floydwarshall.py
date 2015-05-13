import sys
import random
import math
import copy

def gengraph(size, int_only, num_range, inf_val, edge_prob, undirected):
  graph = []
  for i in range(size):
    row = []
    for j in range(size):
      if i == j:
        val = 0
      else:
        val = inf_val
        if random.random() < edge_prob:
          val = random.random()*num_range
          if int_only:
            val = val + 1
        if int_only and val != float("inf"):
          val = int(math.floor(val))
        if undirected and j < i:
          val = graph[j][i]
      row.append(val)
    graph.append(row)
  return graph

def convertInf(graph, inf_val):
  copy = []
  for i in range(len(graph)):
    row = []
    for j in range(len(graph)):
      if graph[i][j] == float(inf_val):
        row.append(0)
      else:
        row.append(graph[i][j])
    copy.append(row)
  return copy

def writegraph(graph, filename):
  f = open(filename, 'w')
  for row in graph:
    f.write(" ".join(map(str, row))) 
    f.write("\n")

def floydwarshall(graph, inf, int_only):
  dist = []
  print('Generating distance graph...')
  r = [float("inf")]*len(graph)
  for i in range(len(graph)):
    print('Generating row '+str(i))
    dist.append(copy.copy(r))
  for v in range(len(graph)):
    dist[v][v] = 0
  l = str(len(graph))
  for u in range(len(graph)):
    print('Updating distance values in row '+str(u))
    for v in range(len(graph)):
      if v != u and graph[u][v] < inf:
        dist[u][v] = graph[u][v]
  for k in range(len(graph)):
    print('k='+str(k))
    for i in range(len(graph)):
      for j in range(len(graph)):
        if dist[i][j] > dist[i][k] + dist[k][j]:
          dist[i][j] = dist [i][k] + dist[k][j]
  if int_only:
    for i in range(len(dist)):
      for j in range(len(dist)):
        if dist[i][j] != float("inf"):
          dist[i][j] = int(dist[i][j])
  return dist

def readgraph(filename):
  f = open(filename, 'r')
  s = f.readlines()
  graph = []
  for i in range(len(s)):
    print('Reading line '+str(i))
    row = s[i].split(' ')
    row = map(float, row)
    graph.append(row)
  return graph  

def main(argv):
  gen, filename, size, int_only, num_range, inf_val, edge_prob, undirected = argv
  if(int(gen)):
    graph = gengraph(int(size), int(int_only), int(num_range), float(inf_val), float(edge_prob), int(undirected))
    writegraph(graph, filename+".in")
    writegraph(convertInf(graph, inf_val), filename+".test")
  else:
    print('Reading graph...')
    graph = readgraph(filename+".in")
  print('Running Floyd-Warshall algorithm...')
  solution = floydwarshall(graph, float(inf_val), int(int_only))
  writegraph(solution, filename+".out")
  writegraph(convertInf(solution, inf_val), filename+".diff")

if __name__ == "__main__":
   main(sys.argv[1:])
