BEGIN{
   totexec = 0;
   totequa = 0;
   perexec = 0;
   perequa = 0;
   nprob = 0;
   printf("-----------------------------------------------------------------------------------------------------------\n");
   printf("probname  |    nexec|            nequa|         ntwovar|      nsingleton|    nimpliedfree|            nnew|\n");
}

function max(x,y)
{
   return (x) > (y) ? (x) : (y);
}

function min(x,y)
{
   return (x) < (y) ? (x) : (y);
}

function floor(x)
{
   return int(x);
}

function round(x)
{
   return int(x+0.5);
}

function ceil(x)
{
   return int(x) + ((x%1)!=0);
}

/Compare/ {
   nexec = $6;
   nequa = $8;
   ntwo  = $10;
   nsgt  = $12;
   nifv  = $14;
   nnew  = $16;
   n  = split ($1, a, ".");
   prob = a[2] "[" a[3] "]";
   if( nexec >= 1 )
   {
      printf("%-10.10s| ", prob);
      printf("%8d| ", nexec);
      printf("%8d (%3d %)|", nequa, 100*nequa/nexec);
      printf("%8d (%3d %)|", ntwo, 100*ntwo/nexec);
      printf("%8d (%3d %)|", nsgt, 100*nsgt/nexec);
      printf("%8d (%3d %)|", nifv, 100*nifv/nexec);
      printf("%8d (%3d %)|", nnew, 100*nnew/nexec);
      printf("\n");
      totexec += nexec;
      totequa += nequa;
      perequa += nequa/nexec;
      nprob += 1;
   }
}

END{
   printf("-----------------------------------------------------------------------------------------------------------\n");
   printf("number of instances: %d\n", nprob);
   printf("tot: equa/exec = %.2f%%\n", 100*totequa/totexec);
   printf("per: equa/exec = %.2f%%\n", 100*perequa/nprob);
}
