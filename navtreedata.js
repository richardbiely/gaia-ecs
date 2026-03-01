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
        [ "Add or remove component", "index.html#add-or-remove-component", null ],
        [ "Component presence", "index.html#component-presence", null ],
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
        [ "Query string", "index.html#query-string", null ],
        [ "Uncached query", "index.html#uncached-query", null ],
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
        [ "Cleanup rules", "index.html#cleanup-rules", null ]
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
        [ "Threads", "index.html#threads", null ]
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
      ] ]
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
        [ "Variables", "functions_vars.html", null ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ]
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
"classgaia_1_1ecs_1_1CopyIter.html#a4013b58420c6e66cfffc5cb06821b1a8",
"classgaia_1_1util_1_1delegate_3_01Ret_07Args_8_8_8_08_4.html#a68bfa25c1f04459b3d554ae3af6d9fc7",
"semaphore_8h_source.html",
"structgaia_1_1ecs_1_1OnDeleteTarget__.html",
"structgaia_1_1mt_1_1ThreadCtx.html#ae1b5bf500b0ffd38a6400e64f498bf8f"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';