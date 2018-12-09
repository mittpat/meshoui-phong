#include <cstdio>
#define EXPECT_EQ(ref, value) if (ref != value) { fprintf(stderr, "Assertion failed: %f != %f at %s:%d.", (double)ref, (double)value, __FILE__, __LINE__); return 1; }
#define EXPECT_EQ_TOL(ref, value, tol) if (fabs(ref - value) > tol) { fprintf(stderr, "Assertion failed: %f != %f (tol:%f) at %s:%d.", (double)ref, (double)value, (double)tol, __FILE__, __LINE__); return 1; }
#define EXPECT_EQ_S(ref, value) if (ref != value) { fprintf(stderr, "Assertion failed: %s != %s at %s:%d.", std::string(ref).c_str(), value.c_str(), __FILE__, __LINE__); return 1; }
#define EXPECT_NOT_EQ(ref, value) if (ref == value) { fprintf(stderr, "Assertion failed: %f == %f at %s:%d.", (double)ref, (double)value, __FILE__, __LINE__); return 1; }

#define ASSERT_EQ(ref, value) if (ref != value) { fprintf(stderr, "Assertion failed: %f != %f at %s:%d.", (double)ref, (double)value, __FILE__, __LINE__); exit(1); }
#define ASSERT_EQ_TOL(ref, value, tol) if (fabs(ref - value) > tol) { fprintf(stderr, "Assertion failed: %f != %f (tol:%f) at %s:%d.", (double)ref, (double)value, (double)tol, __FILE__, __LINE__); exit(1); }
#define ASSERT_EQ_S(ref, value) if (ref != value) { fprintf(stderr, "Assertion failed: %s != %s at %s:%d.", std::string(ref).c_str(), value.c_str(), __FILE__, __LINE__); exit(1); }
#define ASSERT_NOT_EQ(ref, value) if (ref == value) { fprintf(stderr, "Assertion failed: %f == %f at %s:%d.", (double)ref, (double)value, __FILE__, __LINE__); exit(1); }
