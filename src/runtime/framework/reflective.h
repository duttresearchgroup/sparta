/*******************************************************************************
 * Copyright (C) 2018 Tiago R. Muck <tmuck@uci.edu>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/*
 * Some notes on the actuation interfaces, sensing interfaces and reflection:
 *
 *   - sense() returns the aggregated value of the sensor across the last
 *     sensing window (aggregation happens as defined by SensingTypeInfo::agg)
 *
 *   - senseIf() returns the predicted sensed value for the next sensed window.
 *     At first, senseIf() returns the same thing as sense(), since we
 *     assume in the future the system will behave the past window, unless we
 *     try to change something. So predictions are performed only when
 *     actuations are issued (by actuate()/tryActuate()).
 *
 *  - actuate() changes some actuation knob, obviously. Any call to senseIf()
 *    after actuate() returns predicted values if the sensed information is
 *    affected the actuation knob being set
 *
 *  - tryActuate() pretends to set an actuation knob. The difference between
 *    this one and actuate() is that tryActuate() affects values returned by
 *    senseIf(), but it doesn't actually set the physical knob.
 *
 *    In this context, the ReflectiveEngine is responsible mainly for providing
 *    the functionality needed by senseIf(). The predictions needed by senseIf
 *    are implemented by the predict() functions (in reflective_pred.cc). These
 *    functions uses some standard prediction metrics done by the BaselineModel
 *    to assess how the system behaves given the current values of sensed
 *    data, actuations and predicted actuations. The predicted actuations
 *    are actuations that are expected to be performed during the future window.
 *    These are modeled as actuations/resource management policy models which
 *    are executed as needed. in summary, the senseIf() flow is as follows:
 *
 *    1) Policy models predict which actuations are going to be performed in
 *    in the future. If a model is undefined, we just assume the current values
 *    provided by actuate/tryActuate will not be changed during the future
 *    window and those are used. Notice that, for actuations that change
 *    during the window, the predicted value will be the "average" value
 *    if possible, or the most frequent value (define by
 *    ActuationTypeInfo::AggregatorType).
 *
 *    2) predict() functions take the actuations predicted by step (1) and
 *    feed them to the BaselineModel. The metrics returned by the baseline
 *    model are then used to predict the requested metric returned to senseIf()
 *
 *    Notice there is one predict() function implementation for each senseIf().
 *
 *    Notice that senseIf() only calls predict() (see sensing_interface.h) if
 *    the resource we are predicting to was previously affected by a actuate()
 *    or tryActuate() (see needsPred() function). The dependencies between
 *    resources are generated by the addToRscNeedingPred() function, called
 *    whenever there is a tryActuate() or actuate().
 *
 */

#ifndef __arm_rt_framework_reflective_h
#define __arm_rt_framework_reflective_h

#include <unordered_set>
#include <type_traits>

#include "types.h"
#include <runtime/common/gen_store.h>
#include "models/baseline_model.h"
#include "actuation_interface_impl.h"

class Model;
class Policy;

class ReflectiveEngine {

    friend class SensingWindowManager;

  public:

    struct Context {

        int wid() { return _wid;}

        int winPeriodMS() { return _winPeriodMS;}

        int timestamp() { return _timestamp;}

        void incTimestamp() { ++_timestamp;}

        bool reflecting() { return _reflecting; }

        int reflectingOffset() { return _reflectingOffset; }

        Model* currentModel() { return _currentModel; }

        struct ReflectiveScope {
            ReflectiveScope(int offset)
                :_reflecting_prev(_currentContext.reflecting()),
                 _reflectingOffset_prev(_currentContext.reflectingOffset())
            {
                assert_true(offset > 0);
                _currentContext._reflecting = true;
                _currentContext._reflectingOffset = offset;
            }
            ~ReflectiveScope()
            {
                _currentContext._reflecting = _reflecting_prev;
                _currentContext._reflectingOffset = _reflectingOffset_prev;
            }

          private:
            bool _reflecting_prev;
            int _reflectingOffset_prev;
        };
        struct NonReflectiveScope {
            NonReflectiveScope()
                :_reflecting_prev(_currentContext.reflecting())
            {
                _currentContext._reflecting = false;
            }
            ~NonReflectiveScope()
            {
                _currentContext._reflecting = _reflecting_prev;
            }

          private:
            bool _reflecting_prev;
        };
        struct SensingWindowScope : NonReflectiveScope {
            SensingWindowScope(int wid, int periodMS)
            {
                assert_true(_currentContext._wid == -1);
                _currentContext._wid = wid;
                _currentContext._winPeriodMS = periodMS;
                _currentContext.incTimestamp();
            }
            ~SensingWindowScope()
            {
                _currentContext._wid = -1;
            }
        };
        struct PolicyScope {
            PolicyScope(Policy *pol); // implemented in the header to prevent circular dependence
            ~PolicyScope()
            {
                _currentContext._currentModel = nullptr;
            }
        };
        struct ModelScope {
            ModelScope(Model *m): _prev(_currentContext._currentModel)
            {
                assert_true(_currentContext._currentModel != m);
                _currentContext._currentModel = m;
            }
            ~ModelScope()
            {
                _currentContext._currentModel = _prev;
            }
          private:
            Model *_prev;
        };

        Context()
            :_wid(-1), _winPeriodMS(-1),
             _reflecting(false), _reflectingOffset(0),
             _timestamp(-1),
             _currentModel(nullptr)
        {}

      private:
        // Current window id
        int _wid;

        // Period of the current window in millisecs
        int _winPeriodMS;

        // If true, it means we are running policies and models for predicting
        // actuations. Sense now returns predicted sensed values given an
        // actuation made by tryActuate.
        // Otherwise, returns real sensed data
        bool _reflecting;

        // When running policies as models or pure models, this tells how many
        // time units the model is ahead of the policy we are predicting for
        int _reflectingOffset;

        // Timestamp incremented at every window.
        int _timestamp;

        // Is non-null while a policy is being executed
        Model* _currentModel;
    };

    static thread_local Context _currentContext;

    static ReflectiveEngine* _currentReflectiveEngine;

    static void enable(const sys_info_t *si)
    {
        assert_true(_currentReflectiveEngine == nullptr);
        _currentReflectiveEngine = new ReflectiveEngine(si);
    }

    static void disable()
    {
        if(_currentReflectiveEngine != nullptr)
            delete _currentReflectiveEngine;
    }

  public:

    static ReflectiveEngine& get()
    {
        assert_true(_currentReflectiveEngine != nullptr);
        return *_currentReflectiveEngine;
    }

    static bool enabled() { return _currentReflectiveEngine != nullptr; }

    static Context& currentContext() { return _currentContext; }

    static int currentWID() { return _currentContext.wid(); }

    static bool isReflecting() { return _currentContext.reflecting(); }


  private:

    const sys_info_t *_sys_info;

    DefaultBaselineModel _baselineModel;

    // If true, policy model that are finer-grained than the current policy
    // will be executed the next time senseIf is called. This is typically
    // set to true every time there is a new actuation and then set to false
    // after the models are run.
    bool _needToRunModels;

    // See winActMap,polActMap,modelTryActMap
    bool _winActMapClean;
    bool _polActMapClean;
    bool _modelActMapClean;

    typedef std::unordered_map<const void*,VariantStore<
            ActuationTypeInfo<ACT_FREQ_MHZ>::AggregatorType,
            ActuationTypeInfo<ACT_FREQ_GOV>::AggregatorType,
            ActuationTypeInfo<ACT_ACTIVE_CORES>::AggregatorType,
            ActuationTypeInfo<ACT_TASK_MAP>::AggregatorType,
            ActuationTypeInfo<ACT_DUMMY1>::AggregatorType,
            ActuationTypeInfo<ACT_DUMMY2>::AggregatorType>> TryActMap;



    // Actuations performed by only actuate() by all policies or sensing window
    // handlers executed in the same period as the current sensing window.
    // This also includes actuations performed in different sensing windows, as
    // long as their last period coincides with the period of the current
    // window.
    // This map is always cleared before executing the first handler for the
    // current window
    TryActMap winActMap[SIZE_ACT_TYPES];

    // Stores actuations performed only by tryActuate() by the current policy
    // or sensing window handler.
    // This map is always cleared before executing any handler or policy.
    TryActMap polActMap[SIZE_ACT_TYPES];

    // When a policy is run as a model (isReflecting()==true), tryActuates
    // are stored in this map (note in this context all actuates behave like
    // tryActuates()). Predictions are made first based on this map
    // and then based on polTryActMap, then based on winActMap. This map is
    // cleared every time before policies are run as models.
    TryActMap modelActMap[SIZE_ACT_TYPES];

    // All resources affected by actuate/tryActute in the current window
    std::unordered_set<const void*> rscNeedingPred;

    void addToRscNeedingPredImpl(const power_domain_info_t *rsc);
    void addToRscNeedingPredImpl(const freq_domain_info_t *rsc);
    void addToRscNeedingPredImpl(const core_info_t *rsc);
    void addToRscNeedingPredImpl(const tracked_task_data_t *rsc);
    void addToRscNeedingPredImpl(const NullResource *rsc);

    template<typename ResourceT>
    void addToRscNeedingPred(const ResourceT *rsc)
    {
        _needToRunModels = !isReflecting();
        addToRscNeedingPredImpl(rsc);
    }

    template<ActuationType ACT_T,typename ResourceT>
    void _registerActuation(const ResourceT *rsc, const typename ActuationTypeInfo<ACT_T>::ValType &val, TryActMap &map, int offset)
    {
        auto iter = map.find(rsc);
        if(iter == map.end()){
            if(offset > 0)
                map[rsc].template get<typename ActuationTypeInfo<ACT_T>::AggregatorType>().addValue(
                        ActuationInterfaceImpl::Impl::actuationVal<ACT_T>(rsc),0);
            map[rsc].template get<typename ActuationTypeInfo<ACT_T>::AggregatorType>().addValue(val,offset);
        }
        else
            iter->second.template get<typename ActuationTypeInfo<ACT_T>::AggregatorType>().addValue(val,offset);

        addToRscNeedingPred(rsc);
    }

  public:

    ReflectiveEngine(const sys_info_t *si)
      :_sys_info(si),_baselineModel(si),_needToRunModels(false),
       _winActMapClean(true), _polActMapClean(true), _modelActMapClean(true)
    {}

    bool needsPred(const void *rsc) { return rscNeedingPred.find(rsc) != rscNeedingPred.end(); }

    template<ActuationType ACT_T,typename ResourceT>
    void tryActuate(const ResourceT *rsc, const typename ActuationTypeInfo<ACT_T>::ValType &val)
    {
        static_assert(ACT_T < SIZE_ACT_TYPES, "Invalid ACT_T");
        if(isReflecting()){
            //pinfo("\t\t***TRY_ACT_M: ACT_T = %d\n",ACT_T);
            _registerActuation<ACT_T>(rsc,val,modelActMap[ACT_T], currentContext().reflectingOffset());
            _modelActMapClean = false;
        }
        else {
            //pinfo("\t\t***TRY_ACT_P: ACT_T = %d\n",ACT_T);
            _registerActuation<ACT_T>(rsc,val,polActMap[ACT_T], 0);
            _polActMapClean = false;
        }
    }

    template<ActuationType ACT_T,typename ResourceT>
    void actuate(const ResourceT *rsc, const typename ActuationTypeInfo<ACT_T>::ValType &val)
    {
        static_assert(ACT_T < SIZE_ACT_TYPES, "Invalid ACT_T");
        _registerActuation<ACT_T>(rsc,val,winActMap[ACT_T], 0);
        _winActMapClean = false;
    }

    // TODO maybe optimize hasNewActuationVal so we can more easily know which
    // map has the new actuation val
    template<ActuationType ACT_T,typename ResourceT>
    bool hasNewActuationVal(const ResourceT *rsc)
    {
        static_assert(ACT_T < SIZE_ACT_TYPES, "Invalid ACT_T");
        if(modelActMap[ACT_T].find(rsc) != modelActMap[ACT_T].end())
            return true;
        else if(polActMap[ACT_T].find(rsc) != polActMap[ACT_T].end())
            return true;
        else
            return winActMap[ACT_T].find(rsc) != winActMap[ACT_T].end();
    }

    template<ActuationType ACT_T,typename ResourceT>
    typename ActuationTypeInfo<ACT_T>::ValType newActuationVal(const ResourceT *rsc)
    {
        static_assert(ACT_T < SIZE_ACT_TYPES, "Invalid ACT_T");
        auto iter = modelActMap[ACT_T].find(rsc);
        if(iter == modelActMap[ACT_T].end()){
            iter = polActMap[ACT_T].find(rsc);
            if(iter == polActMap[ACT_T].end()){
                iter = winActMap[ACT_T].find(rsc);
                // Can only be called when hasNewActuationVal returns true
                assert_true(iter != winActMap[ACT_T].end());
            }
        }
        // TODO currently the aggregate value is returned for the policy that
        // triggered the reflection, and the latest value for policies/models
        // being emulated in the reflective scope. However this is an
        // approximation. To be fully accurate, when emulating a policy, we
        // need to differentiate whether or not it is calling tryActuate or
        // actuate and consider these scenarios when isReflecting()==true:
        //   1) The policy calls actuateVal, and other finer-grained policies
        //      have not modified the val, so we return the latest value
        //      (this is the one current implemented and the intended behavior
        //       in this scenario)
        //   2) Same as (1), but a finer-grained have modified the value, so we
        //      need to return the aggregated value (e.g. mean) of the values
        //      set between the beginning of the reflection and the current
        //      time. Also if the same policy is emulated multiple times, the
        //      aggregation should be between the time of the current and the
        //      previous emulation. Implementing this would require at least
        //      multiple versions of modelTryActMap and complicate things
        //   3) When emulating a policy that is also reflexive (it calls
        //      tryActuate/tryActuateVal) we should do some nested reflection
        //      (not implemented as of now) and return the future aggregate val
        //      after the emulation of finer grained models
        auto &val = iter->second.template get<typename ActuationTypeInfo<ACT_T>::AggregatorType>();
        return isReflecting() ? val.latest() : val.agg(currentContext().winPeriodMS());
    }

    template<SensingType SEN_T,typename ResourceT>
    typename SensingTypeInfo<SEN_T>::ValType predict(const ResourceT *rsc);

    template<SensingType SEN_T,typename ResourceT>
    typename SensingTypeInfo<SEN_T>::ValType predict(typename SensingTypeInfo<SEN_T>::ParamType p, const ResourceT *rsc);

    DefaultBaselineModel& baselineModel() { return _baselineModel; }

    // Called before the first window handler after a new window starts
    // on a different period.
    void resetModelsOnNewWindowPeriod()
    {
        // Any actuation already performed have now manifested itself on the
        // new sensed data, so we don't need to predict anything until new
        // actuations are issued
        rscNeedingPred.clear();

        // Clear for the next actuations
        if(!_winActMapClean){
            for(int i = 0; i < SIZE_ACT_TYPES; ++i) winActMap[i].clear();
            _winActMapClean = true;
        }

        // Should be false until actuations are issued
        _needToRunModels = false;
    }

    // Called before the first handler of the current window
    void resetModelsOnWindow()
    {
        // In each new window, we get a fresh set of shared data, se we reset
        // the baseline model
        _baselineModel.reset();
    }


    // Called before every window handler or policy to be executed
    // For policies this may be called twice (once by the window handle and
    // once by the PolicyManager, since the same handler can be used to run
    // multiple policies)
    void resetModelsOnHandler()
    {
        // Clear for the next actuations (though tryActuate)
        if(!_polActMapClean){
            for(int i = 0; i < SIZE_ACT_TYPES; ++i) polActMap[i].clear();
            _polActMapClean = true;
        }

        // Wanna make sure we run the reflective models at least once
        // before the first senseIf
        _needToRunModels = true;
    }

    // Called before policy models are executed in reflective context
    void resetModelsOnReflection()
    {
        // Clear for the next mo
        if(!_modelActMapClean){
            for(int i = 0; i < SIZE_ACT_TYPES; ++i) modelActMap[i].clear();
            _modelActMapClean = true;
        }
    }

    // Called before every senseIf
    // All policies/models that are finer-grained than the current model are
    // executed in reflective scope.
    void runFinerGrainedModels()
    {
        Model *m = currentContext().currentModel();
        if((m == nullptr) || (_needToRunModels == false)) return;

        _needToRunModels = false;

        // Reset actuations from previous runs
        resetModelsOnReflection();

        runFinerGrainedModelsImpl(m);
    }

  private:

    void runFinerGrainedModelsImpl(Model *currModel);

  public:

    // Stablishes the ORDER models will execute.
    // For example, consider three policies:
    //   P0: period=50ms
    //   P1: period=100ms
    //   P3: period=250ms
    // When predicting sensed values for P3 at time 250, we need to emulate P0
    // and P1 as models in the following order with the proper future times (in
    // ()'s):
    //   P3-> P0(300) P1(300) P0(350) P0(400) P1(400) P0(450)
    struct ReflectiveScheduleEntry {
        // Next entry in the list
        ReflectiveScheduleEntry *next = nullptr;
        // Model to run in this entry
        Model *model = nullptr;
        // Copy of next. Used tho restore nexy in case we had to temporally
        // change it
        ReflectiveScheduleEntry *next_copy = nullptr;
    };

    void buildSchedule(Model *model);
};

#endif /* ACTUATOR_H_ */