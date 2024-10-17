#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <mpi.h>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <ctime>

using namespace std;

#define MAX_LINE           1000
#define MAX_HOST_NAME      16

typedef struct
{
   int myid;
   char name[MAX_HOST_NAME];
}HostInfo;

/* check whether host i has enough thread to run a new job */
bool CheckThreads(vector<int>& numthreads, int i, int workthread, int totalthread)
{
   if( numthreads[i] + workthread <= totalthread )
      return true;
   else
      return false;
}

static string getInsName(string command)
{
   string name;
   stringstream str1(command);
   for( int i = 0; i < 7; i++ )
   {
      getline(str1, name, '=');
   }
   stringstream str2(name);
   for( int i = 0; i < 1; i++ )
   {
      getline(str2, name, ' ');
   }
   return name;
}

int main(int argc,char *argv[])
{
   /* number of threads per host */
   int totalthread = 8;
   /* number of threads per job */
   int workthread = 1;
   if( argc < 2 )
   {
      printf("Error!, please enter the tasks file\n");
      exit(-1);
   }
   if( argc >= 3 )
      totalthread = atoi(argv[2]);
   if( argc >= 4 )
      workthread = atoi(argv[3]);
   if( workthread > totalthread )
   {
      printf("Error!, workthread cannot be greater than totalthread\n");
      exit(-1);
   }
   int myid;
   int numprocs;
   MPI_Init(&argc, &argv);
   /* get the current process index */
   MPI_Comm_rank(MPI_COMM_WORLD, &myid);
   /* get the number of processes */
   MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
   /* check thread number */
   if( numprocs <= 1 )
   {
      printf("only one thread is available!");
      return 0;
   }
   /* make new data type in mpi to collect the host information */
   HostInfo hostinfo;
   MPI_Datatype Type_HostInfo;
   MPI_Datatype Struct_HostInfo[2] = {MPI_INT, MPI_CHAR};
   /* number of elements in each block */
   int blocklens[2] = {1, MAX_HOST_NAME};
   /* byte displacement of each block */
   MPI_Aint indices[2];
   /* old version function of mpi */
   //MPI_Address(&hostinfo, &indices[0]);
   //MPI_Address(&hostinfo.name, &indices[1]);
   MPI_Get_address(&hostinfo, &indices[0]);
   MPI_Get_address(&hostinfo.name, &indices[1]);
   indices[1] -= indices[0];
   indices[0] = 0;
   /* make new date */
   //MPI_Type_struct(2, blocklens, indices, Struct_HostInfo, &Type_HostInfo);
   MPI_Type_create_struct(2, blocklens, indices, Struct_HostInfo, &Type_HostInfo);
   /* commit new data */
   MPI_Type_commit(&Type_HostInfo);

   /* get host information */
   hostinfo.myid = myid;
   gethostname(hostinfo.name, MAX_HOST_NAME);

   /* master thread */
   if( myid == 0 )
   {
      string filename = argv[1];
      ifstream file;
      file.open(filename);
      if( !file.is_open() )
      {
         printf("Error! cannot open file %s\n", filename.c_str());
         exit(-1);
      }
      /* number of threads working at the same time */
      int nworker = numprocs-1;
      unordered_map<string, int> map;
      unordered_map<string, int>::iterator iter;
      vector<int> thread2host(numprocs);
      /* host name list */
      vector<string> hostnames;
      /* number of working threads per host */
      vector<int> numthreads;
      map[hostinfo.name] = 0;
      thread2host[myid] = 0;
      hostnames.push_back(hostinfo.name);
      /* master use 1 thread */
      numthreads.push_back(1);
      /* collect host information from workers */
      for( int i = 1; i < numprocs; i++ )
      {
         MPI_Recv(&hostinfo, 1, Type_HostInfo, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         iter = map.find(hostinfo.name);
         if( iter == map.end() )
         {
            map[hostinfo.name] = hostnames.size();
            thread2host[hostinfo.myid] = hostnames.size();
            hostnames.push_back(hostinfo.name);
            /* add load to the host */
            numthreads.push_back(workthread);
         }
         else
         {
            thread2host[hostinfo.myid] = iter->second;
            //TODO check number of threads
            if( CheckThreads(numthreads, iter->second, workthread, totalthread) )
               numthreads[iter->second] += workthread;
            else
            {
               thread2host[hostinfo.myid] = -1;
               nworker -= 1;
            }
         }
      }
      printf("--- Host Statistic --- %d hosts, %d threads, %d wokers, %d workthread, %d totalthread.\n", int(hostnames.size()), numprocs, nworker, workthread, totalthread);
      /* turn the worker threads on or off depend on the value of thread2host */
      for( int i = 1; i < numprocs; i++ )
         MPI_Send(&thread2host[i], 1, MPI_INT, i, 2, MPI_COMM_WORLD);

      /* target process index */
      int target;
      /* inline command in task file */
      string command;
      /* start time of each job */
      vector<double> startTime;
      /* run time of each job */
      vector<double> runTime;
      /* thread accepts the task order */
      vector<int> threadOrder;
      /* command execution order */
      vector<string> cmdOrder;
      /* current job of each worker, -1 means no job */
      vector<int> currentJobIdx(numprocs, -1);
      /* work(command) index */
      int workIdx = 0;
      /* master assigns tasks to workers */
      cout<<endl<<"============================== MPI START =============================="<<endl<<endl;
      while( getline(file, command) )
      {
         /* process target is ready to execute the command */
         MPI_Recv(&target, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         /* if the previous work completed, record the run time */
         if( currentJobIdx[target] >= 0 )
         {
            runTime[currentJobIdx[target]] = MPI_Wtime() - startTime[currentJobIdx[target]];
            cout<<"---END--- "<<getInsName(cmdOrder[currentJobIdx[target]])<<endl;
         }
         currentJobIdx[target] = workIdx;
         startTime.push_back(MPI_Wtime());
         runTime.push_back(0);
         threadOrder.push_back(target);
         cmdOrder.push_back(command);
         /* command.size() needs plus 1, because the function size() does not count \0 */
         MPI_Send(command.c_str(), command.size()+1, MPI_CHAR, target, 0, MPI_COMM_WORLD);
         cout<<"---SUBMIT--- "<<getInsName(command)<<endl;
         workIdx++;
      }
      /* get the current system time */
      time_t now = std::time(nullptr);
      /* convert to a string and output */
      cout<<endl<<"Submit Over, Current Time: "<<asctime(localtime(&now));
      cout<<endl<<"========== exit =========="<<endl<<endl;
      /* stop running with a number of nworker */
      for( int i = 0; i < nworker; i++ )
      {
         /* turn on worker threads */
         char none[5] = "none";
         //if( thread2host[i] >= 0 )
         {
            MPI_Recv(&target, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(none, 5, MPI_CHAR, target, 0, MPI_COMM_WORLD);
            cout<<"thread "<<target<<" ends"<<endl;
         }
         if( currentJobIdx[target] >= 0 )
         {
            runTime[currentJobIdx[target]] = MPI_Wtime() - startTime[currentJobIdx[target]];
            cout<<"---END--- "<<getInsName(cmdOrder[currentJobIdx[target]])<<endl;
         }
      }
      cout<<endl<<"========== over =========="<<endl<<endl;
      for( int i = 0; i < workIdx; i++ )
      {
         printf("job %4d \t costs %5.1f \t seconds in thread %4d \t  run %s\n", i, runTime[i], threadOrder[i], getInsName(cmdOrder[i]).c_str());
      }
      file.close();
      cout<<endl<<"==============================  MPI END  =============================="<<endl<<endl;
   }
   /* worker thread */
   else
   {
      /* send the host information to master */
      int isuse;
      MPI_Send(&hostinfo, 1, Type_HostInfo, 0, 1, MPI_COMM_WORLD);
      MPI_Recv(&isuse, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      /* receive command from master */
      char command[MAX_LINE];
      int status;
      /* keep running until receive a stop signal from the master */
      while( isuse >= 0 )
      {
         /* waiting task */
         MPI_Send(&myid, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
         /* workers receives tasks froms master */
         MPI_Recv(command, MAX_LINE, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         if( strcmp(command, "none") == 0 )
         {
            break;
         }
         /* execute the command */
         status = system(command);
         if( status != 0 )
         {
            printf("work %d error! instance name: %s\n", myid, getInsName(command).c_str());
         }
      }
   }
   MPI_Type_free(&Type_HostInfo);
   MPI_Finalize();
}
