#ifndef DEF_TESTINGPSO_H
#define DEF_TESTINGPSO_H

#include <Eigen/Dense>
#define OptimT_NO_OUTPUT
#define OptimT_PSO_USE_EIGEN
#include <OptimTemplates/PSO>

void testPSOBase();

void testRastriginFun();

#endif // DEF_TESTINGPSO_H
