//
// Copyright (c) 2017 CNRS
//
// This file is part of PinInvDyn
// PinInvDyn is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
// PinInvDyn is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Lesser Public License for more details. You should have
// received a copy of the GNU Lesser General Public License along with
// PinInvDyn If not, see
// <http://www.gnu.org/licenses/>.
//

#ifndef __invdyn_solvers_hqp_eiquadprog_rt_hpp__
#define __invdyn_solvers_hqp_eiquadprog_rt_hpp__

#include "pininvdyn/solvers/solver-HQP-eiquadprog-rt.h"
#include "pininvdyn/solvers/eiquadprog_rt.hpp"
#include "pininvdyn/utils/stop-watch.hpp"
#include "pininvdyn/math/utils.hpp"

//#define PROFILE_EIQUADPROG_RT

#ifdef PROFILE_EIQUADPROG_RT
#define START_PROFILER_EIQUADPROG_RT START_PROFILER
#define STOP_PROFILER_EIQUADPROG_RT  STOP_PROFILER
#else
#define START_PROFILER_EIQUADPROG_RT
#define STOP_PROFILER_EIQUADPROG_RT
#endif

#define PROFILE_EIQUADPROG_PREPARATION "EiquadprogRT problem preparation"
#define PROFILE_EIQUADPROG_SOLUTION "EiquadprogRT problem solution"

namespace pininvdyn
{
  namespace solvers
  {
    
    template<int nVars, int nEqCon, int nIneqCon>
    Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::Solver_HQP_eiquadprog_rt(const std::string & name)
    : Solver_HQP_base(name)
    , m_hessian_regularization(DEFAULT_HESSIAN_REGULARIZATION)
    {
      m_n = nVars;
      m_neq = nEqCon;
      m_nin = nIneqCon;
      m_output.resize(nVars, nEqCon, 2*nIneqCon);
    }
    
    template<int nVars, int nEqCon, int nIneqCon>
    void Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::sendMsg(const std::string & s)
    {
      std::cout<<"[Solver_HQP_eiquadprog_rt."<<m_name<<"] "<<s<<std::endl;
    }
    
    template<int nVars, int nEqCon, int nIneqCon>
    void Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::resize(unsigned int n, unsigned int neq, unsigned int nin)
    {
      assert(n==nVars);
      assert(neq==nEqCon);
      assert(nin==nIneqCon);
    }
    
    template<int nVars, int nEqCon, int nIneqCon>
    const HqpOutput & Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::solve(const HqpData & problemData)
    {
      using namespace pininvdyn::math;
      //  Eigen::internal::set_is_malloc_allowed(false);
      
      START_PROFILER_EIQUADPROG_RT(PROFILE_EIQUADPROG_PREPARATION);
      
      if(problemData.size()>2)
      {
        assert(false && "Solver not implemented for more than 2 hierarchical levels.");
      }
      
      // Compute the constraint matrix sizes
      unsigned int neq = 0, nin = 0;
      const ConstraintLevel & cl0 = problemData[0];
      if(cl0.size()>0)
      {
        const unsigned int n = cl0[0].second->cols();
        for(ConstraintLevel::const_iterator it=cl0.begin(); it!=cl0.end(); it++)
        {
          const ConstraintBase* constr = it->second;
          assert(n==constr->cols());
          if(constr->isEquality())
            neq += constr->rows();
          else
            nin += constr->rows();
        }
        // If necessary, resize the constraint matrices
        resize(n, neq, nin);
        
        int i_eq=0, i_in=0;
        for(ConstraintLevel::const_iterator it=cl0.begin(); it!=cl0.end(); it++)
        {
          const ConstraintBase* constr = it->second;
          if(constr->isEquality())
          {
            m_CE.middleRows(i_eq, constr->rows()) = constr->matrix();
            m_ce0.segment(i_eq, constr->rows())   = -constr->vector();
            i_eq += constr->rows();
          }
          else if(constr->isInequality())
          {
            m_CI.middleRows(i_in, constr->rows()) = constr->matrix();
            m_ci0.segment(i_in, constr->rows())   = -constr->lowerBound();
            i_in += constr->rows();
            m_CI.middleRows(i_in, constr->rows()) = -constr->matrix();
            m_ci0.segment(i_in, constr->rows())   = constr->upperBound();
            i_in += constr->rows();
          }
          else if(constr->isBound())
          {
            m_CI.middleRows(i_in, constr->rows()).setIdentity();
            m_ci0.segment(i_in, constr->rows())   = -constr->lowerBound();
            i_in += constr->rows();
            m_CI.middleRows(i_in, constr->rows()) = -Matrix::Identity(m_n, m_n);
            m_ci0.segment(i_in, constr->rows())   = constr->upperBound();
            i_in += constr->rows();
          }
        }
      }
      else
        resize(m_n, neq, nin);
      
      if(problemData.size()>1)
      {
        const ConstraintLevel & cl1 = problemData[1];
        m_H.setZero();
        m_g.setZero();
        for(ConstraintLevel::const_iterator it=cl1.begin(); it!=cl1.end(); it++)
        {
          const double & w = it->first;
          const ConstraintBase* constr = it->second;
          if(!constr->isEquality())
            assert(false && "Inequalities in the cost function are not implemented yet");
          
          m_H.noalias() += w*constr->matrix().transpose()*constr->matrix();
          m_g.noalias() -= w*(constr->matrix().transpose()*constr->vector());
        }
        m_H.diagonal().noalias() += m_hessian_regularization*Vector::Ones(m_n);
      }
      
      STOP_PROFILER_EIQUADPROG_RT(PROFILE_EIQUADPROG_PREPARATION);
      
      //  // eliminate equality constraints
      //  if(m_neq>0)
      //  {
      //    m_CE_lu.compute(m_CE);
      //    sendMsg("The rank of CD is "+toString(m_CE_lu.rank());
      //    const MatrixXd & Z = m_CE_lu.kernel();
      
      //  }
      
      START_PROFILER_EIQUADPROG_RT(PROFILE_EIQUADPROG_SOLUTION);
      
      //  min 0.5 * x G x + g0 x
      //  s.t.
      //  CE x + ce0 = 0
      //  CI x + ci0 >= 0
      typename RtVectorX<nVars>::d sol(m_n);
      RtEiquadprog_status status = m_solver.solve_quadprog(m_H, m_g,
                                                           m_CE, m_ce0,
                                                           m_CI, m_ci0,
                                                           sol);
      STOP_PROFILER_EIQUADPROG_RT(PROFILE_EIQUADPROG_SOLUTION);
      
      m_output.x = sol;
      //  Eigen::internal::set_is_malloc_allowed(true);
      
      if(status==RT_EIQUADPROG_OPTIMAL)
      {
        m_output.status = HQP_STATUS_OPTIMAL;
        m_output.lambda = m_solver.getLagrangeMultipliers();
        //    m_output.activeSet = m_solver.getActiveSet().template tail< 2*nIneqCon >().head(m_solver.getActiveSetSize());
        m_output.activeSet = m_solver.getActiveSet().segment(m_neq, m_solver.getActiveSetSize()-m_neq);
        m_output.iterations = m_solver.getIteratios();
        
#ifndef NDEBUG
        const Vector & x = m_output.x;
        
        if(cl0.size()>0)
        {
          for(ConstraintLevel::const_iterator it=cl0.begin(); it!=cl0.end(); it++)
          {
            const ConstraintBase* constr = it->second;
            if(constr->checkConstraint(x)==false)
            {
              if(constr->isEquality())
              {
                sendMsg("Equality "+constr->name()+" violated: "+
                        toString((constr->matrix()*x-constr->vector()).norm()));
              }
              else if(constr->isInequality())
              {
                sendMsg("Inequality "+constr->name()+" violated: "+
                        toString((constr->matrix()*x-constr->lowerBound()).minCoeff())+"\n"+
                        toString((constr->upperBound()-constr->matrix()*x).minCoeff()));
              }
              else if(constr->isBound())
              {
                sendMsg("Bound "+constr->name()+" violated: "+
                        toString((x-constr->lowerBound()).minCoeff())+"\n"+
                        toString((constr->upperBound()-x).minCoeff()));
              }
            }
          }
        }
#endif
      }
      else if(status==RT_EIQUADPROG_UNBOUNDED)
        m_output.status = HQP_STATUS_INFEASIBLE;
      else if(status==RT_EIQUADPROG_MAX_ITER_REACHED)
        m_output.status = HQP_STATUS_MAX_ITER_REACHED;
      else if(status==RT_EIQUADPROG_REDUNDANT_EQUALITIES)
        m_output.status = HQP_STATUS_ERROR;
      
      return m_output;
    }
    
    template<int nVars, int nEqCon, int nIneqCon>
    double Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::getObjectiveValue()
    {
      return m_solver.getObjValue();
    }
    
    template<int nVars, int nEqCon, int nIneqCon>
    bool Solver_HQP_eiquadprog_rt<nVars, nEqCon, nIneqCon>::setMaximumIterations(unsigned int maxIter)
    {
      Solver_HQP_base::setMaximumIterations(maxIter);
      return m_solver.setMaxIter(maxIter);
    }
  
  } // namespace solvers
} // namespace pininvdyn

#endif // ifndef __invdyn_solvers_hqp_eiquadprog_rt_hpp__