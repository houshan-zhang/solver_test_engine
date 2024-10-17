#include "paramset.h"
#include "functions.h"
#include <sys/stat.h>

/* remove space at the head and tail of the string */
void DeleteSpace(char* str)
{
   char *start = str;
   char *end = str + strlen(str) - 1;

   while( *start == ' ' )
      start++;

   while( end > start && *end == ' ' )
      end--;

   /* add the end flag */
   *(end + 1) = '\0';

   /* change the starting position */
   memmove(str, start, strlen(start) + 1);
}

/* check whether the input parameter is a .set file */
bool CheckEnds(char* input)
{
   size_t length = strlen(input);
   if( length < strlen(".set") )
      return false;
   char* ends = input + (length - strlen(".set"));
   if( !strcmp(ends, ".set") )
      return true;
   else
      return false;
}

/* constructor to set the default value */
Set::Set()
{
   timelimit = 7200;
   memorylimit = 32768;
   randseed = 0;
   mipgap = 1e-6;
   strcpy(soladress, " ");
   heurmode = 0;
   cpxmode = 0;
}

Set::~Set()
{
   free(soladress);
}

/* read parameters from a string */
void Set::ReadParam(char* input)
{
   char* name;
   char* value;
   char* setfile;
   char* token;
   setfile = NULL;
   token = strtok(input, " ");
   while( token != NULL )
   {
      char *equalsign = strchr(token, '=');
      if( equalsign != NULL )
      {
         *equalsign = '\0';
         name = token;
         value = equalsign + 1;
         /* set parameter values */
         if( !strcmp(name, "TIME") )
            timelimit = atoi(value);
         else if( !strcmp(name, "MEM") )
            memorylimit = atoi(value);
         else if( !strcmp(name, "SEED") )
            randseed = atoi(value);
         else if( !strcmp(name, "MINGAP") )
            mipgap = atof(value);
         else if( !strcmp(name, "MODE") )
            heurmode = atoi(value);
         else if( !strcmp(name, "CPX") )
            cpxmode = atoi(value);
         else if( !strcmp(name, "SOL") )
            strcpy(soladress, value);
         else if( !strcmp(name, "SETTING") )
         {
            if( CheckEnds(value) )
            {
               setfile = (char*)malloc(2000*sizeof(char));
               strcpy(setfile, value);
            }
            else
               printf("unidentified settings file - %s\n", value);
         }
         else
            printf("undefined parameter - %s\n", name);
      }
      token = strtok(NULL, " ");
   }
   if( setfile != NULL )
   {
      ReadFile(setfile);
      Free(setfile);
   }
}

/* read parameters from a file */
void Set::ReadFile(char* paramfile)
{
   char* name;
   char* value;
   FILE* file = fopen(paramfile, "r");
   if( file != NULL )
   {
      char line[2000];
      while( fgets(line, sizeof(line), file) )
      {
         char* delimiter = strchr(line, '=');
         if( delimiter != NULL )
         {
            *delimiter = '\0';
            /* parameter name */
            name = line;
            DeleteSpace(name);
            /* parameter value */
            value = delimiter + 1;
            DeleteSpace(value);
            /* set parameter values */
            if( !strcmp(name, "TIME") )
               timelimit = atoi(value);
            else if( !strcmp(name, "MEM") )
               memorylimit = atoi(value);
            else if( !strcmp(name, "SEED") )
               randseed = atoi(value);
            else if( !strcmp(name, "MINGAP") )
               mipgap = atof(value);
            else if( !strcmp(name, "MODE") )
               heurmode = atoi(value);
            else if( !strcmp(name, "CPX") )
               cpxmode = atoi(value);
            else if( !strcmp(name, "SOL") )
               strcpy(soladress, value);
            else
               printf("undefined parameter - %s\n", name);
         }
      }
      fclose(file);
   }
   else
   {
      printf("parameter file not found - using default parameters\n");
   }
}

/* print parameter values */
void Set::PrintParam()
{
   printf("TIME: %d\n", timelimit);
   printf("MEM: %d\n", memorylimit);
   printf("SEED: %d\n", randseed);
   printf("MINGAP: %f\n", mipgap);
   printf("HEUR MODE: %d\n", heurmode);
   printf("CPX MODE: %d\n", cpxmode);
   printf("SOL: %s\n", soladress);
}

int file_exists(const char *filename)
{
   struct stat buffer;
   return (stat(filename, &buffer) == 0);
}

bool Set::check_benchmark_file(char* path, double* ref_obj)
{
   char* path_copy = (char*)malloc(2000*sizeof(char));
   strcpy(path_copy, path);

   char *lastSlash = strrchr(path_copy, '/');
   /* get instance name */
   char *name = strrchr(path_copy, '/');
   name++; // 跳过最后的 '/'
   char *dot = strchr(name, '.');
   *dot = '\0';

   /* get ndp type */
   char type[2000];
   strncpy(type, path_copy, lastSlash - path_copy);
   type[lastSlash - path_copy] = '\0'; // 截断字符串

   /* construct the path_copy of benchmark.txt */
   int needed = snprintf(NULL, 0, "%s/benchmark.txt", type);
   char benchmark[needed + 1];
   snprintf(benchmark, needed + 1, "%s/benchmark.txt", type);

   if( !file_exists(benchmark) )
   {
      Free(path_copy);
      return false;
   }

   /* read benchmark.txt */
   FILE *file = fopen(benchmark, "r");
   if (!file)
   {
      Free(path_copy);
      return false;
   }

   char line[2000];
   while( fgets(line, sizeof(line), file) )
   {
      char firstColumn[1000], secondColumn[1000];
      if( sscanf(line, "%s %s", firstColumn, secondColumn) == 2 )
      {
         if( strcmp(firstColumn, name) == 0 )
         {
            *ref_obj = atof(secondColumn);
            Free(path_copy);
            fclose(file);
            return true;
         }
      }
   }

   Free(path_copy);
   fclose(file);
   return false;
}
