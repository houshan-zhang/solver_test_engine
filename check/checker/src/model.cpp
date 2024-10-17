/**
 * @file model.cpp
 * @brief Basic classes to describe a MIP model
 */

#include "model.h"
#include "string.h"
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>

#define SOL_MAX_LINELEN 1024

Var::Var(const char* _name, VarType _type, const Rational& _lb, const Rational& _ub, const Rational& _obj):name(_name), type(_type), lb(_lb), ub(_ub), objCoef(_obj) {}

bool Var::checkBounds(const Rational& boundTolerance) const
{
   Rational absx(value);
   absx.abs();
   /* compute lb tolerance */
   Rational abslb(lb);
   abslb.abs();
   Rational lbtol(1);
   max(lbtol, lbtol, abslb);
   max(lbtol, lbtol, absx);
   lbtol *= boundTolerance;
   /* compute ub tolerance */
   Rational absub(ub);
   absub.abs();
   Rational ubtol(1);
   max(ubtol, ubtol, absub);
   max(ubtol, ubtol, absx);
   ubtol *= boundTolerance;
   /* compute relaxed bounds */
   Rational relaxedLb(lb);
   relaxedLb -= lbtol;
   Rational relaxedUb(ub);
   relaxedUb += ubtol;
   if( ((value < relaxedLb) || (value > relaxedUb)) && (type != SEMICONTINUOUS || !value.isZero()) )
   {
      printf("Failed check for var bound: ");
      print();
      return false;
   }
   return true;
}

bool Var::checkIntegrality(const Rational& intTolerance) const
{
   if( type != CONTINUOUS && type != SEMICONTINUOUS && !value.isInteger(intTolerance) )
   {
      printf("Failed check for var integrality: ");
      print();
      return false;
   }
   return true;
}

void Var::boundsViolation(Rational& boundViol) const
{
   Rational lbViol;
   Rational ubViol;
   if( type == SEMICONTINUOUS && value.isZero() )
   {
      lbViol.toZero();
      ubViol.toZero();
   }
   else
   {
      sub(lbViol, lb, value);
      if( lbViol.isNegative() )
         lbViol.toZero();
      sub(ubViol, value, ub);
      if( ubViol.isNegative() )
         ubViol.toZero();
   }
   max(boundViol, lbViol, ubViol);
}

void Var::integralityViolation(Rational& intViol) const
{
   intViol.toZero();
   if( type == CONTINUOUS || type == SEMICONTINUOUS )
      return;
   value.integralityViolation(intViol);
}

void Var::print() const
{
   std::string vartype;
   switch(type)
   {
      case BINARY:
         vartype = "binary";
         break;
      case INTEGER:
         vartype = "integer";
         break;
      case SEMICONTINUOUS:
         vartype = "semi-continuous";
      case CONTINUOUS:
         vartype = "continuous";
         break;
      default:
         vartype = "(unknown)";
   }
   printf("%s [%f,%f]", name.c_str(), lb.toDouble(), ub.toDouble());
   printf(" %s. Value %f\n", vartype.c_str(), value.toDouble());
}

Constraint::Constraint(const char* _name)
   :name(_name), type("<unknown>") {}

LinearConstraint::LinearConstraint(const char* _name, LinearType _lintype, const Rational& _lhs, const Rational& _rhs)
   :Constraint(_name), lintype(_lintype), lhs(_lhs), rhs(_rhs)
{
   type = "<linear>";
}

void LinearConstraint::push(Var* v, const Rational& c)
{
   vars.push_back(v);
   coefs.push_back(c);
}

bool LinearConstraint::check(const Rational& tolerance) const
{
   /* compute row activity (with its positive and negative parts) */
   Rational posact;
   Rational negact;
   for( unsigned int i = 0; i < vars.size(); ++i )
   {
      Rational prod;
      mult(prod, coefs[i], vars[i]->value);
      if( prod.isPositive() )
         posact += prod;
      else
         negact += prod;
   }
   Rational activity;
   activity += posact;
   activity += negact;
   /* check lhs, tolerance is: tolerance * max {pospart, negpart, |lhs|, 1} */
   Rational abslhs(lhs);
   abslhs.abs();
   Rational lhstol(1);
   max(lhstol, lhstol, posact);
   max(lhstol, lhstol, negact);
   max(lhstol, lhstol, abslhs);
   lhstol *= tolerance;
   Rational relaxedLhs(lhs);
   relaxedLhs -= lhstol;
   /* check rhs, tolerance is: tolerance * max {pospart, negpart, |rhs|, 1} */
   Rational absrhs(rhs);
   absrhs.abs();
   Rational rhstol(1);
   max(rhstol, rhstol, posact);
   max(rhstol, rhstol, negact);
   max(rhstol, rhstol, absrhs);
   rhstol *= tolerance;
   Rational relaxedRhs(rhs);
   relaxedRhs += rhstol;
   if( activity < relaxedLhs || activity > relaxedRhs )
   {
      printf("Failed check for linear cons %s: %f not in [%f,%f]\n", name.c_str(), activity.toDouble(), relaxedLhs.toDouble(), relaxedRhs.toDouble());
      return false;
   }
   return true;
}

void LinearConstraint::violation(Rational& viol) const
{
   /* compute row activity */
   Rational activity;
   for( unsigned int i = 0; i < vars.size(); ++i )
      activity.addProduct(coefs[i], vars[i]->value);
   /* check lhs and rhs */
   Rational lhsViol;
   Rational rhsViol;
   sub(lhsViol, lhs, activity);
   if( lhsViol.isNegative() )
      lhsViol.toZero();
   sub(rhsViol, activity, rhs);
   if( rhsViol.isNegative() )
      rhsViol.toZero();
   max(viol, lhsViol, rhsViol);
}

void LinearConstraint::print() const
{
   printf("%s %s. %f <=", name.c_str(), type.c_str(), lhs.toDouble());
   for( unsigned int i = 0; i < vars.size(); ++i )
   {
      printf(" %f %s", coefs[i].toDouble(), vars[i]->name.c_str());
   }
   printf(" <= %f", rhs.toDouble());
   printf("\n");
}

SOSConstraint::SOSConstraint(const char* _name, SOSType _sostype)
   :Constraint(_name), sostype(_sostype)
{
   type = "<SOS>";
}

void SOSConstraint::push(Var* v)
{
   vars.push_back(v);
}

bool SOSConstraint::check(const Rational& tolerance) const
{
   switch(sostype)
   {
      case TYPE_1:
         return checkType1(tolerance);
      case TYPE_2:
         return checkType2(tolerance);
      default :
         return false;
   }
   return false;
}

void SOSConstraint::violation(Rational& viol) const
{
   viol.toZero();
}

void SOSConstraint::print() const
{
   printf("%s %s", name.c_str(), type.c_str());
   if( sostype == TYPE_1 )
      printf("1.");
   else
      printf("2.");
   for( unsigned int i = 0; i < vars.size(); ++i )
      printf(" %s", vars[i]->name.c_str());
   printf("\n");
}

bool SOSConstraint::checkType1(const Rational& tolerance) const
{
   int cnt = 0;
   Rational lb;
   Rational ub;
   lb -= tolerance;
   ub += tolerance;
   /* count number of non-zero variables */
   for( unsigned int i = 0; i < vars.size(); ++i )
   {
      if( (vars[i]->value < lb) || (vars[i]->value > ub) )
         cnt++;
   }
   if( cnt >= 2)
   {
      printf("Failed check for sos1 cons %s:\n", name.c_str());
      return false;
   }
   else
      return true;
}

bool SOSConstraint::checkType2(const Rational& tolerance) const
{
   int cnt = 0;
   Rational lb;
   Rational ub;
   lb -= tolerance;
   ub += tolerance;
   unsigned int firstIndex = -1;
   /* count number of non-zero variables */
   for( unsigned int i = 0; i < vars.size(); ++i )
   {
      if( (vars[i]->value < lb) || (vars[i]->value > ub) )
      {
         cnt++;
         if( firstIndex == -1 )
            firstIndex = i;
      }
   }
   /* check if var in position (firstIndex + 1) is non-zero */
   if( cnt <= 1 || (cnt ==2 && (vars[firstIndex + 1]->value < lb) || (vars[firstIndex + 1]->value > ub)) )
      return true;
   else
   {
      printf("Failed check for sos2 cons %s:\n", name.c_str());
      return false;
   }
}

IndicatorConstraint::IndicatorConstraint(const char* _name, Var* _ifvar, bool _ifvalue, Constraint* _thencons)
   :Constraint(_name), ifvar(_ifvar), ifvalue(_ifvalue), thencons(_thencons)
{
   type = "<IND>";
}

IndicatorConstraint::~IndicatorConstraint()
{
   delete thencons;
}

bool IndicatorConstraint::check(const Rational& tolerance) const
{
   Rational half(1,2);
   if( ifvar->value > half && !ifvalue )
      return true;
   if( ifvar->value < half && ifvalue )
      return true;
   if( thencons->check(tolerance) )
      return true;
   printf("Failed check for indicator cons %s:\n", name.c_str());
   return false;
}

void IndicatorConstraint::violation(Rational& viol) const
{
   viol.toZero();
   Rational half(1,2);
   if( ifvar->value > half && !ifvalue )
      return;
   if( ifvar->value < half && ifvalue )
      return;
   thencons->violation(viol);
}

void IndicatorConstraint::print() const
{
   printf("%s %s. %s == %d -> ", name.c_str(), type.c_str(), ifvar->name.c_str(), ifvalue);
   thencons->print();
}

Model::Model():objSense(MINIMIZE), hasObjectiveValue(false) {}

Model::~Model()
{
   /* delete constraints */
   std::map<std::string, Constraint*>::iterator citr = conss.begin();
   std::map<std::string, Constraint*>::iterator cend = conss.end();
   while( citr != cend )
   {
      delete citr->second;
      ++citr;
   }
   conss.clear();

   /* delete vars */
   std::map<std::string, Var*>::iterator vitr = vars.begin();
   std::map<std::string, Var*>::iterator vend = vars.end();
   while( vitr != vend )
   {
      delete vitr->second;
      ++vitr;
   }
   vars.clear();
}

Var* Model::getVar(const char* name) const
{
   std::map<std::string, Var*>::const_iterator itr = vars.find(name);
   if( itr != vars.end() )
      return itr->second;
   return NULL;
}

Constraint* Model::getCons(const char* name) const
{
   std::map<std::string, Constraint*>::const_iterator itr = conss.find(name);
   if( itr != conss.end() )
      return itr->second;
   return NULL;
}

void Model::pushVar(Var* var)
{
   assert( var != NULL );
   vars[var->name] = var;
}

void Model::pushCons(Constraint* cons)
{
   assert( cons != NULL );
   conss[cons->name] = cons;
}

void Model::removeCons(const char* name)
{
   conss.erase(name);
}

unsigned int Model::numVars() const
{
   return vars.size();
}

unsigned int Model::numConss() const
{
   return conss.size();
}

bool Model::readSol(const char* filename)
{
   assert( filename != NULL );
   char buf[SOL_MAX_LINELEN];
   int nunexpectedvars = 0;

   FILE* fp = fopen(filename, "r");
   if( fp == NULL )
   {
      printf("cannot open file <%s> for reading\n", filename);
      return false;
   }

   hasObjectiveValue = false;
   bool hasVarValue = false;
   bool isSolFeas = true;

   while(true)
   {
      /* clear buffer content */
      memset((void*)buf, 0, SOL_MAX_LINELEN);
      if (NULL == fgets(buf, sizeof(buf), fp))
         break;

      /* Normalize white spaces in line */
      unsigned int len = strlen(buf);
      for( unsigned int i = 0; i < len; i++ )
      {
         if( (buf[i] == '\t') || (buf[i] == '\n') || (buf[i] == '\r') )
            buf[i] = ' ';
      }

      /* tokenize */
      char* nexttok;
      const char* varname = strtok_r(&buf[0], " ", &nexttok);
      if( varname == NULL )
         continue;

      if( strcmp(varname, "=infeas=") == 0 )
      {
         isSolFeas = false;
         break;
      }

      const char* valuep = strtok_r(NULL, " ", &nexttok);
      assert( valuep != NULL );

      if( strcmp(varname, "=obj=") == 0 )
      {
         /* read objective value */
         hasObjectiveValue = true;
         objectiveValue.fromString(valuep);
      }
      else
      {
         /* read variable value */
         Var* var = getVar(varname);
         if( var != NULL )
         {
            Rational value;
            value.fromString(valuep);
            var->value = value;
            hasVarValue = true;
         }
         else
         {
            ++nunexpectedvars;
            printf("unexpected variable <%s> in solution file\n", varname);
         }
      }
   }
   isSolFeas = isSolFeas && hasVarValue;

   if( nunexpectedvars > 0 )
      printf("Encountered %d unexpected variables\n", nunexpectedvars);

   fclose(fp);
   fp = NULL;

   return isSolFeas;
}

void Model::check(
      const Rational& intTolerance,
      const Rational& linearTolerance,
      bool& intFeasible,
      bool& linearFeasible,
      bool& correctObj) const
{
   /* check vars */
   intFeasible = true;
   linearFeasible = true;
   std::map<std::string, Var*>::const_iterator vitr = vars.begin();
   std::map<std::string, Var*>::const_iterator vend = vars.end();
   while( vitr != vend && intFeasible && linearFeasible )
   {
      linearFeasible &= vitr->second->checkBounds(linearTolerance);
      intFeasible &= vitr->second->checkIntegrality(intTolerance);
      ++vitr;
   }

   /* check constraints */
   std::map<std::string, Constraint*>::const_iterator citr = conss.begin();
   std::map<std::string, Constraint*>::const_iterator cend = conss.end();
   while( citr != cend && linearFeasible )
   {
      linearFeasible &= citr->second->check(linearTolerance);
      ++citr;
   }

   /* check objective function */
   correctObj = false;
   if( hasObjectiveValue )
   {
      correctObj = true;
      Rational objValPlus;
      Rational objvalMinus;

      vitr = vars.begin();
      while( vitr != vend )
      {
         Rational prod;

         mult(prod, vitr->second->objCoef, vitr->second->value);
         if( prod.isPositive() )
            objValPlus += prod;
         else
            objvalMinus += prod;
         ++vitr;
      }

      Rational objVal(objConstant);
      objVal += objValPlus;
      objVal += objvalMinus;

      Rational absobj(objectiveValue);
      absobj.abs();
      Rational objtol(1);
      max(objtol, objtol, objValPlus);
      max(objtol, objtol, objvalMinus);
      max(objtol, objtol, absobj);
      objtol *= linearTolerance;

      Rational diff;
      sub(diff, objVal, objectiveValue);
      diff.abs();
      if( diff > objtol )
      {
         correctObj = false;
         printf("Failed check for objective value: %f != %f\n", objectiveValue.toDouble(), objVal.toDouble());
      }
   }
}

void Model::maxViolations(
      Rational& intViol,
      Rational& linearViol,
      Rational& objViol) const
{
   /* check vars */
   intViol.toZero();
   linearViol.toZero();
   objViol.toZero();
   std::map<std::string, Var*>::const_iterator vitr = vars.begin();
   std::map<std::string, Var*>::const_iterator vend = vars.end();
   while( vitr != vend )
   {
      Rational viol;
      vitr->second->boundsViolation(viol);
      max(linearViol, viol, linearViol);
      vitr->second->integralityViolation(viol);
      max(intViol, viol, intViol);
      ++vitr;
   }
   /* check constraints */
   std::map<std::string, Constraint*>::const_iterator citr = conss.begin();
   std::map<std::string, Constraint*>::const_iterator cend = conss.end();
   while( citr != cend )
   {
      Rational viol;
      citr->second->violation(viol);
      max(linearViol, viol, linearViol);
      ++citr;
   }
   /* check objective */
   if( hasObjectiveValue )
   {
      Rational objVal(objConstant);
      vitr = vars.begin();
      while( vitr != vend )
      {
         objVal.addProduct(vitr->second->objCoef, vitr->second->value);
         ++vitr;
      }
      sub(objViol, objVal, objectiveValue);
      objViol.abs();
   }
}

void Model::print() const
{
   std::string sense;
   if( objSense == MINIMIZE )
      sense = "Minimize ";
   else if( objSense == MAXIMIZE )
      sense = "Maximize ";
   printf("Model: %s, %s\n", modelName.c_str(), sense.c_str());

   printf("Obj: objName:");
   std::map<std::string, Var*>::const_iterator vitr = vars.begin();
   std::map<std::string, Var*>::const_iterator vend = vars.end();
   while( vitr != vend )
   {
      printf(" %s %s", vitr->second->objCoef.toString().c_str(), vitr->second->name.c_str());
      ++vitr;
   }
   printf("\n");

   printf("Constraints\n");
   std::map<std::string, Constraint*>::const_iterator citr = conss.begin();
   std::map<std::string, Constraint*>::const_iterator cend = conss.end();
   while( citr != cend )
   {
      citr->second->print();
      ++citr;
   }
   printf("\n");

   printf("Variables\n");
   vitr = vars.begin();
   vend = vars.end();
   while( vitr != vend )
   {
      vitr->second->print();
      ++vitr;
   }
   printf("\n");
}

void Model::printSol() const
{
   printf("Solution:\n");
   std::map<std::string, Var*>::const_iterator vitr = vars.begin();
   std::map<std::string, Var*>::const_iterator vend = vars.end();
   while( vitr != vend )
   {
      printf("%s = %f\n", vitr->second->name.c_str(), vitr->second->value.toDouble());
      ++vitr;
   }
}
