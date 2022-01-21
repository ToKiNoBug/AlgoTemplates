/*
 Copyright © 2022  TokiNoBug
This file is part of OptimTemplates.

    OptimTemplates is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OptimTemplates is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OptimTemplates.  If not, see <https://www.gnu.org/licenses/>.

*/

#ifndef PSO_HPP
#define PSO_HPP
#include "PSOOption.hpp"
#include "PSOBase.hpp"
#include <array>
#include <vector>
#include <tuple>

#ifndef EIGEN_CORE_H
#ifdef OptimT_PSO_USE_EIGEN
#error You must include Eigen before you define OptimT_PSO_USE_EIGEN! Include Eigen before OptimT.
#endif
#endif

namespace OptimT {

template<class Var_t,   //Var_t must support operator[]
         size_t Dim,
         DoubleVectorOption VecType,
         FitnessOption FitnessOpt,
         RecordOption RecordOpt,
         class ...Args>
class PSO : public PSOBase<Var_t,double,RecordOpt,Args...>
{
public:
    using Base_t = PSOBase<Var_t,double,RecordOpt,Args...>;
    OPTIMT_MAKE_PSOBASE_TYPES

    virtual void setPVRange(double pMin,double pMax,double vMax) {
        for(size_t i=0;i<Dim;i++) {
            Base_t::_posMin[i]=pMin;
            Base_t::_posMax[i]=pMax;
            Base_t::_velocityMax[i]=vMax;
        }
    }

    virtual double bestFitness() const {
        return Base_t::gBest.fitness;
    }

protected:
    static bool isBetterThan(double a,double b) {
        if(FitnessOpt==FitnessOption::FITNESS_GREATER_BETTER) {
            return a>b;
        } else {
            return a<b;
        }
    }

    virtual void updatePGBest() {
        Point_t * curGBest=Base_t::_population.data();
        for(Particle_t & i : Base_t::_population) {
            if(Base_t::_generation<=1) {
                this->gBest.fitness=(FitnessOpt==FITNESS_GREATER_BETTER)?(ninfD):(pinfD);
                i.pBest=i;
            }
            else {
                if(isBetterThan(i.fitness,i.pBest.fitness)) {
                    i.pBest=i;
                }
            }

            if(isBetterThan(i.pBest.fitness,curGBest->fitness)) {
                curGBest=&i.pBest;
            }
        }

            if(isBetterThan(curGBest->fitness,Base_t::gBest.fitness)) {
                Base_t::_failTimes=0;
                Base_t::gBest=*curGBest;
            }
            else {
                Base_t::_failTimes++;
            }
    }

    virtual void updatePopulation() {
        for(Particle_t & i : Base_t::_population) {
            for(size_t idx=0;idx<Dim;idx++) {
                i.velocity[idx]=
                        Base_t::_option.inertiaFactor*i.velocity[idx]
                        +Base_t::_option.learnFactorP*randD()*(i.pBest.position[idx]-i.position[idx])
                        +Base_t::_option.learnFactorG*randD()*(Base_t::gBest.position[idx]-i.position[idx]);
                if(std::abs(i.velocity[idx])>Base_t::_velocityMax[idx]) {
                    i.velocity[idx]=sign(i.velocity[idx])*Base_t::_velocityMax[idx];
                }
                i.position[idx]+=i.velocity[idx];
                i.position[idx]=std::max(i.position[idx],Base_t::_posMin[idx]);
                i.position[idx]=std::min(i.position[idx],Base_t::_posMax[idx]);
            }
            i.setUncalculated();
        }
    }

};

template<size_t Dim,
        FitnessOption FitnessOpt,
         RecordOption RecordOpt,
         class ...Args>
using PSO_std = PSO<std::array<double,Dim>,Dim,StdArray,FitnessOpt,RecordOpt,Args...>;


#ifdef OptimT_PSO_USE_EIGEN
template<size_t Dim,
        FitnessOption FitnessOpt,
         RecordOption RecordOpt,
         class ...Args>
using PSO_Eigen = PSO<Eigen::Array<double,Dim,1>,Dim,EigenArray,FitnessOpt,RecordOpt,Args...>;

template<
         size_t Dim,
         FitnessOption FitnessOpt,
         RecordOption RecordOpt,
         class ...Args>
class PSO<Eigen::Array<double,Dim,1>,Dim,EigenArray,FitnessOpt,RecordOpt,Args...>
{
public:
    using Base_t = PSOBase<Eigen::Array<double,Dim,1>,double,RecordOpt,Args...>;
    OPTIMT_MAKE_PSOBASE_TYPES

    virtual void setPVRange(double pMin,double pMax,double vMax) {
        this->_posMin.setConstant(pMin);
        this->_posMax.setConstant(pMax);
        this->_velocityMax.setConstant(vMax);
    }

    virtual double bestFitness() const {
        return Base_t::gBest.fitness;
    }

protected:
    static bool isBetterThan(double a,double b) {
        if(FitnessOpt==FitnessOption::FITNESS_GREATER_BETTER) {
            return a>b;
        } else {
            return a<b;
        }
    }

    virtual void updatePGBest() {
        Point_t * curGBest=Base_t::_population.data();
        for(Particle_t & i : Base_t::_population) {
            if(Base_t::_generation<=1) {
                this->gBest.fitness=(FitnessOpt==FITNESS_GREATER_BETTER)?(ninfD):(pinfD);
                i.pBest=i;
            }
            else {
                if(isBetterThan(i.fitness,i.pBest.fitness)) {
                    i.pBest=i;
                }
            }

            if(isBetterThan(i.pBest.fitness,curGBest->fitness)) {
                curGBest=&i.pBest;
            }
        }

            if(isBetterThan(curGBest->fitness,Base_t::gBest.fitness)) {
                Base_t::_failTimes=0;
                Base_t::gBest=*curGBest;
            }
            else {
                Base_t::_failTimes++;
            }
    }

    virtual void updatePopulation() {
        for(Particle_t & i : Base_t::_population) {
            i.velocity=this->_option.interiaFactor*i.velocity
                        +this->_option.learnFactorP*randD()*(i.pBest.position-i.position)
                        +this->_option.learnFactorG*randD()*(this->gBest.position-i.position);

            i.velocity=i.velocity.min(Base_t::_velocityMax);
            i.velocity=i.velocity.max(-Base_t::_velocityMax);

            i.position+=i.velocity;

            i.position=i.position.min(Base_t::_posMax);
            i.position=i.position.max(Base_t::_posMin);
            
            i.setUncalculated();
        }
    }

};
#endif // OptimT_PSO_USE_EIGEN

}

#endif // PSO_HPP
