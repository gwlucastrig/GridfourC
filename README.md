# The Gridfour Software Project Port to C
A C library for raster data including scientific and geophysical applications.

## Welcome
Welcome to the Gridfour software project's port to the C programming language.  We are just getting
started and aspects of this project are still in flux.  Currently, we are focused on
a C-langage implementation of the GVRS API.

## What is GVRS?
The Gridfour Virtual Raster Store (GVRS, pronounced "givers") is a file-backed system
that provides memory-efficient access to large and very large raster (grid) data sets.
GVRS was created with three main purposes in mind:

**1. Authoring Data:** For applications that produce data sets, GVRS provides a high-performance
virtual management system for handling content. Applications may store their final results
in persisent GVRS files, or transcribe them to conventional formats (NetCDF, HDF5, TIFF and GeoTIFF, etc.).

**2. Experiments in Data Compression:**  GVRS provides a convenient testbed for developers
who are exploring new ways of performing data compression for raster data sources.

**3. Distribution:**  GVRS provides a light-weight API and data format suitable for distributing
data to other systems. GVRS was originally conceived as a way of providing environment data for small systems, 
single-board computers, and platforms such as Autonomous Underwater Vehicles. But it is a feasible
solution for many other use cases.

## Project Status
The original Gridfour project was implemented in Java. More information about the original implementation
is available at the [Gridfour Software Project](https://github.com/gwlucastrig/gridfour).

Currently, we have completed a read-only version of GVRS that runs under Windows. We are working
on Linux.  Once that is ready, we will turn our attention to implementing the ability
for the C API to write data to a GVRS data store.

## Documentation

Please see our Wiki page for more information on the C API.

For the Gridfour project in general, we have two main documentation pages:

1. [The Gridfour Project Notes](https://gwlucastrig.github.io/GridfourDocs/notes/index.html) give information on
   the underlying concepts and algorithms used by this project. The Notes page isn't just about Gridfour.
   It covers ideas and topics related to raster data processing in general.

2. [The Gridfour Wiki](https://github.com/gwlucastrig/gridfour/wiki) gives lots of helpful information
   on using Gridfour software including our Gridfour Virtual Raster Store (GVRS). It also gives information
   about our project goals and roadmap.
 
