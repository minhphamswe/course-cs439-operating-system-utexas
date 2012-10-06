#ifndef PINTOS_THREADS_FIXEDPOINT_H_
#define PINTOS_THREADS_FIXEDPOINT_H_

#define Q 12          // Number of bits after the floating point
#include <stdint.h>

enum {  F = (1 << Q) };   // Conversion factor
enum { HF = F/2 };       // Half of F (used for rounding)

// Conversion float -> int and int -> float
inline int Float(int i);
inline int Round0(int f);
inline int Round(int f);

// Arithmetic between 2 floats
inline int AddF(int f1, int f2);
inline int SubF(int f1, int f2);
inline int MulF(int f1, int f2);
inline int DivF(int f1, int f2);

// Arithmetic between a float and an int
inline int AddI(int f, int i);
inline int SubI(int f, int i);
inline int MulI(int f, int i);
inline int DivI(int f, int i);

// Convert an integer to a float
inline int Float (int i) {
  return i * F;
}

// Round a float to 0, returning an integer (note this is not rounding down)
inline int Round0 (int f) {
  return f / F;
}

// Round a float to the nearest integer
inline int Round (int f) {
  return ((f > 0) ? (Round0(f + HF)) : (Round0(f - HF)));
}

// Add two floats, returning a float
inline int AddF (int f1, int f2) {
  return f1 + f2;
}

// Subtract one float from another float, returning a float
inline int SubF (int f1, int f2) {
  return f1 - f2;
}

// Multiply two floats, returning a float
inline inline int MulF (int f1, int f2) {
  return ((int64_t) f1) * f2 / F;
}

// Divide one float by another float, returning a float
inline int DivF (int f1, int f2) {
  return ((int64_t) f1) * F / f2;
}

// Add an int to a float, returning a float
inline int AddI (int f, int i) {
  return AddF(f, Float(i));
}

// Subtract an int from a float, returning a float
inline int SubI (int f, int i) {
  return SubF(f, Float(i));
}

// Multiply a float by an int, returning a float
inline int MulI (int f, int i) {
  return f * i;
}

// Subtract a float by an int, returning a float
inline int DivI (int f, int i) {
  return f / i;
}



#endif