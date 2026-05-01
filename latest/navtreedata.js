/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Gaia-ECS", "index.html", [
    [ "Table of Contents", "index.html#table-of-contents", null ],
    [ "Introduction", "index.html#introduction", [
      [ "ECS", "index.html#ecs", null ],
      [ "Implementation", "index.html#implementation", null ],
      [ "Project structure", "index.html#project-structure", null ]
    ] ],
    [ "Usage", "index.html#usage", [
      [ "Minimum requirements", "index.html#minimum-requirements", null ],
      [ "Basic operations", "index.html#basic-operations", [
        [ "Create or delete entity", "index.html#create-or-delete-entity", null ],
        [ "Name entity", "index.html#name-entity", null ],
        [ "Names and lookup", "index.html#names-and-lookup", null ],
        [ "Component scope", "index.html#component-scope", null ],
        [ "Non-fragmenting and sparse components", "index.html#non-fragmenting-and-sparse-components", null ],
        [ "Component presence", "index.html#component-presence", null ],
        [ "Add or remove component", "index.html#add-or-remove-component", null ],
        [ "Component hooks", "index.html#component-hooks", null ],
        [ "Observers", "index.html#observers", null ],
        [ "Bulk editing", "index.html#bulk-editing", null ],
        [ "Set or get component value", "index.html#set-or-get-component-value", null ],
        [ "Copy entity", "index.html#copy-entity", null ],
        [ "Entity cleanup", "index.html#entity-cleanup", null ],
        [ "Batched creation", "index.html#batched-creation", null ],
        [ "Entity lifespan", "index.html#entity-lifespan", [
          [ "SafeEntity", "index.html#safeentity", null ],
          [ "WeakEntity", "index.html#weakentity", null ]
        ] ],
        [ "Archetype lifespan", "index.html#archetype-lifespan", null ]
      ] ],
      [ "Data processing", "index.html#data-processing", [
        [ "Query", "index.html#query", null ],
        [ "Simple query", "index.html#simple-query", null ],
        [ "Query traversal", "index.html#query-traversal", null ],
        [ "Traversal order", "index.html#traversal-order", null ],
        [ "Query variables", "index.html#query-variables", null ],
        [ "Multi-variable queries", "index.html#multi-variable-queries", null ],
        [ "Query low-level API", "index.html#query-low-level-api", null ],
        [ "Query string", "index.html#query-string", null ],
        [ "Uncached query", "index.html#uncached-query", null ],
        [ "Query remarks", "index.html#query-remarks", [
          [ "Query cache behavior", "index.html#query-cache-behavior", null ]
        ] ],
        [ "Iteration", "index.html#iteration", null ],
        [ "Constraints", "index.html#constraints", null ],
        [ "Change detection", "index.html#change-detection", null ],
        [ "Grouping", "index.html#grouping", null ],
        [ "Sorting", "index.html#sorting", null ],
        [ "Parallel execution", "index.html#parallel-execution", null ]
      ] ],
      [ "Relationships", "index.html#relationships", [
        [ "Relationship basics", "index.html#relationship-basics", null ],
        [ "Targets", "index.html#targets", null ],
        [ "Relations", "index.html#relations", null ],
        [ "Entity dependencies", "index.html#entity-dependencies", null ],
        [ "Combination constraints", "index.html#combination-constraints", null ],
        [ "Exclusivity", "index.html#exclusivity", null ],
        [ "Entity inheritance", "index.html#entity-inheritance", null ],
        [ "Prefabs", "index.html#prefabs", null ],
        [ "Cleanup rules", "index.html#cleanup-rules", null ],
        [ "Hierarchies", "index.html#hierarchies", null ]
      ] ],
      [ "Unique components", "index.html#unique-components", null ],
      [ "Delayed execution", "index.html#delayed-execution", [
        [ "Command Merging rules", "index.html#command-merging-rules", [
          [ "Entity Merging", "index.html#entity-merging", null ],
          [ "Component Merging", "index.html#component-merging", null ]
        ] ]
      ] ],
      [ "Systems", "index.html#systems", [
        [ "System basics", "index.html#system-basics", null ],
        [ "System dependencies", "index.html#system-dependencies", null ],
        [ "System jobs", "index.html#system-jobs", null ]
      ] ],
      [ "Data layouts", "index.html#data-layouts", null ],
      [ "Serialization", "index.html#serialization", [
        [ "Compile-time serialization", "index.html#compile-time-serialization", null ],
        [ "Runtime serialization", "index.html#runtime-serialization", null ],
        [ "World serialization", "index.html#world-serialization", null ]
      ] ],
      [ "Multithreading", "index.html#multithreading", [
        [ "Jobs", "index.html#jobs", null ],
        [ "Job dependencies", "index.html#job-dependencies", null ],
        [ "Priorities", "index.html#priorities", null ],
        [ "Job behavior", "index.html#job-behavior", null ],
        [ "Threads", "index.html#threads", null ],
        [ "Scheduler adapters", "index.html#scheduler-adapters", null ]
      ] ],
      [ "Customization", "index.html#customization", [
        [ "Logging", "index.html#logging", null ]
      ] ]
    ] ],
    [ "Requirements", "index.html#requirements", [
      [ "Compiler", "index.html#compiler", null ],
      [ "Dependencies", "index.html#dependencies", null ]
    ] ],
    [ "Installation", "index.html#installation", [
      [ "CMake", "index.html#cmake", [
        [ "Project settings", "index.html#project-settings", null ],
        [ "Sanitizers", "index.html#sanitizers", null ],
        [ "Single-header", "index.html#single-header", null ]
      ] ],
      [ "Conan", "index.html#conan", null ]
    ] ],
    [ "Repository structure", "index.html#repository-structure", [
      [ "Examples", "index.html#examples", null ],
      [ "Benchmarks", "index.html#benchmarks", null ],
      [ "Profiling", "index.html#profiling", null ],
      [ "Testing", "index.html#testing", null ],
      [ "Documentation", "index.html#documentation", null ]
    ] ],
    [ "Future", "index.html#future", null ],
    [ "Contributions", "index.html#contributions", null ],
    [ "License", "index.html#license", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"classgaia_1_1ecs_1_1ComponentCache.html#a43f33f922e3d6344744537c9f54536f8",
"classgaia_1_1ecs_1_1World.html#ac61cf63eefdacc175f9a98cfe83076a4",
"classgaia_1_1mem_1_1StackAllocator.html#aa4352d0f3e00f7dca68800cd0bdc6bb2",
"functions_func_s.html",
"structgaia_1_1cnt_1_1detail_1_1ilist__has__handle__gen_3_01T_00_01std_1_1void__t_3_01decltype_07ac617e2cb9510a57a6690c602a6dceae.html",
"structgaia_1_1ecs_1_1EntityContainer.html#afbcaaf2e19b7fd37ef1075c84b9a21eb",
"structgaia_1_1ecs_1_1detail_1_1QueryImpl_1_1TypedIterErasedCallback.html",
"structgaia_1_1util_1_1str.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';