/**
 * @file gmputils.cpp
 * @brief Basic classes to for rational arithmetic
 */

#include "gmputils.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

char Rational::buffer[] = {'\0'};

Rational::Rational(int num, int den)
{
   mpq_init(number);
   mpq_set_si(number, num, den);
   mpq_canonicalize(number);
}

Rational::Rational(double frac)
{
   mpq_init(number);
   mpq_set_d(number, frac);
   mpq_canonicalize(number);
}

Rational::Rational(const Rational& val)
{
   mpq_init(number);
   mpq_set(number, val.number);
}

Rational::~Rational()
{
   mpq_clear(number);
}

Rational& Rational::operator=(const Rational& val)
{
   if (this != &val)
      mpq_set(number, val.number);
   return *this;
}

double Rational::toDouble() const
{
   return mpq_get_d(number);
}

bool Rational::operator==(const Rational& val) const
{
   return (mpq_equal(number, val.number) != 0);
}

bool Rational::operator!=(const Rational& val) const
{
   return (mpq_equal(number, val.number) == 0);
}

bool Rational::operator>(const Rational& val) const
{
   return (mpq_cmp(number, val.number) > 0);
}

bool Rational::operator<(const Rational& val) const
{
   return (mpq_cmp(number, val.number) < 0);
}

Rational& Rational::operator+=(const Rational& val)
{
   mpq_add(number, number, val.number);
   mpq_canonicalize(number);
   return *this;
}

Rational& Rational::operator-=(const Rational& val)
{
   mpq_sub(number, number, val.number);
   mpq_canonicalize(number);
   return *this;
}

Rational& Rational::operator*=(const Rational& val)
{
   mpq_mul(number, number, val.number);
   mpq_canonicalize(number);
   return *this;
}

void Rational::addProduct(const Rational& val1, const Rational& val2)
{
   mpq_t prod;
   mpq_init(prod);
   mpq_mul(prod, val1.number, val2.number);
   mpq_add(number, number, prod);
   mpq_canonicalize(number);
   mpq_clear(prod);
}

void Rational::abs()
{
   mpq_abs(number, number);
}

void Rational::integralityViolation(Rational& violation) const
{
   // if denominator is 1, then there is no integrality violation for sure
   if( mpz_cmp_ui(mpq_denref(number), 1) == 0 )
   {
      violation.toZero();
      return;
   }
   // otherwise, we must check w.r.t. the given tolerance
   // first calculate the fractional part
   violation = (*this);
   violation.abs();
   mpz_t r;
   mpz_init(r);
   mpz_fdiv_r(r, mpq_numref(violation.number), mpq_denref(violation.number));
   mpq_set_num(violation.number, r);
   mpz_clear(r);
   // then integrality violation
   if( violation > Rational(1, 2) )
      sub(violation, Rational(1,1), violation);
}

void Rational::toZero()
{
   mpq_set_ui(number, 0, 1);
}

bool Rational::isInteger(const Rational& tolerance) const
{
   // if denominator is 1, then it is an integer for sure
   if (mpz_cmp_ui(mpq_denref(number), 1) == 0) return true;
   // otherwise, we must check w.r.t. the given tolerance
   // first calculate the fractional part
   Rational viol(*this);
   viol.abs();
   mpz_t r;
   mpz_init(r);
   mpz_fdiv_r(r, mpq_numref(viol.number), mpq_denref(viol.number));
   mpq_set_num(viol.number, r);
   mpz_clear(r);
   // then integrality violation
   if( viol > Rational(1, 2) )
      sub(viol, Rational(1,1), viol);
   return !(viol > tolerance);
}

bool Rational::isPositive() const
{
   return (mpq_sgn(number) > 0);
}

bool Rational::isNegative() const
{
   return (mpq_sgn(number) < 0);
}

bool Rational::isZero() const
{
   return (mpq_sgn(number) == 0);
}

void Rational::fromString(const char* num)
{
   char* tmp = &buffer[0];
   int k = 0;
   int exponent = 0;
   int fraction = 0;

   assert(num != NULL);
   assert(strlen(num) <  32);

   // Skip initial whitespace
   while(isspace(*num))
      num++;

   // Skip initial +/-
   if (*num == '+')
      num++;
   else if (*num == '-')
      tmp[k++] = *num++;

   for(int i = 0; num[i] != '\0'; i++)
   {
      if (isdigit(num[i]))
      {
         tmp[k++]  = num[i];
         exponent -= fraction;
      }
      else if (num[i] == '.')
         fraction = 1;
      else if (tolower(num[i]) == 'e')
      {
         exponent += atoi(&num[i + 1]);
         break;
      }
   }
   while(exponent > 0)
   {
      tmp[k++] = '0';
      exponent--;
   }
   tmp[k++] = '/';
   tmp[k++] = '1';

   while(exponent < 0)
   {
      tmp[k++] = '0';
      exponent++;
   }
   tmp[k] = '\0';

   mpq_set_str(number, tmp, 10);
   mpq_canonicalize(number);
}

std::string Rational::toString() const
{
   // assure the buffer is big enough to store the result
   assert( mpz_sizeinbase(mpq_numref(number), 10 ) + mpz_sizeinbase(mpq_denref(number), 10) + 3 < 1024 );

   mpq_get_str(buffer, 10, number);
   return std::string(buffer);
}

void add(Rational& res, const Rational& val1, const Rational& val2)
{
   mpq_add(res.number, val1.number, val2.number);
   mpq_canonicalize(res.number);
}

void sub(Rational& res, const Rational& val1, const Rational& val2)
{
   mpq_sub(res.number, val1.number, val2.number);
   mpq_canonicalize(res.number);
}

void mult(Rational& res, const Rational& val1, const Rational& val2)
{
   mpq_mul(res.number, val1.number, val2.number);
   mpq_canonicalize(res.number);
}

void div(Rational& res, const Rational& val1, const Rational& val2)
{
   mpq_div(res.number, val1.number, val2.number);
   mpq_canonicalize(res.number);
}

void min(Rational& res, const Rational& val1, const Rational& val2)
{
   if( mpq_cmp(val1.number, val2.number) < 0 )
      res = val1;
   else
      res = val2;
}

void max(Rational& res, const Rational& val1, const Rational& val2)
{
   if( mpq_cmp(val1.number, val2.number) > 0 )
      res = val1;
   else
      res = val2;
}
