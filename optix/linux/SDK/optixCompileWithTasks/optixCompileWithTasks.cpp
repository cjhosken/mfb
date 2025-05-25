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

#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stack_size.h>
#include <optix_stubs.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <cuda.h>
#include <cuda_runtime.h>

#include <CompileWithTasks.h>
#include <sutil/Exception.h>

using namespace optix::CompileWithTasks;



static void context_log_cb( unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw( 2 ) << level << "][" << std::setw( 12 ) << tag << "]: "
    << message << "\n";
}

OptixDeviceContext          s_context                = 0;
OptixDeviceContextOptions   s_options                = {};
OptixModuleCompileOptions   s_moduleCompileOptions   = {};
OptixPipelineCompileOptions s_pipelineCompileOptions = {};
unsigned int                s_defaultLogLevel        = 4;

OptixTaskExecutePool        g_pool;


static void SetUp()
{
    CUDA_CHECK( cudaFree( 0 ) );
    void* handle;
    OPTIX_CHECK( optixInitWithHandle( &handle ) );

    s_options.logCallbackFunction = &context_log_cb;
    s_options.logCallbackLevel    = s_defaultLogLevel;
    CUcontext cuCtx               = 0;  // zero means take the current context
    OPTIX_CHECK( optixDeviceContextCreate( cuCtx, &s_options, &s_context ) );
}

static void SetLoggingLevel( unsigned int level )
{
    OPTIX_CHECK( optixDeviceContextSetLogCallback( s_context, &context_log_cb, 0, level ) );
}

static void TearDown()
{
    OPTIX_CHECK( optixDeviceContextDestroy( s_context ) );
}

std::string readInputFile( const std::string& filename )
{
    std::ifstream input( filename.c_str(), std::ios::binary );
    if( !input )
    {
        std::cerr << "ERROR: Failed to open input file '" << filename << "'\n";
        exit( 1 );
    }

    std::vector<unsigned char> buffer( std::istreambuf_iterator<char>( input ), {} );
    return std::string( buffer.begin(), buffer.end() );
}

struct Timer
{
    Timer() { m_start = m_clock.now(); }

    double elapsed() const
    {
        std::chrono::duration<double> e = m_clock.now() - m_start;
        return e.count();
    }

    friend std::ostream& operator<<( std::ostream& out, const Timer& timer ) { return out << timer.elapsed(); }
    std::chrono::high_resolution_clock             m_clock;
    std::chrono::high_resolution_clock::time_point m_start;
};

void compileModule( const std::string& input, int numIters = 1 )
{
    OptixModule module;
    Timer       overallTimer;
    for( int i = 0; i < numIters; ++i )
    {
        Timer iterTimer;
        OPTIX_CHECK( optixModuleCreateFromPTX( s_context, &s_moduleCompileOptions, &s_pipelineCompileOptions,
                                               input.c_str(), input.size(), 0, 0, &module ) );
        if( i == 0 )
            SetLoggingLevel( 0 );
        std::cout << "iter[" << i << "] duration = " << iterTimer << " seconds\n";
    }
    double seconds = overallTimer.elapsed();
    SetLoggingLevel( s_defaultLogLevel );
    std::cout << "over all time " << seconds << " seconds, per iter average = " << seconds / numIters << "\n";
    std::cout << "Successfully compiled\n";
}

void compileModuleWithTasks( const std::string& input, int numIters = 1 )
{
    OptixModule module;
    Timer       overallTimer;
    for( int i = 0; i < numIters; ++i )
    {
        Timer     iterTimer;
        OptixTask firstTask;
        OPTIX_CHECK( optixModuleCreateFromPTXWithTasks( s_context, &s_moduleCompileOptions, &s_pipelineCompileOptions,
                                                        input.c_str(), input.size(), 0, 0, &module, &firstTask ) );
        OPTIX_CHECK( g_pool.executeTaskAndWait( module, firstTask ) );
        if( i == 0 )
            SetLoggingLevel( 0 );
        std::cout << "iter[" << i << "] duration = " << iterTimer << " seconds\n";
    }
    double seconds = overallTimer.elapsed();
    SetLoggingLevel( s_defaultLogLevel );
    std::cout << "over all time " << seconds << " seconds, per iter average = " << seconds / numIters << "\n";
    std::cout << "Successfully compiled\n";
}

void printUsageAndExit( const std::string& argv0, bool doExit = true )
{
    // These provide a rudimentary set of options and are by no means exhaustive to the
    // set of compile options available to optixModuleCreateFromPTX.
    std::cerr << "\nUsage  : " << argv0 << " [options] <input_file>\n"
              << "App options:\n"
              << "  -h   | --help                     Print this usage message\n"
              << "  -na  | --num-attributes <N>       Number of attribute values (up to 8, default 2)\n"
              << "  -npv | --num-payload-values <N>   Number of payload values (up to "
              << OPTIX_COMPILE_DEFAULT_MAX_PAYLOAD_VALUE_COUNT << ", default 2)\n"
              << "  -npt | --num-payload-types <N>    Number of payload types (up to "
              << OPTIX_COMPILE_DEFAULT_MAX_PAYLOAD_TYPE_COUNT << ", default 1)\n"
              << "  -ni  | --num-iters <N>            Number of iterations to compile. > 1 disables disk cache (default 1)\n"
              << "  -dt  | --disable-tasks            Disable compilation with tasks (default enabled)\n"
              << "  -dc  | --disable-cache            Disable caching of compiled ptx on disk (default enabled)\n"
              << "  -nt  | --num-threads <N>          Number of threads (default 1)\n"
              << "  -mt  | --max-num-tasks <N>        Maximum number of additional tasks (default 2)\n"
              << "  -g                                Enable debug support (implies -O0)\n"
        << std::endl;

    if( doExit )
        exit( 1 );
}

int main( int argc, char** argv )
{
    bool  useTasks    = true;
    bool  useCache    = true;
    int   numThreads  = 2;
    int   maxNumTasks = 2;
    int   numIters    = 1;
    std::string                   filename;
    std::vector<OptixPayloadType> types;
    std::vector<unsigned int>     defaultPayloadSemantics;
    
    if( argc < 2 )
    {
        std::cerr << "\nERROR: No input file provided for compilation\n";
        printUsageAndExit( argv[0] );
    }

    for( int i = 1; i < argc; ++i )
    {
        std::string arg( argv[i] );
        if( arg == "-h" || arg == "--help" )
        {
            printUsageAndExit( argv[0] );
        }
        else if( arg == "-na" || arg == "--num-attributes" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            s_pipelineCompileOptions.numAttributeValues = atoi( argv[++i] );
        }
        else if( arg == "-npv" || arg == "--num-payload-values" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            s_pipelineCompileOptions.numPayloadValues = atoi( argv[++i] );
        }
        else if( arg == "-npt" || arg == "--num-payload-types" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            int numTypes = atoi( argv[++i] );
            types.resize( numTypes, {} );
            defaultPayloadSemantics.resize( s_pipelineCompileOptions.numPayloadValues, 0 );
            for( unsigned int& payloadSemantic : defaultPayloadSemantics )
            {
                payloadSemantic = OPTIX_PAYLOAD_SEMANTICS_TRACE_CALLER_READ_WRITE
                                  | OPTIX_PAYLOAD_SEMANTICS_CH_READ_WRITE | OPTIX_PAYLOAD_SEMANTICS_MS_READ_WRITE
                                  | OPTIX_PAYLOAD_SEMANTICS_AH_READ_WRITE | OPTIX_PAYLOAD_SEMANTICS_IS_READ_WRITE;
            }
            for( OptixPayloadType& type : types )
            {
                type.numPayloadValues = static_cast<unsigned int>( defaultPayloadSemantics.size() );
                type.payloadSemantics = defaultPayloadSemantics.data();
            }
            s_pipelineCompileOptions.numPayloadValues = 0;
            s_moduleCompileOptions.numPayloadTypes    = numTypes;
            s_moduleCompileOptions.payloadTypes       = types.data();
        }
        else if( arg == "-ni" || arg == "--num-iters" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            numIters = atoi( argv[++i] );
        }
        else if( arg == "-dt" || arg == "--disable-tasks" )
        {
            useTasks = false;
        }
        else if( arg == "-dc" || arg == "--disable-cache" )
        {
            useCache = false;
        }
        else if( arg == "-nt" || arg == "--num-threads" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            numThreads = atoi( argv[++i] );
        }
        else if( arg == "-mt" || arg == "--max-num-tasks" )
        {
            if( i >= argc-1 )
                printUsageAndExit( argv[0] );
            maxNumTasks = atoi( argv[++i] );
        }
        else if( arg == "-g" )
        {
            s_moduleCompileOptions.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
            s_moduleCompileOptions.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
        }
        else
        {
            if( !filename.empty() )
            {
                std::cerr << "Only one filename supported. Found additional filename: " << arg << ". Already found " << filename << "\n";
                printUsageAndExit( argv[0] );
            }
            filename = arg;
        }
    }

    SetUp();
    if( numIters > 1 || !useCache )
        optixDeviceContextSetCacheEnabled( s_context, 0 );

    std::string input = readInputFile( filename );
    if( useTasks )
    {
        std::cout << "Running with " << numThreads << " threads and " << maxNumTasks << " maximum number of tasks\n";
        g_pool.m_threadPool.startPool( numThreads );
        g_pool.m_maxNumAdditionalTasks = maxNumTasks;
        compileModuleWithTasks( input, numIters );
        g_pool.m_threadPool.terminate();
    }
    else
        compileModule( input, numIters );

    TearDown();

    return 0;
}
