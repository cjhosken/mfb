//
// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace optix {
namespace CompileWithTasks {

inline void check( OptixResult res, const char* call, const char* file, unsigned int line )
{
    if( res != OPTIX_SUCCESS )
    {
        std::stringstream s;
        s << "Optix call in " << file << ", line " << line << " (" << call << ") failed with code " << res;
        throw std::runtime_error( s.str() );
    }
}
#define COMPILE_WITH_TASKS_CHECK( call ) check( call, #call, __FILE__, __LINE__ )


// Using the specified number of threads, execute the functions added to the work
// queue. Work can be added asynchronously by any other thread. Call terminate() to exit
// all the threads. The thread pool can be started up again after being
// terminated. Threads are started, but suspended until there is work in the queue.
struct ThreadPool
{
    std::vector<std::thread> m_pool;
    std::mutex               m_queueMutex;
    using FunctionType = std::function<void()>;
    std::deque<FunctionType> m_workQueue;
    bool                     m_killPool = false;
    std::condition_variable  m_condition;

    void startPool( int numThreads )
    {
        for( int i = 0; i < numThreads; ++i )
            m_pool.emplace_back( std::bind( &ThreadPool::workerExecute, this ) );
    }

    void addWork( FunctionType&& function )
    {
        std::lock_guard<std::mutex> lock( m_queueMutex );
        m_workQueue.push_back( function );
        // Wake up one thread to handle this new job if it's not already awake.
        m_condition.notify_one();
    }

    void workerExecute()
    {
        while( true )
        {
            FunctionType work;
            {
                std::unique_lock<std::mutex> lock( m_queueMutex );

                // Sit here and wait until there's some work to do or we are terminating the pool
                m_condition.wait( lock, [this] { return !m_workQueue.empty() || m_killPool; } );
                if( m_killPool )
                    break;
                work = m_workQueue.front();
                m_workQueue.pop_front();
            }
            work();
        }
    }

    void terminate()
    {
        {
            std::unique_lock<std::mutex> lock( m_queueMutex );
            m_killPool = true;
            // This bit of code is optional depending on whether you want to be able to
            // terminate a non-empty queue.
            if( !m_workQueue.empty() )
                throw std::runtime_error( "pool not empty" );
        }
        // Wake all threads, so they can exit the work/wait loop.
        m_condition.notify_all();
        for( size_t i = 0; i < m_pool.size(); ++i )
            m_pool[i].join();
        m_pool.clear();
    }
};

// Compiles a single OptixModule using multiple threads contained in m_threadPool. As new
// tasks are generated from calling optixTaskExecute, add more work to the thread pool.
struct OptixTaskExecutePool
{
    ThreadPool   m_threadPool;
    unsigned int m_maxNumAdditionalTasks;
    bool         m_stop = false;

    // Returns after the OptixTask objects have completed executing. As new tasks are
    // created, add them to the thread pool for execution.
    OptixResult executeTaskAndWait( OptixModule module, OptixTask firstTask )
    {
        // This condition variable helps this thread to sleep until a dependent task is
        // executed, so we don't have to spin wait.
        std::condition_variable      cv;
        std::mutex                   mutex;
        std::unique_lock<std::mutex> lock( mutex );
        // Push work
        m_threadPool.addWork( [&]() { executeTask( firstTask, cv ); } );

        // The condition variable is triggered when a dependent task is finished
        // executed. Check the module to see if it's finished executing.
        OptixModuleCompileState state;
        cv.wait( lock, [&] {
            COMPILE_WITH_TASKS_CHECK( optixModuleGetCompilationState( module, &state ) );
            return state == OPTIX_MODULE_COMPILE_STATE_FAILED || state == OPTIX_MODULE_COMPILE_STATE_COMPLETED || m_stop;
        } );
        return state == OPTIX_MODULE_COMPILE_STATE_FAILED || state == OPTIX_MODULE_COMPILE_STATE_IMPENDING_FAILURE ?
                   OPTIX_ERROR_UNKNOWN :
                   OPTIX_SUCCESS;
    }

    void executeTask( OptixTask task, std::condition_variable& cv )
    {
        // When we execute the task, OptiX can generate upto the number of additional
        // tasks that we provide. [0..m_maxNumAdditionalTasks] are valid values for
        // numAdditionalTasksCreated.
        std::vector<OptixTask> additionalTasks( m_maxNumAdditionalTasks );
        unsigned int           numAdditionalTasksCreated;
        COMPILE_WITH_TASKS_CHECK( optixTaskExecute( task, additionalTasks.data(), m_maxNumAdditionalTasks, &numAdditionalTasksCreated ) );
        for( unsigned int i = 0; i < numAdditionalTasksCreated; ++i )
        {
            // Capture additionalTasks[i] by value since it will go out of scope.
            OptixTask task = additionalTasks[i];
            m_threadPool.addWork( [task, &cv, this]() { executeTask( task, cv ); } );
        }
        // Notify the thread calling executeTaskAndWait that a task has finished
        // executing.
        cv.notify_all();
    }
};

}  // end namespace CompileWithTasks
}  // end namespace optix
