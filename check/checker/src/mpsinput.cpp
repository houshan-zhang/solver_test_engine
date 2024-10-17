/**
 * @file mpsinput.cpp
 * @brief MPS reader class
 */

#include "mpsinput.h"
#include "model.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <sstream>
#include <iostream>

#define MPS_MIN_LINELEN   80
#define MPS_MAX_NAMELEN   256
#define PATCH_CHAR        '_'
#define BLANK             ' '
#define INFBOUND          1e20

/* fill the line from \p pos up to column MPS_MIN_LINELEN with blanks. */
static void clearFrom(
      char*                 buf,
      unsigned int          pos
      )
{
   for( unsigned int i = pos; i < MPS_MIN_LINELEN; ++i )
      buf[i] = BLANK;
   buf[MPS_MIN_LINELEN] = '\0';
}

/* change all blanks inside a field to #PATCH_CHAR. */
static void patchField(
      char*                 buf,
      int                   beg,
      int                   end
      )
{
   while( (beg <= end) && (buf[end] == BLANK) )
      end--;

   while( (beg <= end) && (buf[beg] == BLANK) )
      beg++;

   for( int i = beg; i <= end; i++ )
      if( buf[i] == BLANK )
         buf[i] = PATCH_CHAR;
}

MpsInput::MpsInput()
{
   section     = MPS_NAME;
   model       = NULL;
   fp          = NULL;
   gzfp        = NULL;
   isZipped    = false;
   linenu      = 0;
   isInteger   = false;
   isFreeFormat = false;
   buf[0]      = '\0';
   f0          = NULL;
   f1          = NULL;
   f2          = NULL;
   f3          = NULL;
   f4          = NULL;
   f5          = NULL;
}

/* read a mps format data line and parse the fields. */
bool MpsInput::readLine()
{
   bool isMarker;
   bool isEmpty;
   char* nexttok;

   do
   {
      f0 = f1 = f2 = f3 = f4 = f5 = 0;
      isMarker = false;

      /* Read until we have a not comment line. */
      if( isZipped )
      {
         do
         {
            memset((void*)buf, 0, MPS_MAX_LINELEN);
            if( gzgets(gzfp, buf, sizeof(buf)) == NULL )
               return false;
            linenu++;
         }
         while( buf[0] == '*' );
      }
      else
      {
         do
         {
            memset((void*)buf, 0, MPS_MAX_LINELEN);
            if ( fgets(buf, sizeof(buf), fp) == NULL )
               return false;
            linenu++;
         }
         while( buf[0] == '*' );
      }

      /* Normalize line */
      unsigned int len = strlen(buf);
      for( unsigned int i = 0; i < len; i++ )
      {
         if( (buf[i] == '\t') || (buf[i] == '\n') || (buf[i] == '\r') )
            buf[i] = BLANK;
      }
      if( len < MPS_MIN_LINELEN )
         clearFrom(buf, len);
      assert(strlen(buf) >= MPS_MIN_LINELEN);

      /* Look for new section */
      if( buf[0] != BLANK )
      {
         f0 = strtok_r(buf, " ", &nexttok);
         f1 = strtok_r(NULL, " ", &nexttok);
         return true;
      }

      /* If we decide to use the free format we never revert this decision */
      if( !isFreeFormat )
      {
         /* Test for fixed format comments */
         if( (buf[14] == '$') && (buf[13] == ' ') )
            clearFrom(buf, 14);
         else if( (buf[39] == '$') && (buf[38] == ' ') )
            clearFrom(buf, 39);

         /* Test for fixed format */
         int space = buf[12] | buf[13]
            | buf[22] | buf[23]
            | buf[36] | buf[37] | buf[38]
            | buf[47] | buf[48]
            | buf[61] | buf[62] | buf[63];

         if ( space == BLANK )
         {
            /* Now we have space at the right positions.
             * But are there also the non space where they
             * should be ?
             */
            int number = isdigit(buf[24]) || isdigit(buf[25])
               || isdigit(buf[26]) || isdigit(buf[27])
               || isdigit(buf[28]) || isdigit(buf[29])
               || isdigit(buf[30]) || isdigit(buf[31])
               || isdigit(buf[32]) || isdigit(buf[33])
               || isdigit(buf[34]) || isdigit(buf[35]);

            /* len < 13 is handle ROW lines with embedded spaces
             * in the names correctly
             */
            if( number || len < 13 )
            {
               /* We assume fixed format, so we patch possible embedded spaces. */
               patchField(buf,  4, 12);
               patchField(buf, 14, 22);
               patchField(buf, 39, 47);
            }
            else
            {
               if( section == MPS_COLUMNS || section == MPS_RHS || section == MPS_RANGES  || section == MPS_BOUNDS )
                  isFreeFormat = true;
            }
         }
         else
         {
            isFreeFormat = true;
         }
      }
      char* s = &buf[1];

      /* At this point it is not clear if we have a indicator field.
       * If there is none (e.g. empty) f1 will be the first name field.
       * If there is one, f2 will be the first name field.
       *
       * Initially comment marks '$' are only allowed in the beginning
       * of the 2nd and 3rd name field. We test all fields but the first.
       * This makes no difference, since if the $ is at the start of a value
       * field, the line will be errornous anyway.
       */
      do
      {
         if( NULL == (f1 = strtok_r(s, " ", &nexttok)) )
            break;

         if( (NULL == (f2 = strtok_r(NULL, " ", &nexttok))) || (*f2 == '$') )
         {
            f2 = 0;
            break;
         }
         if( !strcmp(f2, "'MARKER'") )
            isMarker = true;

         if( (NULL == (f3 = strtok_r(NULL, " ", &nexttok))) || (*f3 == '$') )
         {
            f3 = 0;
            break;
         }
         if( isMarker )
         {
            if( !strcmp(f3, "'INTORG'") )
               isInteger = true;
            else if( !strcmp(f3, "'INTEND'") )
               isInteger = false;
            else
               break; /* unknown marker */
         }
         if( !strcmp(f3, "'MARKER'") )
            isMarker = true;

         if( (NULL == (f4 = strtok_r(NULL, " ", &nexttok))) || (*f4 == '$') )
         {
            f4 = 0;
            break;
         }
         if( isMarker )
         {
            if( !strcmp(f4, "'INTORG'") )
               isInteger = true;
            else if( !strcmp(f4, "'INTEND'") )
               isInteger = false;
            else
               break; /* unknown marker */
         }
         if( (NULL == (f5 = strtok_r(NULL, " ", &nexttok))) || (*f5 == '$') )
            f5 = 0;
      }
      while( false );

      /* check for empty lines */
      isEmpty = (f0 == NULL && f1 == NULL);
   }
   while( isMarker || isEmpty );

   return true;
}

/* Insert \p name as field 1 and shift all other fields up. */
void MpsInput::insertName(
      const char*     name,
      bool            second
      )
{
   assert(name != NULL);

   f5 = f4;
   f4 = f3;
   f3 = f2;

   if( second )
      f2 = name;
   else
   {
      f2 = f1;
      f1 = name;
   }
}

/* Process NAME section. */
void MpsInput::readName()
{
   /* This has to be the Line with the NAME section. */
   if( !readLine() || (f0 == NULL) || strcmp(f0, "NAME") )
      printf("Read MPS file error! Line %d\n", linenu);

   /* Sometimes the name is omitted. */
   if( f1 == 0 )
      model->modelName = "_MPS_";
   else
      model->modelName = std::string(f1);

   /* This has to be a new section */
   if( !readLine() || (f0 == NULL) )
      printf("Read MPS file error! Line %d\n", linenu);

   if( !strncmp(f0, "ROWS", 4) )
      section = MPS_ROWS;
   else if( !strncmp(f0, "USERCUTS", 8) )
      section = MPS_USERCUTS;
   else if( !strncmp(f0, "LAZYCONS", 8) )
      section = MPS_LAZYCONS;
   else if( !strncmp(f0, "OBJSEN", 6) )
      section = MPS_OBJSEN;
   else if( !strncmp(f0, "OBJNAME", 7) )
      section = MPS_OBJNAME;
   else if( !strncmp(f0, "INDICATORS", 10) )
      section = MPS_INDICATORS;
   else
      printf("Read MPS file error! Line %d\n", linenu);
}

/* Process OBJSEN section. This Section is an ILOG extension. */
void MpsInput::readObjsen()
{
   /* This has to be the Line with MIN or MAX. */
   if( !readLine() || (f1 == NULL) )
      printf("Read MPS file error! Line %d\n", linenu);

   if( !strncmp(f1, "MIN", 3) )
      model->objSense = Model::MINIMIZE;
   else if( !strncmp(f1, "MAX", 3) )
      model->objSense = Model::MAXIMIZE;
   else
      printf("Read MPS file error! Line %d\n", linenu);

   /* Look for ROWS, USERCUTS, LAZYCONS, or OBJNAME Section */
   if( !readLine() || f0 == NULL )
      printf("Read MPS file error! Line %d\n", linenu);

   if( !strcmp(f0, "ROWS") )
      section = MPS_ROWS;
   else if( !strcmp(f0, "USERCUTS") )
      section = MPS_USERCUTS;
   else if( !strcmp(f0, "LAZYCONS") )
      section = MPS_LAZYCONS;
   else if( !strcmp(f0, "OBJNAME") )
      section = MPS_OBJNAME;
   else
      printf("Read MPS file error! Line %d\n", linenu);
}

/* Process OBJNAME section. This Section is an ILOG extension. */
void MpsInput::readObjname()
{
   /* This has to be the Line with the name. */
   if( !readLine() || f1 == NULL )
      printf("Read MPS file error! Line %d\n", linenu);

   printf("readObjName %s\n", f1);
   model->objName = std::string(f1);

   /* Look for ROWS, USERCUTS, or LAZYCONS Section */
   if( !readLine() || f0 == NULL )
      printf("Read MPS file error! Line %d\n", linenu);

   if( !strcmp(f0, "ROWS") )
      section = MPS_ROWS;
   else if( !strcmp(f0, "USERCUTS") )
      section = MPS_USERCUTS;
   else if( !strcmp(f0, "LAZYCONS") )
      section = MPS_LAZYCONS;
   else
      printf("Read MPS file error! Line %d\n", linenu);
}

/* Process ROWS, USERCUTS, or LAZYCONS section. */
void MpsInput::readRows()
{
   while( readLine() )
   {
      if( f0 != NULL )
      {
         if( !strcmp(f0, "ROWS") )
            section = MPS_ROWS;
         else if( !strcmp(f0, "USERCUTS") )
            section = MPS_USERCUTS;
         else if( !strcmp(f0, "LAZYCONS") )
            section = MPS_LAZYCONS;
         else if( !strcmp(f0, "COLUMNS") )
            section = MPS_COLUMNS;
         else
            printf("Read MPS file error! Line %d\n", linenu);
         return;
      }

      if( *f1 == 'N' )
      {
         if( model->objName.empty() )
            model->objName = std::string(f2);
      }
      else
      {
         LinearConstraint::LinearType ctype;
         Rational clb; // default to zero
         Rational cub; // default to zero

         switch(*f1)
         {
            case 'G' :
               ctype = LinearConstraint::GREATER_THAN;
               cub = INFBOUND;
               break;
            case 'E' :
               ctype = LinearConstraint::EQUAL;
               break;
            case 'L' :
               ctype = LinearConstraint::LESS_THAN;
               clb = -INFBOUND;
               break;
            default :
               printf("Read MPS file error! Line %d\n", linenu);
         }

         model->pushCons(new LinearConstraint(f2, ctype, clb, cub));
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process COLUMNS section. */
void MpsInput::readCols()
{
   char colname[MPS_MAX_NAMELEN] = { '\0' };

   while( readLine() )
   {
      if( f0 != 0 )
      {
         if( strcmp(f0, "RHS") )
            break;
         section = MPS_RHS;
         return;
      }
      if( f1 == NULL || f2 == NULL || f3 == NULL )
         break;

      /* new column */
      if( strcmp(colname, f1) )
      {
         (void)strncpy(colname, f1, MPS_MAX_NAMELEN - 1);

         if( isInteger )
         {
            /* for integer variables, default bounds are 0 <= x , and default cost is 0 */
            model->pushVar(new Var(colname, Var::INTEGER, 0.0, INFBOUND, 0.0));
         }
         else
         {
            /* for continuous variables, default bounds are 0 <= x, and default cost is 0 */
            model->pushVar(new Var(colname, Var::CONTINUOUS, 0.0, INFBOUND, 0.0));
         }
      }

      Var* var = model->getVar(colname);
      assert( var != NULL );
      Rational val;
      val.fromString(f3);

      if( std::string(f2) == model->objName )
         var->objCoef = val;
      else
      {
         LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f2));
         /* we cannot assert cons != NULL, because we may have ignored free rows! */
         if(cons != NULL)
            cons->push(var, val);
      }
      if( f5 != NULL )
      {
         assert( f4 != NULL );

         val.fromString(f5);

         if( std::string(f4) == model->objName )
            var->objCoef = val;
         else
         {
            LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f4));
            /* we cannot assert cons != NULL, because we may have ignored free rows! */
            if(cons != NULL)
               cons->push(var, val);
         }
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process RHS section. */
void MpsInput::readRhs()
{
   Rational val;

   while( readLine() )
   {
      if( f0 != NULL )
      {
         if( !strcmp(f0, "RANGES") )
            section = MPS_RANGES;
         else if( !strcmp(f0, "BOUNDS") )
            section = MPS_BOUNDS;
         else if( !strcmp(f0, "INDICATORS") )
            section = MPS_INDICATORS;
         else if( !strcmp(f0, "SOS") )
            section = MPS_SOS;
         else if( !strcmp(f0, "ENDATA") )
            section = MPS_ENDATA;
         else
            break;
         return;
      }
      if( (f2 != NULL && f3 == NULL)
            || (f4 != NULL && f5 == NULL) )
         insertName("_RHS_", false);

      if( f1 == NULL || f2 == NULL || f3 == NULL )
         break;

      val.fromString(f3);
      LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f2));
      if( cons == NULL )
      {
         if( std::string(f2) == model->objName )
            model->objConstant = val *= -1;
         continue;
      }
      switch(cons->lintype)
      {
         case LinearConstraint::LESS_THAN:
            cons->rhs = val;
            break;
         case LinearConstraint::GREATER_THAN:
            cons->lhs = val;
            break;
         case LinearConstraint::EQUAL:
            cons->lhs = val;
            cons->rhs = val;
            break;
         default:
            printf("Read MPS file error! Line %d\n", linenu);
      }
      if( f5 != NULL )
      {
         val.fromString(f5);
         LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f4));
         if( cons == NULL )
         {
            if( std::string(f4) == model->objName )
               model->objConstant = val *= -1;
            continue;
         }
         switch(cons->lintype)
         {
            case LinearConstraint::LESS_THAN:
               cons->rhs = val;
               break;
            case LinearConstraint::GREATER_THAN:
               cons->lhs = val;
               break;
            case LinearConstraint::EQUAL:
               cons->lhs = val;
               cons->rhs = val;
               break;
            default:
               printf("Read MPS file error! Line %d\n", linenu);
         }
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process RANGES section */
void MpsInput::readRanges()
{
   char rngname[MPS_MAX_NAMELEN] = {'\0'};
   Rational val;

   while( readLine() )
   {
      if( f0 != NULL )
      {
         if( !strcmp(f0, "BOUNDS") )
            section = MPS_BOUNDS;
         else if( !strcmp(f0, "INDICATORS") )
            section = MPS_INDICATORS;
         else if( !strcmp(f0, "SOS") )
            section = MPS_SOS;
         else if( !strcmp(f0, "ENDATA") )
            section = MPS_ENDATA;
         else
            break;
         return;
      }
      if( (f2 != NULL && f3 == NULL )
            || (f4 != NULL && f5 == NULL) )
         insertName("_RNG_", false);

      if( f1 == NULL || f2 == NULL || f3 == NULL )
         break;

      if( *rngname == '\0' )
         (void)strncpy(rngname, f1, MPS_MAX_NAMELEN - 1);

      /* The rules are:
       * Row Sign   LHS             RHS
       * ----------------------------------------
       *  G   +/-   rhs             rhs + |range|
       *  L   +/-   rhs - |range|   rhs
       *  E   +     rhs             rhs + range
       *  E   -     rhs + range     rhs
       * ----------------------------------------
       */
      if( !strcmp(rngname, f1) )
      {
         val.fromString(f3);
         LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f2));
         assert( cons != NULL );
         switch(cons->lintype)
         {
            case LinearConstraint::LESS_THAN:
               val.abs();
               sub(cons->lhs, cons->rhs, val);
               break;
            case LinearConstraint::GREATER_THAN:
               val.abs();
               add(cons->rhs, cons->lhs, val);
               break;
            case LinearConstraint::EQUAL:
               if (val.isPositive())
                  cons->rhs += val;
               else
                  cons->lhs += val;
               break;
            default:
               printf("Read MPS file error! Line %d\n", linenu);
         }
         cons->lintype = LinearConstraint::RANGED;
         if( f5 != NULL )
         {
            val.fromString(f5);
            LinearConstraint* cons = static_cast<LinearConstraint*>(model->getCons(f4));
            assert( cons != NULL );
            switch(cons->lintype)
            {
               case LinearConstraint::LESS_THAN:
                  val.abs();
                  sub(cons->lhs, cons->rhs, val);
                  break;
               case LinearConstraint::GREATER_THAN:
                  val.abs();
                  add(cons->rhs, cons->lhs, val);
                  break;
               case LinearConstraint::EQUAL:
                  if (val.isPositive())
                     cons->rhs += val;
                  else
                     cons->lhs += val;
                  break;
               default:
                  printf("Read MPS file error! Line %d\n", linenu);
            }
            cons->lintype = LinearConstraint::RANGED;
         }
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process BOUNDS section. */
void MpsInput::readBounds()
{
   Rational val;
   Rational zero;
   Rational one(1);
   bool shifted;

   while( readLine() )
   {
      if( f0 != 0 )
      {
         if( !strcmp(f0, "INDICATORS") )
            section = MPS_INDICATORS;
         else if( !strcmp(f0, "SOS") )
            section = MPS_SOS;
         else if( !strcmp(f0, "ENDATA") )
            section = MPS_ENDATA;
         else
            break;
         return;
      }

      shifted = false;

      /* Is the value field used ? */
      if( !strcmp(f1, "LO")  /* lower bound given in field 4 */
            || !strcmp(f1, "UP")  /* upper bound given in field 4 */
            || !strcmp(f1, "FX")  /* fixed value given in field 4 */
            || !strcmp(f1, "LI")  /* CPLEX extension: lower bound of integer variable given in field 4 */
            || !strcmp(f1, "UI")  /* CPLEX extension: upper bound of integer variable given in field 4 */
            || !strcmp(f1, "SC") )/* CPLEX extension: semi continuous variable, upper bound given in field 4 */
      {
         if( f3 != NULL && f4 == NULL )
         {
            insertName("_BND_", true);
            shifted = true;
         }
      }
      else if( !strcmp(f1, "FR") /* free variable */
            || !strcmp(f1, "MI")    /* lower bound is minus infinity */
            || !strcmp(f1, "PL")    /* upper bound is plus infinity */
            || !strcmp(f1, "BV") )  /* CPLEX extension: binary variable */
      {
         if( f2 != NULL && f3 == NULL )
         {
            insertName("_BND_", true);
            shifted = true;
         }
      }
      else
         printf("Read MPS file error! Line %d\n", linenu);

      if( f1 == NULL || f2 == NULL || f3 == NULL )
         break;

      Var* var = model->getVar(f3);
      assert( var != NULL );
      if( f4 == NULL )
         val = 0.0;
      else
         val.fromString(f4);

      /* if a bound of a binary variable is given, the variable is converted into an integer variable
       * with default bounds 0 <= x <= infinity
       */
      if( var->type == Var::BINARY )
      {
         if( (f1[1] == 'I') /* ILOG extension (Integer Bound) */
               || (!(f1[0] == 'L' && (val == zero)) && !(f1[0] == 'U' && (val == one))) )
         {
            var->type = Var::INTEGER;
            var->ub = INFBOUND;
         }
      }

      switch( f1[0] )
      {
         case 'L':
            if( f1[1] == 'I' ) /* ILOG extension (Integer Bound) */
               var->type = Var::INTEGER;
            var->lb = val;
            break;
         case 'U':
            if( f1[1] == 'I' ) /* ILOG extension (Integer Bound) */
               var->type = Var::INTEGER;
            var->ub = val;
            break;
         case 'F':
            if( f1[1] == 'X' )
            {
               var->lb = val;
               var->ub = val;
            }
            else
            {
               var->lb = -INFBOUND;
               var->ub = INFBOUND;
            }
            break;
         case 'M':
            var->lb = -INFBOUND;
            break;
         case 'P':
            var->ub = INFBOUND;
            break;
         case 'B' : /* Ilog extension (Binary) */
            var->type = Var::BINARY;
            max(var->lb, zero, var->lb);
            min(var->ub, one, var->ub);
            break;
         case 'S' : /* Ilog extension (Semi-Continuous) */
            var->type = Var::SEMICONTINUOUS;
            var->lb = one;
            var->ub = val;
            break;
         default:
            printf("Read MPS file error! Line %d\n", linenu);
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process SOS section [non standard ILOG extension]. */
void MpsInput::readSOS()
{
   char sosname[MPS_MAX_NAMELEN] = { '\0' };
   int cnt = 0;

   while( readLine() )
   {
      if( f0 != NULL )
      {
         if( !strcmp(f0, "INDICATORS") )
            section = MPS_INDICATORS;
         else if( !strcmp(f0, "ENDATA") )
            section = MPS_ENDATA;
         else
            break;
         return;
      }

      if( f1 == NULL && f2 == NULL )
         break;

      /* check for new SOS set */
      int type = -1;
      if( !strcmp(f1, "S1") )
         type = 1;
      if( !strcmp(f1, "S2") )
         type = 2;

      if( type > 0 )
      {
         assert( type == 1 || type == 2 );
         if( f2 != NULL )
            (void)strncpy(sosname, f2, MPS_MAX_NAMELEN - 1);
         else
            (void)snprintf(sosname, MPS_MAX_NAMELEN, "SOS%d", ++cnt);
         /* create SOS constraint */
         if( type == 1 )
            model->pushCons(new SOSConstraint(sosname, SOSConstraint::TYPE_1));
         else if( type == 2 )
            model->pushCons(new SOSConstraint(sosname, SOSConstraint::TYPE_2));
      }
      else
      {
         SOSConstraint* cons = static_cast<SOSConstraint*>(model->getCons(sosname));
         assert( cons != NULL );
         /* add vars to SOS constraint */
         Var* var = model->getVar(f1);
         assert( var != NULL );
         cons->push(var);
      }
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Process INDICATORS section. */
void MpsInput::readIndicators()
{
   Rational ifval;
   Rational zero;
   Rational one(1);

   while( readLine() )
   {
      if( f0 != 0 )
      {
         if( !strcmp(f0, "SOS") )
            section = MPS_SOS;
         else if( !strcmp(f0, "ENDATA") )
            section = MPS_ENDATA;
         else
            break;
         return;
      }

      /* All indicator rows start with 'IF' */
      if (strcmp(f1, "IF")) printf("Read MPS file error! Line %d\n", linenu);

      /* These fields are mandatory */
      if( f2 == NULL || f3 == NULL || f4 == NULL ) printf("Read MPS file error! Line %d\n", linenu);

      /* Get indicator data */
      Var* ifvar = model->getVar(f3);
      assert( ifvar != NULL );
      ifval.fromString(f4);
      Constraint* thencons = model->getCons(f2);
      assert( thencons != NULL );
      assert( ifval == one || ifval == zero );

      /* Remove old constraint named 'f2' */
      model->removeCons(f2);

      /* Add indicator constraint */
      model->pushCons(new IndicatorConstraint(f2, ifvar, ifval == one, thencons));
   }
   printf("Read MPS file error! Line %d\n", linenu);
}

/* Read LP in "MPS File Format".
 *
 *  A specification of the MPS format can be found at
 *
 *  http://plato.asu.edu/ftp/mps_format.txt,
 *  http://lpsolve.sourceforge.net/5.5/mps-format.htm
 *  http://cgm.cs.mcgill.ca/~avis/courses/567/cplex/reffileformatscplex.pdf
 *
 *  This routine should read all valid MPS format files.
 *  What it will not do, is to find all cases where a file is ill formed.
 *  If this happens it may complain and read nothing or read "something".
 */

bool MpsInput::readMps(
      const char*           _filename,
      Model*                _model
      )
{
   assert( _filename != NULL );
   assert( _model != NULL );

   int pathlen = strlen(_filename);
   if( strcmp(_filename + pathlen - 3, ".gz") == 0 )
   {
      isZipped = true;
      gzfp = gzopen(_filename, "r");
      if( gzfp == NULL )
      {
         printf("Cannot open file <%s> for reading\n", _filename);
         return false;
      }
   }
   else
   {
      fp = fopen(_filename, "r");
      if( fp == NULL )
      {
         printf("Cannot open file <%s> for reading\n", _filename);
         return false;
      }
   }
   assert( fp != NULL || gzfp != NULL );

   model = _model;

   readName();

   /* sections must be in the specified order */
   if( section == MPS_OBJSEN )
      readObjsen();
   if( section == MPS_OBJNAME )
      readObjname();
   while( section == MPS_ROWS || section == MPS_USERCUTS || section == MPS_LAZYCONS )
      readRows();
   if( section == MPS_COLUMNS )
      readCols();
   if( section == MPS_RHS )
      readRhs();
   if( section == MPS_RANGES )
      readRanges();
   if( section == MPS_BOUNDS )
      readBounds();
   if( section == MPS_SOS )
      readSOS();
   if( section == MPS_INDICATORS )
      readIndicators();
   if( section != MPS_ENDATA )
      printf("Read MPS file error! Line %d\n", linenu);

   model = NULL;

   if( isZipped )
   {
      gzclose(gzfp);
      gzfp = NULL;
   }
   else
   {
      fclose(fp);
      fp = NULL;
   }

   return true;
}
