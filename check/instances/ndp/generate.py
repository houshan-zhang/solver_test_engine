#!/usr/bin/python

# Multi-Commodity Multi-Module Capacitated Network Design Problem
import random
import heapq
import math
import subprocess
import numpy as np
from mip.model import Model, xsum, minimize
from mip.constants import BINARY, INTEGER

def iseq(v1, v2):
    if abs(v1-v2) <= 1e-4:
        return True
    else:
        return False

class Data(object):

    def __init__(self):
        # the coordinate of node
        self.y = np.zeros(nNode, dtype=float)
        self.y = np.zeros(nNode, dtype=float)
        # the parameters in model
        self.demand = np.zeros((nNode, nNode), dtype=int)
        # distance matriy (euclidean distance, even if there no arc exist)
        self.distance = np.zeros((nNode, nNode), dtype=float)
        # the number of arcs
        self.nArc = 0
        # adjacency matriy (1 means there exist an arc)
        self.adjmat = np.zeros((nNode, nNode), dtype=int)
        # the setting degree of node
        self.degree_set = np.zeros(nNode, dtype=int)
        # the actual degree of node (may less than the setting degree)
        self.degree_act = np.zeros(nNode, dtype=int)
        # the sum of all the demands
        self.sd = 0

    # calculate the distance between two nodes
    def dist(self, i, j):
        return round(pow((self.y[i]-self.y[j])**2 + (self.y[i]-self.y[j])**2, 0.5), ds)

    # make data
    def make_data(self):
        while True:
            # generate coordinate and set degree for each node
            for i in range(nNode):
                self.y[i] = round(random.uniform(0, R), ds)
                self.y[i] = round(random.uniform(0, R), ds)
                temp = random.randint(1,10)
                if temp <= 2:
                    self.degree_set[i]= 2
                    self.degree_act[i]= 2
                elif temp <= 5:
                    self.degree_set[i]= 3
                    self.degree_act[i]= 3
                elif temp <= 8:
                    self.degree_set[i]= 4
                    self.degree_act[i]= 4
                elif temp <= 10:
                    self.degree_set[i]= 5
                    self.degree_act[i]= 5
            # calculate the euclidean distance between different nodes
            for i in range(nNode):
                for j in range(nNode):
                    self.distance[i][j] = self.dist(i, j)
            # connected each node
            for i in range(nNode):
                # sort nearlist nodes
                near = list(map(self.distance[i].tolist().index, heapq.nsmallest(nNode, self.distance[i].tolist())))
                nAdd = 0
                for jidx in range(nNode):
                    if( nAdd == self.degree_set[i] ):
                        break;
                    j = near[jidx]
                    if i != j and self.adjmat[i][j] == 0:
                        self.degree_act[j] += 1
                        self.adjmat[i][j] = 1
                        self.adjmat[j][i] = 1
                        self.nArc += 1
                        nAdd += 1
            # use BSF to check graph connectivity
            reached = np.zeros(nNode, dtype=int)
            nreached = 1
            reached[0] = 1
            queue = [0]
            while len(queue) >= 1:
                node = queue[0]
                for near in range(nNode):
                    if self.adjmat[node][near] == 1 and reached[near] == 0:
                        queue.append(near)
                        reached[near] = 1
                        nreached += 1
                del(queue[0])
            if nreached < nNode:
                # reject the current graph and generate new one
                continue

            # make demand
            used_combinations = set()
            for i in range(nCommodity):
                while True:
                    a = random.randint(0, nNode-1)
                    b = random.randint(0, nNode-1)
                    if a == b or (a, b) in used_combinations:
                        continue
                    else:
                        used_combinations.add((a, b))
                        self.demand[a][b] = random.randint(demand_lb, demand_ub)
                        self.sd += self.demand[a][b]
                        break
            break

    # build mip model
    def build_model(self):
        model = Model('network design problem_' + str(nNode) + '_' + str(nModule) + '_' + str(uidx) + '_' + str(cidx) + '_' + str(randomidx))

        # variable
        # the MIP package automatically removes unused variables if the file are saved in MPS format
        # arc denoted by (i, j), commodity denoted by (a, b)
        # a->b denote a flow iff demand[a][b] != 0
        f = [[[[model.add_var(lb=0, ub=self.demand[a][b], name="f_%d_%d_%d_%d" % (i, j, a, b)) \
                for b in range(nNode)] for a in range(nNode)] for j in range(nNode)] for i in range(nNode)]
        # module
        y = [[[model.add_var(var_type=INTEGER, lb=0, ub=math.ceil(self.sd/u[k]), name="y_%d_%d_%d" % (i, j, k)) \
                for k in range(nModule)] for j in range(nNode)] for i in range(nNode)]

        # total flow on each link
        flow = [[model.add_var(lb=0, ub=self.sd, name="flow_%d_%d" % (i, j)) \
                for j in range(nNode)] for i in range(nNode)]

        # objective
        if model_type == 'directed':
            model.objective = minimize( xsum( c[k]*y[i][j][k] for i in range(nNode) for j in range(nNode) if self.adjmat[i][j] == 1 for k in range(nModule) ) \
                    + xsum( math.floor(self.distance[i][j])*flow[i][j] for i in range(nNode) for j in range(nNode) if self.adjmat[i][j] == 1 ) )
        else:
            model.objective = minimize( xsum( c[k]*y[i][j][k] for i in range(nNode) for j in range(nNode) if self.adjmat[i][j] == 1 and i < j for k in range(nModule) ) \
                    + xsum( math.floor(self.distance[i][j])*flow[i][j] for i in range(nNode) for j in range(nNode) if self.adjmat[i][j] == 1 and i < j ) )
        # flow balance constraints
        for a in range(nNode):
            for b in range(nNode):
                if self.demand[a][b] != 0:
                    for i in range(nNode):
                        if i == a:
                            temp_d = self.demand[a][b]
                        elif i == b:
                            temp_d = -self.demand[a][b]
                        else:
                            temp_d = 0
                        model.add_constr(xsum((f[j][i][a][b] - f[i][j][a][b]) for j in range(nNode) if self.adjmat[i][j] == 1) \
                                == temp_d, 'node_' + str(i) + '_' + str(a) + '_' + str(b))
        # capacity constraints
        for i in range(nNode):
            for j in range(nNode):
                if self.adjmat[i][j] == 1:
                    if model_type == 'directed':
                        model.add_constr(xsum(f[i][j][a][b] for a in range(nNode) for b in range(nNode) if self.demand[a][b] != 0) \
                                <= flow[i][j], 'capa_' + str(i) + '_' + str(j))
                        model.add_constr(flow[i][j] <= xsum(u[k]*y[i][j][k] for k in range(nModule)), 'facility_' + str(i) + '_' + str(j))
                    if model_type == 'undirected' and i < j:
                        model.add_constr(xsum(f[i][j][a][b]+f[j][i][a][b] for a in range(nNode) for b in range(nNode) if self.demand[a][b] != 0) \
                                <= flow[i][j], 'capa_' + str(i) + '_' + str(j))
                        model.add_constr(flow[i][j] <= xsum(u[k]*y[i][j][k] for k in range(nModule)), 'facility_' + str(i) + '_' + str(j))
                    if model_type == 'bidirected' and i < j:
                        model.add_constr(xsum(f[i][j][a][b] for a in range(nNode) for b in range(nNode) if self.demand[a][b] != 0) \
                                <= flow[i][j], 'capa_' + str(i) + '_' + str(j))
                        model.add_constr(xsum(f[j][i][a][b] for a in range(nNode) for b in range(nNode) if self.demand[a][b] != 0) \
                                <= flow[i][j], 'capa_' + str(j) + '_' + str(i))
                        model.add_constr(flow[i][j] <= xsum(u[k]*y[i][j][k] for k in range(nModule)), 'facility_' + str(i) + '_' + str(j))

        ins_name = 'ndp_' + str(nNode) + '_' + str(nModule) + '_' + str(uidx) + '_' + str(cidx) + '_' + str(randomidx)
        model.write(ins_name+'.mps')
        # make directory
        subprocess.run('mkdir -p ' + model_type, shell=True)
        # rename mps file
        subprocess.run('mv ' + ins_name + '.mps.mps.gz ' + model_type + '/' + ins_name + '.mps.gz', shell=True)

# -------------------------------------------------------------------------------- #

# set random seed
random.seed(0)
# decimal save length for distance
ds = 2
# region size
R = 100
# set the number of module
nModuleSet = [1,2,3]
### 1 modules ###
# cost
u_set_1 = [ [130],
            [170],
            [200] ]
# capacity
c_set_1 = [ [10000],
            [18000],
            [25000] ]
### 2 modules ###
# cost
u_set_2 = [ [130,   50],
            [170,   70],
            [200,   80] ]
# capacity
c_set_2 = [ [10000,     5000],
            [18000,     9000],
            [25000,    13000] ]
### 3 modules ###
# cost
u_set_3 = [ [130,   50,   20],
            [170,   70,   30],
            [200,   80,   30] ]
# capacity
c_set_3 = [ [10000,     5000,   2500],
            [18000,     9000,   5000],
            [25000,    13000,   9000] ]
# TODO
# demand range
demand_lb = 10
demand_ub = 190
# model type
model_type_set = ['directed', 'undirected', 'bidirected']
#model_type_set = ['directed']
# TODO
# the number of node
node_number_set = [50]
# number of commodity
nCommodity = 50

# main function
for model_type in model_type_set:
    print(model_type)
    # graph size
    for nNode in node_number_set:
        for nModule in nModuleSet:
            # set capacity and cost, i.e., u(c)_set := u(c)_set_<n>
            u_set = globals()["u_set_{}".format(nModule)]
            c_set = globals()["c_set_{}".format(nModule)]
            for uidx in range(len(u_set)):
                u = u_set[uidx]
                for cidx in range(len(c_set)):
                    c = c_set[cidx]
                    # random model index
                    for randomidx in range(3):
                        print("graph size: %d, module size: %d, uidx: %d, cidx: %d, randomidx: %d" % (nNode, nModule, uidx, cidx, randomidx))
                        # ============================== #
                        #generate model
                        data = Data()
                        data.make_data()
                        data.build_model()
                        del data
                        # ============================== #

# -------------------------------------------------------------------------------- #
