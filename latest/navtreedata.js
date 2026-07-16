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
        [ "Observers", "index.html#observers", [
          [ "Observers for relation pairs", "index.html#observers-for-relation-pairs", null ]
        ] ],
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
        [ "System jobs", "index.html#system-jobs", null ],
        [ "System callbacks and command buffers", "index.html#system-callbacks-and-command-buffers", null ]
      ] ],
      [ "Data layouts", "index.html#data-layouts", null ],
      [ "Serialization", "index.html#serialization", [
        [ "Compile-time serialization", "index.html#compile-time-serialization", null ],
        [ "Runtime serialization", "index.html#runtime-serialization", null ],
        [ "World serialization", "index.html#world-serialization", null ]
      ] ],
      [ "Runtime components", "index.html#runtime-components", [
        [ "Registration", "index.html#registration", null ],
        [ "Field metadata", "index.html#field-metadata", null ],
        [ "Nested structs and fixed arrays", "index.html#nested-structs-and-fixed-arrays", null ],
        [ "Opaque adapters", "index.html#opaque-adapters", null ],
        [ "Dynamic vectors", "index.html#dynamic-vectors", null ],
        [ "Enum and bitmask metadata", "index.html#enum-and-bitmask-metadata", null ],
        [ "Raw access and cursors", "index.html#raw-access-and-cursors", null ],
        [ "Data on runtime relationships", "index.html#data-on-runtime-relationships", null ],
        [ "Querying runtime components", "index.html#querying-runtime-components", null ]
      ] ],
      [ "Multithreading", "index.html#multithreading", [
        [ "Jobs", "index.html#jobs", null ],
        [ "Job dependencies", "index.html#job-dependencies", null ],
        [ "Priorities", "index.html#priorities", null ],
        [ "Job behavior", "index.html#job-behavior", null ],
        [ "Background jobs", "index.html#background-jobs", null ],
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
"classgaia_1_1ecs_1_1ComponentCache.html#a3d335eb23e50fe3c322bf300c6eec3de",
"classgaia_1_1ecs_1_1World.html#a5c1a734a5b62a131335bfc9cb25da752",
"classgaia_1_1ecs_1_1detail_1_1QueryImpl.html#a1e9d7148bed62fd3f2ef3bae43d33d28",
"classgaia_1_1ser_1_1detail_1_1ser__buffer__binary__impl.html#a0d1389b747a6a4183f530e39fc273d09",
"index.html#documentation",
"structgaia_1_1cnt_1_1paged__ilist.html#ae9b31f2956212823e45f92b44a9ae6da",
"structgaia_1_1ecs_1_1ComponentCursor.html#a5e4ddd2d4ced16eca0c89c4a25fbcc49",
"structgaia_1_1ecs_1_1QueryCtx_1_1Data.html#a7a44050adc01a9484c9930620df2ec0e",
"structgaia_1_1ecs_1_1detail_1_1NonFragmentingRelationStore.html#af9c9dbd63feec06e22e97a015d29864b",
"structgaia_1_1mem_1_1MemoryPage.html#a416a68101bcbb9ce631dbe94e338fe40",
"system_8inl_source.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';