// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2022 Shawn Li <tokinobug@163.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef Heu_NSGA3_HPP
#define Heu_NSGA3_HPP

#include "NSGA3Base.hpp"

namespace Heu {
/*
template<typename Var_t,
        size_t ObjNum,
        RecordOption rOpt=DONT_RECORD_FITNESS,
        PFOption pfOpt=PARETO_FRONT_CAN_MUTATE,
        ReferencePointOption rpOpt=ReferencePointOption::SINGLE_LAYER,
        class Args_t=void,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::initializeFun _iFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::fitnessFun _fFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::crossoverFun _cFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::mutateFun _mFun_=nullptr>
class NSGA3 : public internal::NSGA3Base<Var_t,ObjNum,rOpt,pfOpt,rpOpt,Args_t,
            _iFun_,_fFun_,_cFun_,_mFun_>
{
public:
    NSGA3() {};
    virtual ~NSGA3() {};
    using Base_t = internal::NSGA3Base<Var_t,ObjNum,rOpt,pfOpt,rpOpt,Args_t,
        _iFun_,_fFun_,_cFun_,_mFun_>;
    Heu_MAKE_NSGA3ABSTRACT_TYPES

    virtual Fitness_t bestFitness() const {
        Fitness_t best=this->_population.front()._Fitness;
        for(const Gene * i : this->_pfGenes) {
            for(size_t objIdx=0;objIdx<i->_Fitness.size();objIdx++) {
                best[objIdx]=std::min(best[objIdx],i->_Fitness[objIdx]);
            }
        }
        return best;
    }

    
    void initializePop() {
        this->makeReferencePoses();
        Base_t::initializePop();
    }
};  //  NSGA3

*/

/**
 * @brief Parital specialization for NSGA3 using Eigen's array as fitness values
 */
template<typename Var_t,
        size_t ObjNum,
        RecordOption rOpt=DONT_RECORD_FITNESS,
        PFOption pfOpt=PARETO_FRONT_CAN_MUTATE,
        ReferencePointOption rpOpt=ReferencePointOption::SINGLE_LAYER,
        class Args_t=void,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::initializeFun _iFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::fitnessFun _fFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::crossoverFun _cFun_=nullptr,
         typename internal::GAAbstract<Var_t,EigenVecD_t<ObjNum>,Args_t>::mutateFun _mFun_=nullptr>
class NSGA3
        : public internal::NSGA3Base<Var_t,ObjNum,rOpt,pfOpt,rpOpt,Args_t,
            _iFun_,_fFun_,_cFun_,_mFun_>
{
public:
    using Base_t = internal::NSGA3Base<Var_t,ObjNum,rOpt,pfOpt,rpOpt,Args_t,
        _iFun_,_fFun_,_cFun_,_mFun_>;
    Heu_MAKE_NSGA3ABSTRACT_TYPES

    virtual Fitness_t bestFitness() const {
        Fitness_t best=this->_population.front()._Fitness;
        for(const Gene * i : this->_pfGenes) {
            best=i->_Fitness.min(best);
        }
        return best;
    }

    
    void initializePop() {
        this->makeReferencePoses();
        Base_t::initializePop();
    }
private:
    template<size_t HeuSize>
    struct HeuSize2EigenSize
    {
        static const constexpr int64_t value=(HeuSize==Runtime)?(Eigen::Dynamic):(HeuSize);
    };
protected:
    static const int64_t eigSizeFlag=HeuSize2EigenSize<ObjNum>::value;

    virtual void normalize(const std::unordered_set<infoUnit3*> & selected,
        const std::vector<infoUnit3*> & Fl) const {

        const size_t M=this->objectiveNum();
        stdContainer<const infoUnit3*,ObjNum> extremePtrs;
        initializeSize<ObjNum>::template resize<decltype(extremePtrs)>(&extremePtrs,M);
        
        Eigen::Array<double,eigSizeFlag,eigSizeFlag> extremePoints;
        
        Fitness_t ideal,intercepts;
        
        extremePoints.resize(M,M);
        ideal.resize(M);
        intercepts.resize(M);
        
        ideal.setConstant(pinfD);

        for(size_t c=0;c<M;c++) {
            extremePtrs[c]=*(Fl.begin());
            for(size_t r=0;r<M;r++) {
                extremePoints(r,c)=extremePtrs[c]->iterator->_Fitness[r];
            }
        }

        for(auto i : selected) {
            ideal=ideal.min(i->iterator->_Fitness);
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                if(i->iterator->_Fitness[objIdx]>extremePtrs[objIdx]->iterator->_Fitness[objIdx]) {
                    extremePtrs[objIdx]=i;
                }
            }
        }

        for(auto i : Fl) {
            ideal=ideal.min(i->iterator->_Fitness);
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                if(i->iterator->_Fitness[objIdx]>extremePtrs[objIdx]->iterator->_Fitness[objIdx]) {
                    extremePtrs[objIdx]=i;
                }
            }
        }

        for(size_t c=0;c<M;c++) {
            extremePoints.col(c)=extremePtrs[c]->iterator->_Fitness-ideal;
        }

        bool isSingular;

        {
            std::unordered_set<const void *> set;
            set.reserve(M);
            for(auto i : extremePtrs) {
                set.emplace(i);
            }

            isSingular=(set.size()<M);
        }

        if(isSingular) {
            for(size_t r=0;r<M;r++) {
                intercepts[r]=extremePoints(r,r);
            }
        }
        else {
            extremePoints2Intercept(extremePoints,&intercepts);
        }

        for(auto i : selected) {
            i->translatedFitness=(i->iterator->_Fitness-ideal)/intercepts;
        }

        for(auto i : Fl) {
            i->translatedFitness=(i->iterator->_Fitness-ideal)/intercepts;
        }
    }

    size_t findNearest(const Fitness_t & s,double * dist) const {
        
        const auto & w=this->referencePoses;
        auto wT_s=w.matrix().transpose()*s.matrix();
        auto wT_s_w=w.rowwise()*(wT_s.array().transpose());
        Eigen::Array<double,eigSizeFlag,Eigen::Dynamic> norm_wTsw=wT_s_w.rowwise()/(w.colwise().squaredNorm());
        auto s_sub_norm_wTsw=norm_wTsw.colwise()-s;
        auto distance=s_sub_norm_wTsw.colwise().squaredNorm();

        int minIdx;
        *dist=distance.minCoeff(&minIdx);
        return minIdx;
    }

    /// to be reloaded
    virtual void associate(const std::unordered_set<infoUnit3*> & selected) const {
        for(auto i : selected) {
            i->closestRefPoint=findNearest(i->translatedFitness,&i->distance);
        }
    }

    virtual void associate(const std::vector<infoUnit3*> & Fl_src,
        std::unordered_multimap<RefPointIdx_t,infoUnit3*> * Fl_dst) const {
            for(auto i : Fl_src) {
                RefPointIdx_t idx=findNearest(i->translatedFitness,&i->distance);
                i->closestRefPoint=idx;
                Fl_dst->emplace(idx,i);
            }
    }

    inline static void extremePoints2Intercept(const Eigen::Array<double,eigSizeFlag,eigSizeFlag> & P,
        Fitness_t * intercept) {
        auto P_transpose_inv=P.transpose().matrix().inverse();
        auto ONE=Eigen::Matrix<double,eigSizeFlag,1>::Ones(P.cols(),1);
        auto one_div_intercept=(P_transpose_inv*ONE).array();
        *intercept=1.0/one_div_intercept;
    }
};

}   //  namespace Heu

#endif  //  Heu_NSGA3_HPP
