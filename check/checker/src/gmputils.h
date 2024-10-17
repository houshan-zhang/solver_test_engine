/**
 * @file gmputils.h
 * @brief Basic classes to for rational arithmetic
 */

#ifndef GMPUTILS_H
#define GMPUTILS_H

#include <gmp.h>
#include <string>

/**
 * @brief Simple wrapper class around GMP mpq_t.
 */
class Rational
{
   public:
      /**
       * Construct a rational corresponding to num/den.
       * Acts also as default constructor (for the value zero).
       * @param num signed integer as numerator
       * @param den signed integer as denominator
       */
      Rational(int num = 0, int den = 1);

      /**
       * Construct a rational from a double.
       * Note that the conversion is exact, so it may not be what you want...
       * @param val floating value to construct from
       */
      Rational(double frac);

      /** Copy Constructor */
      Rational(const Rational& val);

      /** Destructor */
      ~Rational();

      Rational& operator=(const Rational& val);

      /**
       * Convert to a double, truncating if necessary
       * @return the rational number as a float
       */
      double toDouble() const;

      /** equal operator */
      bool operator==(const Rational& val) const;

      /** not-equal operator */
      bool operator!=(const Rational& val) const;

      /** greater-than operator */
      bool operator>(const Rational& val) const;

      /** less-than operator */
      bool operator<(const Rational& val) const;

      /** compound plus operator: self = self + val */
      Rational& operator+=(const Rational& val);

      /** compound minus operator: self = self - val */
      Rational& operator-=(const Rational& val);

      /** compound multiplication operator: self = self * val */
      Rational& operator*=(const Rational& val);

      /**
       * Add to the current value the product val1 * val2.
       * This is the equivalent of the operation self += val1*val2
       * for standard numeric types. It is provided here because it is
       * useful for computing dot products.
       */
      void addProduct(const Rational& val1, const Rational& val2);

      /**
       * Replaces the current value with its absolute value (inline operator)
       */
      void abs();

      /**
       * Calculate the integrality violation of a given number
       */
      void integralityViolation(Rational& violation) const;

      /**
       * Zero out number (i.e. set it to zero)
       */
      void toZero();

      /** Addition: res = val1 + val2 */
      friend void add(Rational& res, const Rational& val1, const Rational& val2);

      /** Subtraction: res = val1 - val2 */
      friend void sub(Rational& res, const Rational& val1, const Rational& val2);

      /** Multiplication: res = val1 * val2 */
      friend void mult(Rational& res, const Rational& val1, const Rational& val2);

      /** Division: res = val1 / val2 */
      friend void div(Rational& res, const Rational& val1, const Rational& val2);

      /** Min: res = min(val1, val2) */
      friend void min(Rational& res, const Rational& val1, const Rational& val2);

      /** Max: res = max(val1, val2) */
      friend void max(Rational& res, const Rational& val1, const Rational& val2);

      /** Test if a number is integer (up to a given tolerance)  */
      bool isInteger(const Rational& tolerance) const;

      /** Test if a number is positive (exact, no tolerances) */
      bool isPositive() const;

      /** Test if a number is negative (exact, no tolerances) */
      bool isNegative() const;

      /** Test if a number is zero (exact, no tolerances) */
      bool isZero() const;

      /**
       * Read a rational number from a string representation.
       * This functions essentially parses a number of the form
       * [+|-]?[0-9]*.[0-9]+[[e|E][+|-][0-9]+]?
       * and generates the corresponding fraction.
       */
      void fromString(const char* str);

      /**
       * Convert a rational number to a string representation.
       * Note that the format is [+|-]?[0-9]+/[0-9]+.
       * Useful for printing.
       */
      std::string toString() const;
   protected:
      /* rational value */
      mpq_t number;
   private:
      /**
       * static buffer for I/O operations
       * if a number does not fit in here, it does not fit in a double either
       */
      static char buffer[1024];
};

#endif
