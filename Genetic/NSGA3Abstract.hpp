/*
 Copyright © 2022  TokiNoBug
This file is part of HeuristicFlow.

    HeuristicFlow is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    HeuristicFlow is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HeuristicFlow.  If not, see <https://www.gnu.org/licenses/>.

*/

#ifndef Heu_NSGA3ABSTRACT_HPP
#define Heu_NSGA3ABSTRACT_HPP

#include <unordered_map>
#include <unordered_set>
#include "NSGABase.hpp"
#include "../HeuristicFlow/SimpleMatrix"

namespace Heu {


template<typename Var_t,
        size_t ObjNum,
        DoubleVectorOption DVO,
        RecordOption rOpt,
        PFOption pfOpt,
        class Args_t>
class NSGA3Abstract
    : public NSGABase<Var_t,ObjNum,DVO,FITNESS_LESS_BETTER,rOpt,pfOpt,Args_t>
{
public:
    NSGA3Abstract() {};
    virtual ~NSGA3Abstract() {};
    using Base_t = NSGABase<Var_t,ObjNum,DVO,FITNESS_LESS_BETTER,rOpt,pfOpt,Args_t>;
    Heu_MAKE_NSGABASE_TYPES
    using RefPointIdx_t = size_t;

#ifdef EIGEN_CORE_H
    using RefMat_t= typename std::conditional<DVO==Std,
        MatrixDynamicSize<double>,
        Eigen::Array<double,(ObjNum==Dynamic?Eigen::Dynamic:ObjNum),Eigen::Dynamic>>::type;
#else
    using RefMat_t = MatrixDynamicSize<double>;
#endif

    inline const RefMat_t & referencePoints() const {
        return referencePoses;
    }

    struct infoUnit3 : public infoUnitBase_t
    {
        Fitness_t translatedFitness;
        size_t closestRefPoint;
        double distance;
    };

protected:
    RefMat_t referencePoses;

    void computeReferencePointPoses(const size_t dimN,
        const size_t precision,
        std::vector<Fitness_t> * dst) const {
        dst->clear();
        dst->reserve(Heu::NchooseK(dimN+precision-1,precision));

        pri_startRP(dimN,precision,dst);
    }

    inline static bool sortByDominatedNum(const infoUnit3 * A,const infoUnit3* B) {
        return (A->domainedByNum)<(B->domainedByNum);
    }

    void select() {
        const size_t popSizeBef=this->_population.size();
        std::vector<infoUnit3> pop;
        pop.reserve(popSizeBef);
        for(auto it=this->_population.begin();it!=this->_population.end();++it) {
            pop.emplace_back();
            pop.back().iterator=it;
            pop.back().closestRefPoint=-1;
        }

        std::vector<infoUnit3*> sortSpace(popSizeBef);
        for(size_t i=0;i<popSizeBef;i++) {
            sortSpace[i]=pop.data()+i;
        }

        this->calculateDominatedNum((infoUnitBase_t**)sortSpace.data(),popSizeBef);

        std::list<std::vector<infoUnit3*>> pfLayers;
        std::sort(sortSpace.begin(),sortSpace.end(),sortByDominatedNum);

        size_t curDM=-1;
        for(auto i : sortSpace) {
            if(curDM!=i->domainedByNum) {
                curDM=i->domainedByNum;
                pfLayers.emplace_back();
                pfLayers.back().reserve(popSizeBef);
            }
            pfLayers.back().emplace_back(i);
        }

        this->updatePF((const infoUnitBase_t **)pfLayers.front().data(),pfLayers.front().size());

        std::unordered_set<infoUnit3*> selected;
        selected.reserve(this->_option.populationSize);
        std::vector<infoUnit3*> * FlPtr=nullptr;
        bool needRefPoint;

        while(true) {
            if(selected.size()==this->_option.populationSize) {
                needRefPoint=false;
                break;
            }
            if(selected.size()+pfLayers.front().size()>this->_option.populationSize) {
                needRefPoint=true;
                FlPtr=&pfLayers.front();
                break;
            }

            for(infoUnit3* i : pfLayers.front()) {
                selected.emplace(i);
            }
            pfLayers.pop_front();

        }


        if(needRefPoint) {
            ///Normalize procedure
            std::unordered_multimap<RefPointIdx_t,infoUnit3*> Fl;
            Fl.reserve(FlPtr->size());

            normalize(selected,*FlPtr);
            std::unordered_map<RefPointIdx_t,size_t> refPoints;
            refPoints.reserve(referencePoses.cols());
            for(size_t i=0;i<referencePoses.cols();i++) {
                refPoints.emplace(i,0);
            }

            ///Associate procedure
            associate(selected);
            associate(*FlPtr,&Fl);

            nichePreservation(&selected,&Fl,&refPoints);
        }
        
        //erase all unselected genes
        for(auto i : sortSpace) {
            if(selected.find(i)==selected.end()) {
                this->_population.erase(i->iterator);
            }
        }
    }   //  end selection

    /// to be reloaded
    virtual void normalize(const std::unordered_set<infoUnit3*> & selected,
        const std::vector<infoUnit3*> & Fl) const {

        const size_t M=this->objectiveNum();
        stdContainer<const infoUnit3*,ObjNum> extremePtrs;
        iniSize4StdContainer<const infoUnit3*,ObjNum>::iniSize(&extremePtrs,M);

        SquareMat_t<double,ObjNum> extremePoints;
        
        Fitness_t ideal,intercepts;
        
        if constexpr (ObjNum==Dynamic) {
            extremePoints.resize(M,M);
            ideal.resize(M);
            intercepts.resize(M);
        }

        for(size_t c=0;c<M;c++) {
            ideal[c]=pinfD;
            extremePtrs[c]=*(Fl.begin());
            for(size_t r=0;r<M;r++) {
                extremePoints(r,c)=extremePtrs[c]->iterator->_Fitness[r];
            }
        }

        for(auto i : selected) {
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                ideal[objIdx]=std::min(ideal[objIdx],i->iterator->_Fitness[objIdx]);
                if(i->iterator->_Fitness[objIdx]>extremePtrs[objIdx]->iterator->_Fitness[objIdx]) {
                    extremePtrs[objIdx]=i;
                }
            }
        }

        for(auto i : Fl) {
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                ideal[objIdx]=std::min(ideal[objIdx],i->iterator->_Fitness[objIdx]);
                if(i->iterator->_Fitness[objIdx]>extremePtrs[objIdx]->iterator->_Fitness[objIdx]) {
                    extremePtrs[objIdx]=i;
                }
            }
        }

        for(size_t c=0;c<M;c++) {
            for(size_t r=0;r<M;r++) {
                extremePoints(r,c)=extremePtrs[c]->iterator->_Fitness[r]-ideal[r];
            }
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
            i->translatedFitness=i->iterator->_Fitness;
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                i->translatedFitness[objIdx]-=ideal[objIdx];
                i->translatedFitness[objIdx]/=intercepts[objIdx];
            }
        }

        for(auto i : Fl) {
            i->translatedFitness=i->iterator->_Fitness;
            for(size_t objIdx=0;objIdx<M;objIdx++) {
                i->translatedFitness[objIdx]-=ideal[objIdx];
                i->translatedFitness[objIdx]/=intercepts[objIdx];
            }
        }
    }

    size_t findNearest(const Fitness_t & s,double * dist) const {
        std::vector<double> eachDistance(referencePoses.cols());
#ifndef Heu_NSGA_USE_THREADS
        for(size_t c=0;c<referencePoses.cols();c++) {
            double normW=0,w_T_s=0;
            for(size_t r=0;r<referencePoses.rows();r++) {
                normW+=OT_square(referencePoses(r,c));
                w_T_s+=s[r]*referencePoses(r,c);
            }

            const double w_T_s_div_normW=w_T_s/normW;

            double distance=0;
            for(size_t r=0;r<referencePoses.rows();r++) {
                distance+=OT_square(s[r]-w_T_s_div_normW*referencePoses(r,c));
            }
            eachDistance[c]=distance;
        }
#else
        static const size_t thN=OtGlobal::threadNum();
#pragma omp parallel for
        for(size_t begIdx=0;begIdx<thN;begIdx++) {
            
            for(size_t c=begIdx;c<referencePoses.cols();c+=thN) {
            double normW=0,w_T_s=0;
            for(size_t r=0;r<referencePoses.rows();r++) {
                normW+=OT_square(referencePoses(r,c));
                w_T_s+=s[r]*referencePoses(r,c);
            }

            const double w_T_s_div_normW=w_T_s/normW;

            double distance=0;
            for(size_t r=0;r<referencePoses.rows();r++) {
                distance+=OT_square(s[r]-w_T_s_div_normW*referencePoses(r,c));
            }
            eachDistance[c]=distance;
        }
        }
#endif
        size_t minIdx=0;
        for(size_t i=1;i<eachDistance.size();i++) {
            if(eachDistance[i]<eachDistance[minIdx]) {
                minIdx=i;
            }
        }

        *dist=eachDistance[minIdx];
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

    virtual void nichePreservation(std::unordered_set<infoUnit3*> * selected,
        std::unordered_multimap<RefPointIdx_t,infoUnit3*> * Fl,
        std::unordered_map<RefPointIdx_t,size_t> * refPoints) const {
        
        for(auto i : *selected) {
            refPoints->operator[](i->closestRefPoint)++;
        }

        std::vector<std::unordered_map<RefPointIdx_t,size_t>::iterator> minNicheIterators;
        minNicheIterators.reserve(refPoints->size());

        std::pair<typename std::unordered_multimap<RefPointIdx_t,infoUnit3*>::iterator,
            typename std::unordered_multimap<RefPointIdx_t,infoUnit3*>::iterator>
                associatedGenesInFl;
        
        while(selected->size()<this->_option.populationSize) {
            findMinSet(*refPoints,&minNicheIterators);
            auto curRefPoint=minNicheIterators[size_t(randD(0,minNicheIterators.size()))];
            size_t rhoJ=curRefPoint->second;

            associatedGenesInFl=Fl->equal_range(curRefPoint->first);

            if(associatedGenesInFl.first!=associatedGenesInFl.second) { //  not empty
                typename std::unordered_multimap<RefPointIdx_t,infoUnit3*>::iterator
                        pickedGene;
                if(rhoJ==0) {
                    //find element in associatedGenesInFl with minimum distance
                    typename std::unordered_multimap<RefPointIdx_t,infoUnit3*>::iterator
                        minGene=associatedGenesInFl.first;
                    for(auto it=associatedGenesInFl.first;it!=associatedGenesInFl.second;++it) {
                        if(it->second->distance<minGene->second->distance) {
                            minGene=it;
                        }
                    }
                    pickedGene=minGene;
                }
                else {
                    //pick a random member in associatedGenesInFl
                    size_t N=0;
                    for(auto it=associatedGenesInFl.first;it!=associatedGenesInFl.second;++it) {
                        N++;
                    }
                    for(auto it=associatedGenesInFl.first;it!=associatedGenesInFl.second;++it) {
                        if(randD()*N<=1) {
                            pickedGene=it;
                            break;
                        }
                        N--;
                    }

                }

                selected->emplace(pickedGene->second);
                Fl->erase(pickedGene);
                curRefPoint->second++;
            }
            else {
                refPoints->erase(curRefPoint);
            }
        }   //  end while
    }

    inline static void findMinSet(std::unordered_map<RefPointIdx_t,size_t> & refPoints,
            std::vector<std::unordered_map<RefPointIdx_t,size_t>::iterator> * minNicheIterators) {
        minNicheIterators->clear();
        size_t minNiche=-1;
        for(auto i : refPoints) {
            minNiche=std::min(minNiche,i.second);
        }
        for(auto it=refPoints.begin();it!=refPoints.end();++it) {
            if(it->second==minNiche) {
                minNicheIterators->emplace_back(it);
            }
        }
    }


private:
    void pri_makeRP(const size_t dimN,const size_t precision,
        const size_t curDim,const size_t curP,const size_t accum,
        Fitness_t * rec,
        std::vector<Fitness_t> * dst) const {

        if(curDim+1>=dimN) {
            rec->operator[](dimN-1)=1.0-double(accum)/precision;
            dst->emplace_back(*rec);
            return;
        }

        for(size_t p=0;p+accum<=precision;p++) {
            if(curDim>=0)
                rec->operator[](curDim)=double(p)/precision;
            pri_makeRP(dimN,precision,curDim+1,p,accum+p,rec,dst);
        }
    }

    void pri_startRP(const size_t dimN,
        const size_t precision,
        std::vector<Fitness_t> * dst) const {

        Fitness_t rec;

        if constexpr(ObjNum==Dynamic) {
            rec.resize(this->objectiveNum());
        }

        pri_makeRP(dimN,precision,0,0,0,&rec,dst);
    }

    
    inline static void extremePoints2Intercept(const SquareMat_t<double,ObjNum> & P_T,
        Fitness_t * intercept) {
        
        SquareMat_t<double,ObjNum> inv;

        InverseMatrix_LU<double,ObjNum>(P_T,&inv);

        for(size_t r=0;r<P_T.rows();r++) {
            for(size_t c=0;c<r;c++) {
                std::swap(inv(r,c),inv(c,r));
            }
        }

        Matrix_t<double,ObjNum,1> Ones,one_div_intercept;

        if constexpr (ObjNum==Dynamic) {
            Ones.resize(P_T.rows(),1);
            intercept->resize(P_T.rows());
        }

        for(size_t i=0;i<P_T.rows();i++) {
            Ones[i]=1;
        }

        MatrixProduct<double,ObjNum,ObjNum,1>(inv,Ones,&one_div_intercept);

        for(size_t i=0;i<P_T.rows();i++) {
            intercept->operator[](i)=1.0/one_div_intercept[i];
        }
        
    }
#ifndef Heu_NO_STATICASSERT
    static_assert(DVO!=DoubleVectorOption::Custom,
        "Using custom double container as fitness isn't supported");
#endif  //  Heu_NO_STATICASSERT
};

#define Heu_MAKE_NSGA3ABSTRACT_TYPES \
Heu_MAKE_NSGABASE_TYPES \
using RefMat_t = typename Base_t::RefMat_t; \
using infoUnit3 = typename Base_t::infoUnit3; \
using RefPointIdx_t = typename Base_t::RefPointIdx_t;
}

#endif  //  Heu_NSGA3ABSTRACT_HPP
