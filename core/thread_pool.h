// Copyright 2012-2013 Jeffrey Johnson.
// 
// This file is part of Polymec, and is licensed under the Apache License, 
// Version 2.0 (the "License"); you may not use this file except in 
// compliance with the License. You may may find the text of the license in 
// the LICENSE file at the top-level source directory, or obtain a copy of 
// it at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef POLYMEC_THREAD_POOL_H
#define POLYMEC_THREAD_POOL_H

// This type is intended to provide a pool of threads for performing a 
// given task.
typedef struct thread_pool_t thread_pool_t;

// Creates a thread pool with the given number of threads.
thread_pool_t* thread_pool_new(int num_threads);

// Destroys a thread pool.
void thread_pool_free(thread_pool_t* pool);

// Schedules a task within the thread pool, providing a function to 
// perform the work and a context to be used by the function.
void thread_pool_schedule(thread_pool_t* pool,
                          void* context,
                          void (*do_work)(void*));

// Runs the thread pool, executing its queue of tasks.
void thread_pool_execute(thread_pool_t* pool);

#endif
