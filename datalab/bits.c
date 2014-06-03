/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * evenBits - return word with all even-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 1
 */
int evenBits(void) {
  /**
   * The solution should be 0x55555555. To construct 0x55555555 using 0x55,
   * we left shift 0x55 by 8 bits and '|' 0x55. Then we get 0x5555. Do similar
   * operation we will get 0x55555555.
   */
  int lower = 0x55 | (0x55 << 8);
  return (lower << 16) | lower;
}
/* 
 * isEqual - return 1 if x == y, and 0 otherwise 
 *   Examples: isEqual(5,5) = 1, isEqual(4,5) = 0
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int isEqual(int x, int y) {
  /**
   * When x == y, then x ^ y should be zero. When x != y, then x ^ y should be
   * non-zero. So !(x ^ y) will do the trick.
   */
  return !(x ^ y);
}
/* 
 * byteSwap - swaps the nth byte and the mth byte
 *  Examples: byteSwap(0x12345678, 1, 3) = 0x56341278
 *            byteSwap(0xDEADBEEF, 0, 2) = 0xDEEFBEAD
 *  You may assume that 0 <= n <= 3, 0 <= m <= 3
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 25
 *  Rating: 2
 */
int byteSwap(int x, int n, int m) {
  /**
   * We can use the XOR trick that was introduced in the Practice Problem 2.10
   * in our textbook to decrease op numbers. We know that x ^ (x ^ y) == y and 
   * y ^ (x ^ y) == x. If we get a byte that equals nth_byte ^ mth_byte, then 
   * we can use this byte to swap nth_byte and mth_byte.
   *
   * Finally, the ops decrease to 10.
   */
  int nbits = n << 3;
  int mbits = m << 3;
  int magical_byte = ((x >> nbits) ^ (x >> mbits)) & 0xff;
  return x ^ (magical_byte << nbits) ^ (magical_byte << mbits);
}
/* 
 * rotateRight - Rotate x to the right by n
 *   Can assume that 0 <= n <= 31
 *   Examples: rotateRight(0x87654321,4) = 0x18765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 25
 *   Rating: 3 
 */
int rotateRight(int x, int n) {
  /**
   * Basic solution is to right shift x by n bits to get the upper 32 - n bits,
   * left shift x by 32 - n to get the lower n bits. Then '|' this two results
   * to get the answer. But when x < 0, left shift x will cause sign extension.
   * To avoid this, I use x ^ (x >> 31) to flip all bits of a negative number.
   * Then XOR x >> 31 to flip them back.
   */
  
  int sign = x >> 31;
  int lshift_bits = 33 + (~n);
  x = x ^ sign; // flip all bits of a negative number
  return ((x >> n) | (x << lshift_bits)) ^ sign;
}
/* 
 * logicalNeg - implement the ! operator using any of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
  /**
   * Easy way to distinguish zero and non-zero is that 0 == -0 while other
   * numbers do not (Tmin is a special case). Since -x == ~x + 1, then the 
   * sign bit of (~x + 1) | x will be 1 except for x == 0 (when x == Tmin, the
   * sign bit is also 1). So ((~x + 1) | x) >> 31 will be 0 only when x == 0,
   * and -1 when x != 0. So adding 1 will finish this task.
   */
  return ((((~x) + 1) | x) >> 31) + 1;
}
/* 
 * TMax - return maximum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmax(void) {
  /**
   * This one is easy. TMax = 0x7fffffff, all bits are 1 except for the sign
   * bit. So flip 0x80000000 will do the trick.
   */
  return ~(1 << 31);
}
/* 
 * sign - return 1 if positive, 0 if zero, and -1 if negative
 *  Examples: sign(130) = 1
 *            sign(-23) = -1
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 10
 *  Rating: 2
 */
int sign(int x) {
  /**
   * x >> 31 will give us a mask: 0xffffffff (-1) for negative number and 0x0 
   * for non-negative number. This mask helps us to find all negative numbers.
   * !!x will help us to distinguish zero and non-zeros. Since we have find all
   * negative numbers, we use !!x to distinguish positive number and zero.
   */
  return (x >> 31) | (!!(x));
}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
  /**
   * First we test whether the signs of x and y are equal or not. 
   * If signs are not equal, then if x >= 0 we should return 1, otherwise 0.
   * If signs are equal, we do x - y and test the sign of the result.
   */
  int sign_nequal = (x ^ y) >> 31; // 0 when sign equals
  return (sign_nequal & (!(x >> 31))) | 
         ((~sign_nequal) & (((x + (~y)) >> 31) + 1));
}
/* 
 * subOK - Determine if can compute x-y without overflow
 *   Example: subOK(0x80000000,0x80000000) = 1,
 *            subOK(0x80000000,0x70000000) = 0, 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int subOK(int x, int y) {
  /**
   * x - y overflow condition:
   * (x < 0 && y >= 0 && x - y >= 0) or (x >= 0 && y < 0 && x - y < 0)
   * I use ~y + 1 to represent -y, so x - y == x + (~y + 1), and the overflow
   * condition can be explained as:
   * sign of x != sign of y and sign of x != sign of x - y
   * 
   * When y == Tmin, x - y == x - Tmin. Since (~Tmin + 1) == Tmin == -Tmin, we
   * get x - Tmin == x + (~Tmin + 1). So Tmin will not be a special case.
   */
  int dif = x + (~y + 1);
  return !(((x ^ y) & (x ^ dif)) >> 31);
}
/*
 * satAdd - adds two numbers but when positive overflow occurs, returns
 *          maximum possible value, and when negative overflow occurs,
 *          it returns minimum possible value.
 *   Examples: satAdd(0x40000000,0x40000000) = 0x7fffffff
 *             satAdd(0x80000000,0xffffffff) = 0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 30
 *   Rating: 4
 */
int satAdd(int x, int y) {
  /**
   * x + y overflow condition:
   * (x < 0 && y < 0 && x + y >= 0) or (x >= 0 && y >= 0 && x + y < 0)
   * This means that if signs of x != sign of x + y and signs of y != sign of  
   * x + y, x + y will cause overflow.
   * When overflow happens, we should return 0x80000000 or 0x7fffffff, which
   * depends on the sign of x + y. We use 0x80000000 ^ sum_sign to represent 
   * the result.
   */
  int sum = x + y;
  int s_sign = sum >> 31;
  int Tmin = 1 << 31;
  int overflow = ((x ^ sum) & (y ^ sum)) >> 31;
  return (overflow & (Tmin ^ s_sign)) | (~overflow & sum);
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
  /**
   * We can use the Binary Search Algorithm to search for the position of the 
   * highest '1' bit: Firstly, turn x into positive. Then test whether the 
   * upper 16 bits is zero. If true, we know the highest bit locates at 
   * lower 16 bits. If false, the highest bit locates at upper 16 bits, we 
   * right shifts x by 16 bits and add 16 to the position recording number.
   * Repeat this procedure for 8 bits, 4 bits and so on till we find the 
   * highest bit. Finally, we add 1 sign bit to the number and get the result.
   */
  int x_sign = x >> 31;
  // the number of bits are equal for (-1 and 0), (-2 and 1) ... So flip all
  // bits of a negative number
  int val = x ^ x_sign;
  // position recording, 1 for the sign bit
  int number = 1;

  // test upper 16 bits
  int upper = val >> 16;
  int upper_is_zero = !upper;
  // if upper 16 bits is not zero, right shift x by 16 bits
  int shift = (!upper_is_zero) << 4;
  number = number + shift;  // position add 16 or 0
  val = val >> shift;

  // similar to the test of upper 16 bits, we test the upper 8 bits
  upper = val >> 8;
  upper_is_zero = !upper;
  shift = (!upper_is_zero) << 3;
  number = number + shift;
  val = val >> shift;

  // upper 4 bits
  upper = val >> 4;
  upper_is_zero = !upper;
  shift = (!upper_is_zero) << 2;
  number = number + shift;
  val = val >> shift;

  // upper 2 bits
  upper = val >> 2;
  upper_is_zero = !upper;
  shift = (!upper_is_zero) << 1;
  number = number + shift;
  val = val >> shift;
            
  // final bit
  upper = val >> 1;
  number = number + upper;
  val = val >> upper;
  number = number + val;

  return number;
}
/* 
 * float_half - Return bit-level equivalent of expression 0.5*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_half(unsigned uf) {
  /**
   * Test exponent. First make sure uf is not NaN or Infinity. Then if uf is
   * normalized number and exponent > 1, just decrease exponent by 1. If
   * exponent is 1 or if uf is denormalized, we should shift fraction part, set
   * exp to 0 and round the result.
   */
  unsigned int exponent = uf & 0x7f800000;
  if (exponent != 0x7f800000) { // uf is not NaN
    if (exponent > 0x00800000) { // normalized and exp > 1
      uf = uf - 0x00800000; // decrease e by 1
    }
    else { // denormalized or exponent == 1
      /**
       * In this situation, we need to right shift fraction by 1 either to
       * divide uf by 2 (when exp == 0) or add the implied 1 (when exp == 1).
       * So we will lose 1 bit precision. When the last two bits of fraction
       * is 11, we need to round the result.
       */
      unsigned int round = (uf & 3) == 3;
      /**
       * When exp == 0, uf & 0xffffff == uf & 0x7fffff == frac, so right 
       * shifting by 1 represents dividing by 2. 
       * When exp == 1, uf & 0xffffff == 0x800000 + uf & 0x7fffff == 1 + frac,
       * so right shifting by 1 represents adding the implied 1.
       */
      uf = (uf & 0x80000000) | (((uf & 0xffffff) >> 1) + round);
    }
  }
  return uf;
}
/* 
 * float_f2i - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int float_f2i(unsigned uf) {
  /**
   * In this puzzle, we should test the exp of the float number.
   * 1. If exponent < 127, the E of the float number is negative. So the result
   * will be less than 1, and the conversion result should be 0.
   * 2. If exponent > 157, the E of the float number is larger than 30. Since we
   * should right shift the decimal point of the mantissa by E, large E will
   * cause overflow. So the conversion result should be 0x80000000.
   * 3. If 127 <= exponent <= 157, the E of the float number is suitable. We can
   * get E by E = exp - 127. Because the fraction part is 23 bits long, we
   * compare E with 23 to determine the shifting direction of mantissa.
   */
  unsigned int sign = uf >> 31;
  unsigned int exponent = (uf & 0x7f800000) >> 23;
  unsigned int mantissa = (uf & 0x7fffff) | 0x800000;
  if (exponent < 127) return 0; // uf < 1, integer should be 0
  else if (exponent > 157) return 0x80000000; // cause overflow
  else {
    exponent = exponent - 127;
    if (exponent < 23) {
      exponent = 23 - exponent;
      mantissa = mantissa >> exponent;
    }
    else {
      exponent = exponent - 23;
      mantissa = mantissa << exponent;
    }
    if (sign) return -mantissa;
    else return mantissa;
  }
}
